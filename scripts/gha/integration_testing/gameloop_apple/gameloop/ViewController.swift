//
// Copyright (c) 2021 Google Inc.
//
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at
//
//  http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.
//
//  ViewController.swift
//  gameloop
//

import CoreFoundation
import UIKit

/// Minimal view controller to launch the game loop.
class ViewController: UIViewController {
  @IBOutlet var testingLabel: UILabel!
  let scenarioFromEnv = Int(ProcessInfo.processInfo.environment[Constants.gameLoopScenario] ?? "")
  let scenarioFromArg = Int(ProcessInfo.processInfo.arguments.last ?? "")
  var scenario: Int {
    scenarioFromArg ?? scenarioFromEnv ?? 1
  }

  /// Run the game loop as soon as we load.
  override func viewDidLoad() {
    super.viewDidLoad()
    NSLog("Starting game loop with scenario \(scenario)")
    launchGame()
  }

  /// Launch the app under test by calling our custom URL scheme.
  func launchGame() {
    let url = getURL()
    UIApplication.shared.open(url) { success in
      if !success {
        NSLog("Error launching game loop; skipping scenario \(self.scenario)")
        self.markComplete()
      }
    }
  }

  /// Derive the URL from our scheme and the scenario environment variable.
  func getURL() -> URL {
    let urlString = "\(Constants.gameLoopScheme)?scenario=\(scenario)"
    return URL(string: urlString)!
  }

  /// Signal to the UI test the game loop is done by changing the label.
  func markComplete() {
    NSLog("Completing game loop \(scenario)")
    let center = CFNotificationCenterGetDarwinNotifyCenter()
    let name = "com.google.game-loop" as CFString
    CFNotificationCenterPostNotification(center, CFNotificationName(name), nil, nil, true)
    testingLabel.text = Constants.completeText
    writeResultSentinel()
  }

  func writeResultSentinel() {
    let text = "Scenario \(scenario) complete"
    let fileName = "scenario.\(scenario)"
    let fileManager = FileManager.default
    do {
      let docs = try fileManager.url(
        for: .documentDirectory, in: .userDomainMask, appropriateFor: nil, create: true)
      let fileURL = docs.appendingPathComponent(fileName)
      NSLog("Writing to \(fileURL)")
      try text.write(to: fileURL, atomically: false, encoding: .utf8)
      NSLog("Sentinel file for scenario \(scenario) successfully written")
    } catch {
      NSLog("ERROR: There was an error writing to the file: \(error)")
    }
  }
}

