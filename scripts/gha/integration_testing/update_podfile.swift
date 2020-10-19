#!/usr/bin/swift

/*
 * Copyright 2020 Google
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
 *
 * Update integration testapps' Podfiles to match the SDK's Podfile.
 * usage:
 *   ./update_podfile.swift sdkPodfile appPodfile
 */

import Foundation

let sdkPodfile = CommandLine.arguments[1]
let appPodfile = CommandLine.arguments[2]

print(appPodfile)
print(sdkPodfile)
// read contents from sdk's podfile
var sdkPodfileContents = ""
do {
  sdkPodfileContents = try String(contentsOfFile: sdkPodfile, encoding: .utf8)
} catch {
  fatalError("Could not read \(sdkPodfile). \(error)")
}

// dict to store podName & podVersion
let sdkPodfilelines = sdkPodfileContents.components(separatedBy: .newlines)
var dict = [String: String]()
for line in sdkPodfilelines {
  var newLine = line.trimmingCharacters(in: .whitespacesAndNewlines)
  let tokens = newLine.components(separatedBy: [" ", ","] as CharacterSet).filter({ $0 != ""})
  if tokens.first == "pod" {
    let podName = tokens[1]
    let podVersion = tokens[2]
    dict[podName] = podVersion
  }
}


// read contents from app's podfile
var appPodfileContents = ""
do {
  appPodfileContents = try String(contentsOfFile: appPodfile, encoding: .utf8)
} catch {
  fatalError("Could not read \(appPodfile). \(error)")
}

// update podVersion in app's podfile
let appPodfilelines = appPodfileContents.components(separatedBy: .newlines)
var outBuffer = ""
for line in appPodfilelines {
  let tokens = line.components(separatedBy: [" ", ","] as CharacterSet).filter({ $0 != ""})
  if tokens.first == "pod",  let podVersion = dict[tokens[1]]{ // podName = tokens[1]
    outBuffer += "  pod \(tokens[1]), \(podVersion)\n"
  } else {
    outBuffer += line + "\n"
  }
}

// Write out the changed file.
do {
  try outBuffer.write(toFile: appPodfile, atomically: false, encoding: String.Encoding.utf8)
} catch {
  fatalError("Failed to write \(appPodfile). \(error)")
}
