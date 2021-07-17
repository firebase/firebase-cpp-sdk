// Copyright 2021 Google LLC

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
