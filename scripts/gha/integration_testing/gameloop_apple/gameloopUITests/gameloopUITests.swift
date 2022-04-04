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
//  gameloopUITests.swift
//  gameloopUITests
//

import XCTest

/// Constants used across the loop launcher and UI test.
public struct Constants {
  public static let completeText = "Game Loop Complete"
  public static let gameLoopTimeout = "GAME_LOOP_TIMEOUT"
  public static let gameLoopScenario = "GAME_LOOP_SCENARIO"
  public static let gameLoopBundleId = "GAME_LOOP_BUNDLE_ID"

  // Custom URL schemes for starting and ending game loops
  public static let gameLoopScheme = "firebase-game-loop://"
  public static let gameLoopCompleteScheme = "firebase-game-loop-complete://"

  // Directory to look for the output results
  public static let resultsDirectory = "GameLoopResults"
}
/// UI test suite as entry point to launching app under test.
class GameLoopLauncherUITests: XCTestCase {

  /// Launch the game loop through the view controller and wait for its completion.
  func testLaunchGameLoop() {
    // Add the scenario to our launcher app's environment
    let app = XCUIApplication()
    let environment = ProcessInfo.processInfo.environment
    let scenario = environment[Constants.gameLoopScenario] ?? "0"
    let bundleId = environment[Constants.gameLoopBundleId] ?? nil
    app.launchEnvironment[Constants.gameLoopScenario] = scenario

    // Periodically check and dismiss dialogs with "Allow" or "OK"
    Timer.scheduledTimer(withTimeInterval: 5, repeats: true) { (_) in
#if os(iOS)
      let springboard = XCUIApplication(bundleIdentifier: "com.apple.springboard")
      for button in [springboard.buttons["Open"], springboard.buttons["Allow"], springboard.buttons["OK"]] {
        if button.exists {
          NSLog("Dismissing system dialog")
          button.tap()
        }
      }
#elseif os(tvOS)
      NSLog("finding pineboard ...")
      let pineboard = XCUIApplication(bundleIdentifier: "com.apple.PineBoard")
      for button in [pineboard.buttons["Open"], pineboard.buttons["Allow"], pineboard.buttons["OK"]] {
        if button.exists {
          NSLog("Dismissing system dialog")
          let remote: XCUIRemote = XCUIRemote.shared
          remote.press(.select)
        }
      }
#endif
    }

    //Launch Gameloop App
    app.launch()
    //Wait until Gameloop App open Integration Test App
    app.wait(for: .runningBackground, timeout: 20)
    //Wait until Integration Test App closed (testing finished)
    let expectation = XCTestExpectation(description: "Integration Test App closed")
    Timer.scheduledTimer(withTimeInterval: 5, repeats: true) { (_) in
      if app.state == .runningForeground {
        NSLog("Integration Test App closed... Gameloop App back to foreground...")
        expectation.fulfill()
      } else {
        NSLog("Testing... Gameloop App in background...")
      }
    }

    let timeout = getTimeout()
    let wait = XCTWaiter().wait(for: [expectation], timeout: timeout)

    if wait == .completed {
      let result = app.staticTexts[Constants.completeText].waitForExistence(timeout: 10)
      XCTAssert(result)
    } else {
      XCTAssert(false)
    }
  }

  /// Obtain the timeout from the runtime environment.
  func getTimeout() -> TimeInterval {
    if let timeoutString = ProcessInfo.processInfo.environment[Constants.gameLoopTimeout],
      let timeoutSecs = TimeInterval(timeoutString)
    {
      return timeoutSecs
    } else {
      // Default 15 minutes
      return TimeInterval(15 * 60)
    }
  }
}
