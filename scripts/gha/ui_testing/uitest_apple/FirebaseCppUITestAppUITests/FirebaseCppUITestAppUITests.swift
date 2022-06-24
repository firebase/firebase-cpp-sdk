//
// Copyright (c) 2022 Google Inc.
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
//  FirebaseCppUITestAppUITests.swift
//  FirebaseCppUITestAppUITests
//

import XCTest

class FirebaseCppUITestAppUITests: XCTestCase {

  func testGMA() {
    // Launch this Helper App
    let helperApp = XCUIApplication()

    // Periodically check and dismiss dialogs with "Allow" or "OK"
    Timer.scheduledTimer(withTimeInterval: 5, repeats: true) { (_) in
#if os(iOS)
      NSLog("finding springboard ...")
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

    // Launch UI Test App
    helperApp.launch()
    // Wait until UI Test App open Integration Test App
    helperApp.wait(for: .runningBackground, timeout: 20)

    // Wait until Integration Test App closed (testing finished)
    let expectation = XCTestExpectation(description: "Integration Test App closed")
    Timer.scheduledTimer(withTimeInterval: 5, repeats: true) { (_) in
      if helperApp.state == .runningForeground {
        NSLog("Integration Test App closed... UI Test App back to foreground...")
        expectation.fulfill()
      } else {
        NSLog("Testing... UI Test App in background...")
      }
    }


    // Start Automated UI Test
    let app = XCUIApplication(bundleIdentifier: "com.google.ios.admob.testapp")

    // 1. TestAdViewAdOpenedAdClosed
    var reference = app.staticTexts["Test mode"]
    XCTAssertTrue(reference.waitForExistence(timeout: 60))
    // Click on the center point of the "Test Ad" TextView, where the Ad present
    var x = (reference.frame.origin.x + reference.frame.width)/2
    var y = (reference.frame.origin.y + reference.frame.height)/2
    sleep(5)  // Wait until button hittable
    let ad_view = app.findElement(at: CGPoint(x: x, y: y))
    ad_view.tap()
    sleep(5)
    app.activate()

    sleep(5)

    // 2. TestInterstitialAdLoadAndShow
    reference = app.staticTexts["Test mode"]
    XCTAssertTrue(reference.waitForExistence(timeout: 60))
    // Click the center point of the device, where the Ad present
    x = app.frame.width/2
    y = app.frame.height/2
    sleep(5)  // Wait until button hittable
    let interstitial_ad = app.findElement(at: CGPoint(x: x, y: y))
    interstitial_ad.tap()
    sleep(5)
    app.activate()
    sleep(5)
    // Click the top left corner close bottom.
    // Use "Test Ad" TextView bottom position as the reference
    x = (reference.frame.origin.y + reference.frame.height)/2
    y = (reference.frame.origin.y + reference.frame.height)/2
    sleep(5)  // Wait until button hittable
    let interstitial_ad_close_button = app.findElement(at: CGPoint(x: x, y: y))
    interstitial_ad_close_button.tap()

    sleep(5)

    // 3. TestRewardedAdLoadAndShow
    reference = app.staticTexts["Test mode"]
    XCTAssertTrue(reference.waitForExistence(timeout: 60))
    // Click the top right corner close bottom.
    x = app.frame.width - (reference.frame.origin.y + reference.frame.height)/2
    y = (reference.frame.origin.y + reference.frame.height)/2
    sleep(15)  // Wait until button hittable
    let rewarded_ad_close_button = app.findElement(at: CGPoint(x: x, y: y))
    rewarded_ad_close_button.tap()

    let close_video_button = app.webViews.staticTexts["CLOSE VIDEO"]
    if close_video_button.waitForExistence(timeout: 5) {
      sleep(5)
      close_video_button.tap()
      sleep(5)
      rewarded_ad_close_button.tap()
    }

    // Finish GMA Tests
    sleep(60)
    reference = app.staticTexts["Test mode"]
    XCTAssertFalse(reference.exists)
    app.terminate()
  }

}

extension XCUIApplication {
  func findElement(at point: CGPoint) -> XCUICoordinate {
    let normalized = coordinate(withNormalizedOffset: .zero)
    let offset = CGVector(dx: point.x, dy: point.y)
    let coordinate = normalized.withOffset(offset)
    return coordinate
  }
}
