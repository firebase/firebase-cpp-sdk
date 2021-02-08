// Copyright 2019 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "testlab/src/ios/custom_results.h"

#import <Foundation/Foundation.h>

#include <errno.h>
#include <stdio.h>
#include "app/src/assert.h"
#include "app/src/util_ios.h"
#include "testlab/src/common/common.h"
#include "testlab/src/include/firebase/testlab.h"

namespace firebase {
namespace test_lab {
namespace game_loop {
namespace internal {

static const char* kCustomResultDirErrorMsgFmt =
    "Could not create Test Lab's custom results directory. It will not "
    "be possible to log test results for Test Lab game loop scenarios. "
    "NSFileManager:URLForDirectory returned error %s";
static const char* kCustomResultFileErrorMsgFmt =
    "Test Lab could not create the custom result file. It will not be "
    "possible to log test results for Test Lab test cases. "
    "NSFileManager:createFileAtPath returned error code %d: %s";

static NSURL* GetDocumentsUrl(NSFileManager* manager);
static void LogCustomResultsDirError(NSError* error);
static FILE* OpenFileOrLogError(NSFileManager* manager, NSURL* file_url, const char* mode);

void CreateOrOpenLogFile() {
  if (!g_log_file) {
    g_log_file = CreateLogFile();
  }
}

FILE* CreateCustomResultsFile(int scenario) {
  if (g_results_dir != nullptr) {
    return OpenCustomResultsFile(scenario);
  }
  // iOS sandboxes apps' filesystems, so we need to find the documents directory
  // through the NSFileManager
  NSFileManager* manager = [NSFileManager defaultManager];
  NSURL* documents_url = GetDocumentsUrl(manager);
  NSURL* results_dir = [documents_url URLByAppendingPathComponent:@(kResultsDir) isDirectory:YES];
  BOOL results_dir_exists = [manager fileExistsAtPath:results_dir.path];
  if (!results_dir_exists) {
    NSError* error;
    BOOL create_dir_result = [manager createDirectoryAtURL:results_dir
                               withIntermediateDirectories:NO
                                                attributes:nil
                                                     error:&error];
    if (!create_dir_result) {
      LogCustomResultsDirError(error);
      return nullptr;
    }
  }

  // Use the filename that matches the Android results file created by FTL
  NSString* result_filename = [NSString stringWithFormat:@"results_scenario_%d.json", scenario];
  NSURL* result_url = [results_dir URLByAppendingPathComponent:result_filename isDirectory:NO];
  return OpenFileOrLogError(manager, result_url, "w");
}

FILE* CreateLogFile() {
  NSFileManager* manager = [NSFileManager defaultManager];
  NSURL* documents_url = GetDocumentsUrl(manager);
  NSURL* log_url = [documents_url URLByAppendingPathComponent:@"firebase-game-loop.log"
                                                  isDirectory:NO];
  return OpenFileOrLogError(manager, log_url, "w+");
}

static NSURL* GetDocumentsUrl(NSFileManager* manager) {
  NSError* error = nil;
  NSURL* documents_url = [manager URLForDirectory:NSDocumentDirectory
                                         inDomain:NSUserDomainMask
                                appropriateForURL:nil
                                           create:NO
                                            error:&error];
  if (documents_url == nil) {
    LogCustomResultsDirError(error);
    return nullptr;
  }
  return documents_url;
}

static void LogCustomResultsDirError(NSError* error) {
  NSString* errorMessage = [NSString stringWithFormat:@"%@", error];
  LogError(kCustomResultDirErrorMsgFmt, errorMessage.UTF8String);
}

static FILE* OpenFileOrLogError(NSFileManager* manager, NSURL* file_url, const char* mode) {
  BOOL create_file_result = [manager createFileAtPath:file_url.path contents:nil attributes:nil];
  if (!create_file_result) {
    LogError(kCustomResultFileErrorMsgFmt, errno, strerror(errno));
    return nullptr;
  }
  std::string file_path = file_url.path.UTF8String;
  FILE* file = std::fopen(file_path.c_str(), mode);
  if (file == nullptr) {
    LogError(
        "Could not open file %s. Custom results for this scenario may be missing or incomplete",
        file_path.c_str());
  }
  return file;
}

}  // namespace internal
}  // namespace game_loop
}  // namespace test_lab
}  // namespace firebase
