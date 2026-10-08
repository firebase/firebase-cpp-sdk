/*
 * Copyright 2025 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// Multi-turn C++ Hybrid Chat sample for Firebase AI Logic + Google AI Edge
// LiteRT / LiteRT-LM (Gemma).
//
// Features:
// - Live toggle (`/toggle`, `/mode local`, `/mode cloud`, `/mode hybrid`)
//   between Cloud Firebase AI (`gemini-2.5-flash`) and Local On-Device LiteRT
//   inference (`.litertlm` Gemma or `.tflite` CompiledModel) while sharing a
//   single multi-turn `firebase::ai::Chat` history.
// - Real-time token streaming for both Cloud SSE and Local LiteRT-LM.

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "firebase/ai.h"
#include "firebase/app.h"

namespace {

using ::firebase::App;
using ::firebase::AppOptions;
using ::firebase::Future;
using ::firebase::kFutureStatusComplete;
using ::firebase::ai::Backend;
using ::firebase::ai::Chat;
using ::firebase::ai::CountTokensResponse;
using ::firebase::ai::FirebaseAI;
using ::firebase::ai::GenerateContentResponse;
using ::firebase::ai::GenerationConfig;
using ::firebase::ai::GenerativeModel;
using ::firebase::ai::HybridParams;
using ::firebase::ai::InferenceMode;
using ::firebase::ai::InferenceSource;
using ::firebase::ai::kErrorNone;
using ::firebase::ai::kInferenceModeOnlyInCloud;
using ::firebase::ai::kInferenceModeOnlyOnDevice;
using ::firebase::ai::kInferenceModePreferInCloud;
using ::firebase::ai::kInferenceModePreferOnDevice;
using ::firebase::ai::kInferenceSourceOnDevice;
using ::firebase::ai::kLiteRtAcceleratorCpu;
using ::firebase::ai::kLiteRtAcceleratorGpu;
using ::firebase::ai::LiteRtAccelerator;
using ::firebase::ai::ModelContent;
using ::firebase::ai::OnDeviceParams;

bool FileExists(const std::string& path) {
  if (path.empty()) return false;
  std::ifstream ifs(path.c_str(), std::ios::binary);
  return ifs.good();
}

std::string ParentDir(const std::string& path) {
  size_t pos = path.find_last_of("/\\");
  if (pos == std::string::npos) return ".";
  return path.substr(0, pos);
}

std::string FindDefaultLocalModel(const char* argv0) {
  const char* env_model = std::getenv("FIREBASE_LITERT_MODEL");
  if (env_model && env_model[0] != '\0') {
    return env_model;
  }
  std::string bin_dir = argv0 ? ParentDir(argv0) : ".";
  std::vector<std::string> candidates = {
      bin_dir + "/gemma3-1b-it-int4.litertlm",
      bin_dir + "/gemma3-270m.litertlm",
      "./gemma3-1b-it-int4.litertlm",
      "./gemma3-270m.litertlm",
      "./desktop_build/ai/gemma3-1b-it-int4.litertlm",
      "./desktop_build/ai/gemma3-270m.litertlm",
      "./firebase-cpp-sdk/desktop_build/ai/gemma3-1b-it-int4.litertlm",
      "./firebase-cpp-sdk/desktop_build/ai/gemma3-270m.litertlm",
  };
  for (const auto& candidate : candidates) {
    if (FileExists(candidate)) {
      return candidate;
    }
  }
  return "simulated://gemma-3-270m-it";
}

std::string ReadFileToString(const std::string& path) {
  std::ifstream ifs(path.c_str(), std::ios::binary);
  if (!ifs.good()) return "";
  return std::string((std::istreambuf_iterator<char>(ifs)),
                     std::istreambuf_iterator<char>());
}

std::string FindFirebaseConfigFile(const char* argv0,
                                   const std::string& explicit_config_path) {
  if (!explicit_config_path.empty() && FileExists(explicit_config_path)) {
    return explicit_config_path;
  }
  std::string bin_dir = argv0 ? ParentDir(argv0) : ".";
  std::vector<std::string> search_dirs = {
      ".",
      bin_dir,
      "./ai",
      "./ai/samples",
      "./desktop_build/ai",
      "./firebase-cpp-sdk/ai",
      "./firebase-cpp-sdk/ai/samples",
      "./firebase-cpp-sdk/desktop_build/ai",
  };
  const char* config_names[] = {"google-services-desktop.json",
                                "google-services.json"};
  for (const auto& dir : search_dirs) {
    for (const char* name : config_names) {
      std::string candidate = (dir == ".") ? name : (dir + "/" + name);
      if (FileExists(candidate)) {
        return candidate;
      }
    }
  }
  return "";
}

const char* ModeToString(InferenceMode mode) {
  switch (mode) {
    case kInferenceModeOnlyOnDevice:
      return "LOCAL ONLY (LiteRT On-Device)";
    case kInferenceModeOnlyInCloud:
      return "CLOUD ONLY (Firebase AI Gemini)";
    case kInferenceModePreferOnDevice:
      return "HYBRID: PREFER_ON_DEVICE (LiteRT -> Cloud fallback)";
    case kInferenceModePreferInCloud:
      return "HYBRID: PREFER_IN_CLOUD (Cloud -> LiteRT fallback)";
  }
  return "UNKNOWN";
}

const char* SourceBadge(InferenceSource source) {
  return source == kInferenceSourceOnDevice ? "[ON_DEVICE LiteRT]"
                                            : "[IN_CLOUD Firebase]";
}

template <typename T>
void WaitForFuture(const Future<T>& fut) {
  while (fut.status() != kFutureStatusComplete) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
}

void PrintBanner(const App& app, const Chat& chat,
                 const std::string& config_source,
                 const std::string& cloud_model,
                 const std::string& local_model_path) {
  std::cout << "\n============================================================="
               "=======\n";
  std::cout << "  Firebase AI Logic C++ Hybrid Multi-Turn Chat (Cloud + LiteRT "
               "On-Device)\n";
  std::cout << "==============================================================="
               "=====\n";
  std::cout << "  Firebase Cfg: " << config_source
            << " (project_id=" << app.options().project_id() << ")\n";
  std::cout << "  Cloud Model : " << cloud_model << "\n";
  std::cout << "  Local Model : " << local_model_path << " ("
            << (chat.IsOnDeviceAvailable() ? "AVAILABLE" : "UNAVAILABLE")
            << ")\n";
  std::cout << "  Active Mode : [Toggle: "
            << ModeToString(chat.inference_mode()) << "]\n";
  std::cout << "---------------------------------------------------------------"
               "-----\n";
  std::cout << "  Commands:\n";
  std::cout << "    /toggle        Cycle between LOCAL -> CLOUD -> HYBRID "
               "(Prefer Local) -> HYBRID (Prefer Cloud)\n";
  std::cout << "    /mode local    Switch to ONLY_ON_DEVICE (LiteRT)\n";
  std::cout << "    /mode cloud    Switch to ONLY_IN_CLOUD (Firebase AI)\n";
  std::cout << "    /mode hybrid   Switch to PREFER_ON_DEVICE\n";
  std::cout << "    /mode fallback Switch to PREFER_IN_CLOUD (offline fallback "
               "to LiteRT)\n";
  std::cout << "    /history       Print accumulated multi-turn Chat history\n";
  std::cout << "    /compact       Summarize & compact Chat history using "
               "active model\n";
  std::cout << "    /clear         Clear multi-turn Chat history\n";
  std::cout << "    /quit          Exit\n";
  std::cout << "==============================================================="
               "=====\n\n";
}

void PrintHistory(const Chat& chat) {
  std::vector<ModelContent> hist = chat.history();
  std::cout << "\n--- Chat History (" << hist.size() << " turns) ---\n";
  for (size_t i = 0; i < hist.size(); ++i) {
    std::cout << "  [" << i + 1 << "] " << hist[i].role() << ": ";
    for (const auto& part : hist[i].parts()) {
      if (part.is_text()) {
        std::cout << part.text_part().text;
      }
    }
    std::cout << "\n";
  }
  std::cout << "-----------------------------------\n\n";
}

void SendTurn(Chat* chat, const std::string& user_input, bool use_streaming) {
  if (use_streaming) {
    bool printed_prefix = false;
    Future<void> stream_fut = chat->SendMessageStream(
        user_input, [&printed_prefix](const GenerateContentResponse& chunk) {
          if (!printed_prefix) {
            std::cout << SourceBadge(chunk.inference_source()) << " Model: ";
            printed_prefix = true;
          }
          std::cout << chunk.text() << std::flush;
        });
    WaitForFuture(stream_fut);
    if (stream_fut.error() != kErrorNone) {
      std::cout << "\n[Error " << stream_fut.error() << "] "
                << (stream_fut.error_message() ? stream_fut.error_message()
                                               : "")
                << "\n\n";
    } else {
      std::cout << "\n\n";
    }
    return;
  }

  Future<GenerateContentResponse> fut = chat->SendMessage(user_input);
  WaitForFuture(fut);
  if (fut.error() != kErrorNone || fut.result() == nullptr) {
    std::cout << "[Error " << fut.error() << "] "
              << (fut.error_message() ? fut.error_message() : "") << "\n\n";
    return;
  }
  const GenerateContentResponse& resp = *fut.result();
  std::cout << SourceBadge(resp.inference_source()) << " Model: " << resp.text()
            << "\n\n";
}

}  // namespace

int main(int argc, char** argv) {
  std::string local_model_path = FindDefaultLocalModel(argc > 0 ? argv[0] : "");
  std::string litert_lib_path =
      std::getenv("FIREBASE_LITERT_LM_LIB_PATH")
          ? std::getenv("FIREBASE_LITERT_LM_LIB_PATH")
          : (std::getenv("FIREBASE_LITERT_LIB_PATH")
                 ? std::getenv("FIREBASE_LITERT_LIB_PATH")
                 : "");
  std::string config_path =
      std::getenv("FIREBASE_CONFIG") ? std::getenv("FIREBASE_CONFIG") : "";
  std::string cloud_model = "gemini-3.1-flash-lite";
  InferenceMode initial_mode = kInferenceModePreferOnDevice;
  LiteRtAccelerator accelerator = kLiteRtAcceleratorCpu;
  bool use_streaming = true;
  bool run_demo = false;

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--model" && i + 1 < argc) {
      local_model_path = argv[++i];
    } else if (arg == "--litert-lib" && i + 1 < argc) {
      litert_lib_path = argv[++i];
    } else if (arg == "--config" && i + 1 < argc) {
      config_path = argv[++i];
    } else if (arg == "--cloud-model" && i + 1 < argc) {
      cloud_model = argv[++i];
    } else if (arg == "--gpu") {
      accelerator = kLiteRtAcceleratorGpu;
    } else if (arg == "--no-stream") {
      use_streaming = false;
    } else if (arg == "--demo") {
      run_demo = true;
    } else if (arg == "--mode" && i + 1 < argc) {
      std::string m = argv[++i];
      if (m == "local")
        initial_mode = kInferenceModeOnlyOnDevice;
      else if (m == "cloud")
        initial_mode = kInferenceModeOnlyInCloud;
      else if (m == "hybrid")
        initial_mode = kInferenceModePreferOnDevice;
      else if (m == "fallback")
        initial_mode = kInferenceModePreferInCloud;
    }
  }

  // Standard Firebase C++ SDK initialization (`google-services-desktop.json` /
  // `google-services.json` via `App::Create()` or
  // `AppOptions::LoadFromJsonConfig()`).
  std::string resolved_config_file =
      FindFirebaseConfigFile(argc > 0 ? argv[0] : "", config_path);
  App* app = nullptr;
  if (resolved_config_file == "google-services.json" ||
      resolved_config_file == "google-services-desktop.json") {
    app = App::Create();
  } else if (!resolved_config_file.empty()) {
    AppOptions options;
    std::string json_str = ReadFileToString(resolved_config_file);
    if (AppOptions::LoadFromJsonConfig(json_str.c_str(), &options)) {
      app = App::Create(options);
    }
  } else {
    app = App::Create();
  }
  if (!app) {
    std::cerr << "Failed to initialize firebase::App from google-services.json "
                 "/ google-services-desktop.json.\n";
    return 1;
  }
  FirebaseAI* ai = FirebaseAI::GetInstance(app, Backend::GoogleAI());

  OnDeviceParams on_device(local_model_path, accelerator);
  on_device.runtime_library_path = litert_lib_path;
  // Leave max_num_tokens = 0 so LiteRtAdapter auto-detects the model's full
  // context window from the .litertlm metadata (e.g. 4096 for Gemma 3 1B).
  on_device.max_num_tokens = 0;
  HybridParams hybrid_params(initial_mode, on_device);

  GenerationConfig gen_config;
  gen_config.temperature = 0.7f;
  gen_config.max_output_tokens = 2048;

  GenerativeModel model = ai->GetGenerativeModel(
      cloud_model, hybrid_params, gen_config,
      ModelContent::System("You are a concise, helpful assistant."));

  Chat chat = model.StartChat();
  PrintBanner(*app, chat,
              resolved_config_file.empty() ? "google-services.json"
                                           : resolved_config_file,
              cloud_model, local_model_path);

  if (run_demo) {
    std::cout << ">>> [Demo Step 1] Mode = ONLY_ON_DEVICE (Local LiteRT-LM "
                 "Gemma .litertlm neural network)\n";
    chat.set_inference_mode(kInferenceModeOnlyOnDevice);
    const char* prompt1 =
        "My secret code word is ORBIT-7. Repeat the secret code word and tell "
        "me what 12 * 12 is.";
    std::cout << "You: " << prompt1 << "\n";
    SendTurn(&chat, prompt1, use_streaming);

    std::cout << ">>> [Demo Step 2] Toggling Mode -> ONLY_IN_CLOUD (Live "
                 "Firebase AI Cloud Gemini via google-services.json)\n";
    chat.set_inference_mode(kInferenceModeOnlyInCloud);
    const char* prompt2 =
        "Repeat the secret code word from my previous message, and explain in "
        "one short sentence why hybrid on-device + cloud AI is useful.";
    std::cout << "You: " << prompt2 << "\n";
    SendTurn(&chat, prompt2, use_streaming);

    PrintHistory(chat);
    delete ai;
    delete app;
    return 0;
  }

  std::string line;
  while (true) {
    std::cout << "[" << ModeToString(chat.inference_mode())
              << "]\nYou: " << std::flush;
    if (!std::getline(std::cin, line)) break;
    if (line.empty()) continue;

    if (line == "/quit" || line == "/exit") {
      break;
    } else if (line == "/toggle") {
      InferenceMode current = chat.inference_mode();
      InferenceMode next = kInferenceModeOnlyOnDevice;
      if (current == kInferenceModeOnlyOnDevice) {
        next = kInferenceModeOnlyInCloud;
      } else if (current == kInferenceModeOnlyInCloud) {
        next = kInferenceModePreferOnDevice;
      } else if (current == kInferenceModePreferOnDevice) {
        next = kInferenceModePreferInCloud;
      } else {
        next = kInferenceModeOnlyOnDevice;
      }
      chat.set_inference_mode(next);
      std::cout << "--> Toggled mode to: " << ModeToString(next) << "\n\n";
      continue;
    } else if (line == "/mode local") {
      chat.set_inference_mode(kInferenceModeOnlyOnDevice);
      std::cout << "--> Switched to: " << ModeToString(chat.inference_mode())
                << "\n\n";
      continue;
    } else if (line == "/mode cloud") {
      chat.set_inference_mode(kInferenceModeOnlyInCloud);
      std::cout << "--> Switched to: " << ModeToString(chat.inference_mode())
                << "\n\n";
      continue;
    } else if (line == "/mode hybrid") {
      chat.set_inference_mode(kInferenceModePreferOnDevice);
      std::cout << "--> Switched to: " << ModeToString(chat.inference_mode())
                << "\n\n";
      continue;
    } else if (line == "/mode fallback") {
      chat.set_inference_mode(kInferenceModePreferInCloud);
      std::cout << "--> Switched to: " << ModeToString(chat.inference_mode())
                << "\n\n";
      continue;
    } else if (line == "/history") {
      PrintHistory(chat);
      continue;
    } else if (line == "/clear") {
      chat.ClearHistory();
      std::cout << "--> Cleared Chat history.\n\n";
      continue;
    } else if (line == "/compact") {
      size_t before_turns = chat.history().size();
      std::cout << "--> Compacting " << before_turns
                << " history turns using active model...\n";
      Future<GenerateContentResponse> fut = chat.CompactHistory();
      WaitForFuture(fut);
      if (fut.error() != kErrorNone || fut.result() == nullptr) {
        std::cout << "[Error " << fut.error() << "] "
                  << (fut.error_message() ? fut.error_message() : "") << "\n\n";
      } else {
        std::cout << "--> Compacted " << before_turns << " turns into "
                  << chat.history().size() << " turns:\n"
                  << fut.result()->text() << "\n\n";
      }
      continue;
    }

    SendTurn(&chat, line, use_streaming);
  }

  delete ai;
  delete app;
  return 0;
}
