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
//  ViewController.swift
//  FirebaseCppUITestApp
//

import UIKit

/// Minimal view controller to launch the UI tests.
class ViewController: UIViewController {

  /// Run the UI Test as soon as we load.
  override func viewDidLoad() {
    super.viewDidLoad()
    NSLog("Starting UI Test")
    launchGame()
  }

  /// Launch the app under test by calling our custom URL scheme.
  func launchGame() {
    let url = URL(string: "firebase-ui-test://")!
    UIApplication.shared.open(url)
  }
}
