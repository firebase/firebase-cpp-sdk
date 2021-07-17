// Copyright 2021 Google LLC

#include <string>
#include <vector>

#include <Foundation/Foundation.h>

namespace firebase {
namespace test_lab {
namespace game_loop {
namespace internal {

std::vector<std::string> GetArguments() {
  NSArray<NSString*>* args = [[NSProcessInfo processInfo] arguments];
  std::vector<std::string> arguments(args.count);
  for (int i = 0; i < args.count; i++) {
    arguments[i] = args[i].UTF8String;
  }
  return arguments;
}

}  // firebase
}  // test_lab
}  // game_loop
}  // internal
