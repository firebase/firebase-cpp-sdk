# Firebase AI Logic C++ SDK (`firebase::ai`) — Cloud & LiteRT On-Device Hybrid

This directory implements the `firebase::ai` C++ SDK for **Firebase AI Logic** (Gemini Developer API and Vertex AI Gemini API), including **Hybrid On-Device + Cloud Inference** powered by [Google AI Edge LiteRT](https://developers.google.com/edge/litert/overview#c++_1) and [LiteRT-LM](https://github.com/google-ai-edge/LiteRT-LM).

It also builds `firebase_ai_litert_bridge`, a C ABI shared library used by the **Firebase Unity SDK** (`Firebase.AI`) via P/Invoke so Unity apps can run on-device and hybrid inference through the C++ LiteRT engine while keeping their existing C# Cloud implementation.

---

## Quick Start (Build & Run the Hybrid Multi-Turn Chat Demo)

### 1. Configure & Build via CMake

By default (`FIREBASE_AI_DOWNLOAD_LITERT=ON`), CMake automatically downloads the LiteRT C++ SDK headers (`v2.2.0`), `libLiteRt`, and the `CLiteRTLM` (`v0.18.0`) runtime library during configuration and copies the runtime next to the built binary. Model weights (`.litertlm`) are **not** downloaded automatically.

From the repository root (`firebase-cpp-sdk`):

```bash
cmake -S . -B desktop_build \
  -DFIREBASE_INCLUDE_AI=ON \
  -DFIREBASE_AI_BUILD_SAMPLES=ON \
  -DFIREBASE_AI_BUILD_UNITY_BRIDGE=ON \
  -DFIREBASE_CPP_BUILD_TESTS=ON

cmake --build desktop_build \
  --target firebase_ai_hybrid_chat firebase_ai_litert_bridge firebase_ai_test -j8
```

### 2. Download a Local Gemma `.litertlm` Model

Download a LiteRT-LM `.litertlm` model (for example, **Gemma 3 1B IT INT4** with a 4,096-token context window) and place it in `desktop_build/ai/` or pass its path via `--model`:

```bash
curl -L "https://huggingface.co/litert-community/Gemma3-1B-IT/resolve/main/gemma3-1b-it-int4.litertlm" \
  -o desktop_build/ai/gemma3-1b-it-int4.litertlm
```

### 3. Provide Your Firebase Configuration (`google-services.json`)

The demo initializes `firebase::App` using a standard Firebase `google-services.json` (or `google-services-desktop.json`) file. Place `google-services.json` in your working directory (or `desktop_build/ai/`), or pass `--config /path/to/google-services.json`.

### 4. Run the Interactive Multi-Turn Hybrid Chat App

```bash
./desktop_build/ai/firebase_ai_hybrid_chat \
  --config /path/to/google-services.json \
  --model ./desktop_build/ai/gemma3-1b-it-int4.litertlm
```

*(If `google-services.json` and `gemma3-1b-it-int4.litertlm` are placed in `desktop_build/ai/` or the current directory, they are auto-detected and you can run `./desktop_build/ai/firebase_ai_hybrid_chat` with no arguments.)*

---

## Interactive Chat Commands

Inside `firebase_ai_hybrid_chat`, both Cloud (`gemini-3.1-flash-lite`) and On-Device (`LiteRT-LM`) share a single multi-turn `firebase::ai::Chat` history, so you can switch backends mid-conversation without losing context:

| Command | Description |
| :--- | :--- |
| `/toggle` | Cycle between `ONLY_ON_DEVICE` -> `ONLY_IN_CLOUD` -> `PREFER_ON_DEVICE` -> `PREFER_IN_CLOUD` |
| `/mode local` | Force local on-device inference (`kInferenceModeOnlyOnDevice`) |
| `/mode cloud` | Force cloud Firebase AI inference (`kInferenceModeOnlyInCloud`) |
| `/mode hybrid` | Prefer on-device LiteRT; automatically fall back to Cloud on error (`kInferenceModePreferOnDevice`) |
| `/mode fallback` | Prefer Cloud; automatically fall back to on-device LiteRT when offline (`kInferenceModePreferInCloud`) |
| `/history` | Print the accumulated multi-turn `Chat` history |
| `/compact` | Summarize and compact the conversation history into a concise 2-turn context using the active model |
| `/clear` | Clear the multi-turn `Chat` history |
| `/quit` | Exit the application |

### CLI Flags

- `--model <path>`: Path to a `.litertlm` (LiteRT-LM LLM) or `.tflite` (LiteRT `CompiledModel`) file.
- `--config <path>`: Path to `google-services.json` or `google-services-desktop.json`.
- `--cloud-model <name>`: Cloud Gemini model name (default: `gemini-3.1-flash-lite`).
- `--mode <local|cloud|hybrid|fallback>`: Initial inference mode (default: `hybrid` / `PREFER_ON_DEVICE`).
- `--gpu`: Use GPU acceleration (`kLiteRtAcceleratorGpu`) instead of CPU.
- `--no-stream`: Use non-streaming `Chat::SendMessage` instead of `Chat::SendMessageStream`.
- `--demo`: Run a non-interactive 2-turn verification (Turn 1 on-device -> toggle -> Turn 2 in cloud).

---

## Context Window & Automatic Compaction

- **Auto-Detected Context Length:** When `OnDeviceParams::max_num_tokens` is `0` (the default), `LiteRtAdapter` queries `litert_lm_loaded_file_max_context_tokens` from the `.litertlm` file metadata (`4096` tokens for `gemma3-1b-it-int4.litertlm`, `1024` tokens for `gemma3-270m.litertlm`).
- **Automatic Context Compaction:** Before each on-device turn, `LiteRtAdapter` tokenizes the conversation history via `litert_lm_engine_tokenize`. If the accumulated history exceeds the input token budget, older turns are automatically compacted into `[Compacted Earlier Conversation History]` while keeping recent turns verbatim and reserving headroom for generation output.
- **Repetition Prevention:** On-device generation configures `LiteRtLmRepetitionPenaltyConfig` (`repetition_penalty = 1.15`, `frequency_penalty = 0.25`, `presence_penalty = 0.1`) and `LiteRtLmNoRepeatNgramConfig` (`no_repeat_ngram_size = 4`) so small quantized models do not fall into token repetition loops on long outputs.

---

## Running Unit Tests

```bash
./desktop_build/ai/tests/firebase_ai_test
```
