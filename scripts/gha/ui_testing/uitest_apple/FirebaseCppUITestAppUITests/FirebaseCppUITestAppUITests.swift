//
//  FirebaseCppUITestAppUITests.swift
//  FirebaseCppUITestAppUITests
//
//  Created by Mou Sun on 3/19/22.
//  Copyright © 2022 Google. All rights reserved.
//

import XCTest

class FirebaseCppUITestAppUITests: XCTestCase {

  override func setUp() {
    // Put setup code here. This method is called before the invocation of each test method in the class.

    // In UI tests it is usually best to stop immediately when a failure occurs.
    continueAfterFailure = false

    // UI tests must launch the application that they test. Doing this in setup will make sure it happens for each test method.
    XCUIApplication().launch()

    // In UI tests it’s important to set the initial state - such as interface orientation - required for your tests before they run. The setUp method is a good place to do this.
  }

  override func tearDown() {
    // Put teardown code here. This method is called after the invocation of each test method in the class.
  }


  func testGMA() {
    let app = XCUIApplication(bundleIdentifier: "com.google.ios.admob.testapp")
    app.launch()

    // TestAdViewAdClick
    var reference = app.staticTexts["Test mode"]
    XCTAssertTrue(reference.waitForExistence(timeout: 60))
    var x = (reference.frame.origin.x + reference.frame.width)/2
    var y = (reference.frame.origin.y + reference.frame.height)/2
    sleep(5)  // wait until button hittable
    let ad_view = app.findElement(at: CGPoint(x: x, y: y))
    ad_view.tap()
    sleep(5)
    app.activate()

    sleep(5)

    // TestInterstitialAdClose
    reference = app.staticTexts["Test mode"]
    XCTAssertTrue(reference.waitForExistence(timeout: 60))
    x = (reference.frame.origin.y + reference.frame.height)/2
    y = (reference.frame.origin.y + reference.frame.height)/2
    sleep(5)  // wait until button hittable
    let interstitial_ad_close_button = app.findElement(at: CGPoint(x: x, y: y))
    interstitial_ad_close_button.tap()

    sleep(5)

    // TestInterstitialAdClickAndClose
    reference = app.staticTexts["Test mode"]
    XCTAssertTrue(reference.waitForExistence(timeout: 60))
    x = app.frame.width/2
    y = app.frame.height/2
    sleep(5)  // wait until button hittable
    let interstitial_ad = app.findElement(at: CGPoint(x: x, y: y))
    interstitial_ad.tap()
    sleep(5)
    app.activate()
    sleep(5)
    interstitial_ad_close_button.tap()

    sleep(5)

    // TestRewardedAdClose
    reference = app.webViews.staticTexts.containing(NSPredicate(format: "label CONTAINS 'seconds'")).element
    XCTAssertTrue(reference.waitForExistence(timeout: 60))
    x = (reference.frame.origin.x + reference.frame.width + app.frame.width)/2
    y = (reference.frame.origin.y + reference.frame.height)/2
    sleep(5)  // wait until button hittable
    let rewarded_ad_close_button = app.findElement(at: CGPoint(x: x, y: y))
    rewarded_ad_close_button.tap()

    let close_video_button = app.webViews.staticTexts["CLOSE VIDEO"]
    if close_video_button.waitForExistence(timeout: 5) {
      sleep(5)
      close_video_button.tap()
      sleep(5)
      rewarded_ad_close_button.tap()
    }

    // Finish GMA UI Tests
    sleep(10)
    reference = app.staticTexts["Test mode"]
    XCTAssertFalse(reference.exists)
    app.terminate()
  }


//  func testMessaging() {
//    //our test app
//    let app = XCUIApplication(bundleIdentifier: "com.google.FirebaseCppMessagingTestApp.dev")
//
//    //Ask for permission
//    let springboard = XCUIApplication(bundleIdentifier: "com.apple.springboard")
//    let allow_button = springboard.buttons["Allow"]
//    if allow_button.waitForExistence(timeout: 10) {
//      allow_button.tap()
//    }
//
//    //Copy URL
//    let Paste_URL_OK_button = app.alerts["Paste URL"].buttons["OK"]
//    XCTAssertTrue(Paste_URL_OK_button.waitForExistence(timeout: 10))
//    sleep(1)  // wait until button hittable
//    Paste_URL_OK_button.tap()
//
//    //Paste URL in safari app
//    let safari = XCUIApplication(bundleIdentifier: "com.apple.mobilesafari")
//    XCTAssertTrue(safari.wait(for: .runningForeground, timeout: 20))
//    sleep(1)  // wait until button hittable
//    safari.buttons["URL"].tap()
//    sleep(1)  // wait until button hittable
//    safari.textFields["URL"].tap()
//    sleep(1)  // wait until button hittable
//    safari.menuItems["Paste and Go"].tap()
//    sleep(1)  // wait until button hittable
//
//    //      // wait and tap the notification
//    //      let notification = springboard.otherElements[""]
//    //      XCTAssertNotNil(notification)
//    //      notification.tap()
//
//    app.activate()
//    //      let result = app.textViews.element(matching: NSPredicate(format: "label CONTAINS 'PASSED'"))
//    //      XCTAssertTrue(result.waitForExistence(timeout: 60))
//    sleep(60)
//    app.terminate()
//  }
//
//
//  func testAuth() {
//    let app = XCUIApplication(bundleIdentifier: "com.google.FirebaseCppAuthTestApp.dev")
//
//    //TestSuccessfulSignInFederatedProviderNoScopes
//    let id = app.webViews.textFields["Email or phone"]
//    XCTAssertTrue(id.waitForExistence(timeout: 3 * 60))
//    id.tap()
//    id.typeText("sunmou.firebase@gmail.com")
//
//    let nextButton = app.webViews.buttons["Next"]
//    nextButton.tap()
//    let pw = app.webViews.secureTextFields["Enter your password"]
//    XCTAssertTrue(id.waitForExistence(timeout: 10))
//    pw.tap()
//    pw.typeText("sunmou.firebase+1")
//    nextButton.tap()
//
//    let accountButton = app.webViews.staticTexts["sunmou.firebase@gmail.com"]
//    XCTAssertTrue(id.waitForExistence(timeout: 3 * 60))
//    accountButton.tap()
//    let allowButton = app.webViews.buttons["Allow"]
//    allowButton.tap()
//
//    sleep(60)
//    app.terminate()
//  }

}

extension XCUIApplication {
  func findElement(at point: CGPoint) -> XCUICoordinate {
    let normalized = coordinate(withNormalizedOffset: .zero)
    let offset = CGVector(dx: point.x, dy: point.y)
    let coordinate = normalized.withOffset(offset)
    return coordinate
  }
}
