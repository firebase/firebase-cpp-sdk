# Firebase Analytics on macOS desktop via the Firebase Apple SDK

On macOS desktop, `firebase::analytics` can forward to the Firebase Apple SDK
(`FIRAnalytics` / GoogleAppMeasurement) when it is **embedded** in your app.
If it isn't, Analytics keeps the desktop stub behavior from before.

```
your app / FirebaseCppApp.bundle
├── firebase_analytics (analytics_desktop.cc)
│     └── analytics_desktop_apple.mm ── NSClassFromString("FIRAnalytics") ──┐
└── embedded Firebase Apple SDK (force-loaded static archives)  ◄───────────┘
      FirebaseAnalytics, GoogleAppMeasurement, FirebaseCore,
      FirebaseCoreInternal (Swift), FirebaseInstallations, GoogleUtilities,
      FBLPromises, nanopb (or desktop Firestore's copy)
```

`firebase_analytics` binds to the Apple SDK **at runtime**, so it has no new
link-time dependency. Only the final binary (app, bundle or dylib) chooses to
embed the SDK.

## Embedding the SDK (CMake)

```cmake
add_subdirectory(${FIREBASE_CPP_SDK_DIR} bin/ EXCLUDE_FROM_ALL)
target_link_libraries(my_app firebase_analytics firebase_app)
firebase_cpp_link_apple_analytics_sdk(my_app)   # macOS only; no-op elsewhere
```

At configure time this runs `build_apple_analytics_sdk.py` (macOS + Xcode 15+,
network access for SwiftPM), which builds the Firebase Apple SDK version
pinned in [`ios_spm/Package.swift`](../../ios_spm/Package.swift). To reuse a
prebuilt copy (CI caches, offline builds), build it once and pass
`-DFIREBASE_APPLE_ANALYTICS_SDK_DIR=<dir>`:

```sh
python3 analytics/apple_desktop/build_apple_analytics_sdk.py \
    --output-dir /tmp/firebase_apple_analytics_sdk --archs arm64 x86_64
```

The app must be configured with an **Apple** Firebase app. Generate
`google-services-desktop.json` from its `GoogleService-Info.plist`
(`generate_xml_from_google_services_json.py --plist`). With an Android App ID,
Analytics logs a warning and falls back to the stubs.

## Symbol conflicts

| Risk | Handling |
|---|---|
| nanopb is defined both by the Apple SDK and by desktop Firestore | When `protobuf-nanopb-static` exists, the SwiftPM nanopb archive is skipped and Firestore's is linked instead. Both are nanopb 0.3.9.x built with `PB_FIELD_32BIT`, `PB_ENABLE_MALLOC` and `PB_NO_PACKED_STRUCTS`. |
| Other duplicates (zlib, BoringSSL, ObjC classes, ...) | `scripts/check_duplicate_symbols.py` runs as a PRE_LINK step. It fails the link if a force-loaded SDK archive defines a strong symbol that another static library in the link also defines (`FIREBASE_APPLE_ANALYTICS_SDK_CHECK_DUPLICATE_SYMBOLS`). |
| SDK symbols leaking out of / colliding inside shared libraries (e.g. the Unity bundle, or a second Firebase Apple SDK in the process) | For `SHARED`/`MODULE` targets, the SDK's symbols are hidden with `-unexported_symbols_list` ([list](../../cmake/firebase_apple_desktop_sdk_unexported_symbols.txt)). Pass `KEEP_SDK_SYMBOLS_EXPORTED` to opt out. |
| Missing ObjC categories / `+load` registrations | SDK archives are force-loaded, which matches how Xcode links SwiftPM packages. |

The Objective-C runtime registers classes by name per process. If the host
process also loads another copy of the Firebase Apple SDK (for example a
native macOS plugin), the runtime warns about duplicate classes. Don't embed
the SDK twice.

## Binary size

* Each module is archived separately, so only what FirebaseAnalytics needs is
  embedded.
* Non-Debug builds link with `-dead_strip`. Hidden SDK symbols become local,
  so `strip -x` (used by the Unity bundle build) removes their names too.
* `<sdk dir>/size_report.txt` lists each module's archive size. CMake prints
  it when a target embeds the SDK. Compare the final binary with and without
  embedding to track the real (post dead-strip) cost.

## Runtime behavior

| Control | Effect |
|---|---|
| `FIREBASE_ANALYTICS_APPLE_SDK=disabled` | Use the stubs even if the SDK is embedded |
| `FIREBASE_ANALYTICS_APPLE_SDK=enabled` | Use the Apple SDK and clear the persisted debug flag |
| `FIREBASE_ANALYTICS_APPLE_SDK=debug` | Use the Apple SDK with debug mode (DebugView) on |
| *(unset)* | Use the Apple SDK, everywhere including the Unity Editor. The persisted debug flag is left as is |
| `SetDesktopDebugMode(bool)` | Persists the Apple SDK debug flag, which applies from the next launch |

**Unity Editor.** Analytics is on by default in the Editor and behaves like
a standalone player. Debug mode is up to the developer. Known Editor caveats,
which this SDK cannot fix:

* GoogleAppMeasurement reports `[NSBundle mainBundle].bundleIdentifier` and
  the main bundle's app version. In the Editor, those are the Unity Editor's.
  `FIROptions.bundleID` (set from `AppOptions::package_name()`) doesn't change
  this. FirebaseCore only uses it to log `I-COR000008` when it doesn't match
  the main bundle, so expect that log line in the Editor.
* GoogleAppMeasurement keeps its state in the Editor's NSUserDefaults /
  Application Support, which every Unity project on the machine shares. So
  all projects share one app instance ID and event queue in the Editor.
* Analytics starts once per process. It survives play-mode cycles and domain
  reloads. Switching Firebase projects, or changing debug mode, requires
  restarting the Editor.

Feature mapping compared with the stubs:

| API | macOS with Apple SDK |
|---|---|
| `LogEvent`, `SetUserProperty`, `SetUserId`, `SetDefaultEventParameters`, `SetConsent`, `SetSessionTimeoutDuration`, `SetAnalyticsCollectionEnabled`, `ResetAnalyticsData` | Same semantics as iOS |
| `GetAnalyticsInstanceId`, `GetSessionId` | Real values. The instance ID future times out after 30 s |
| `IsDesktopInitialized` | `true` |
| `InitiateOnDeviceConversionMeasurement*` | Forwarded if the SDK supports it on macOS, otherwise a logged no-op |
| `LogAppleTransaction` | Still a no-op that returns success (StoreKit 2 bridging is Swift-only) |
| `SetLogCallback`, `NotifyAppLifecycleChange` | No-op. The Apple SDK observes `NSApplication` itself |

## Privacy manifest

The build merges every `PrivacyInfo.xcprivacy` in the embedded SDK into
`<sdk dir>/PrivacyInfo.xcprivacy`. Ship it in your `.app`'s
`Contents/Resources` when distributing through the Mac App Store. Sandboxed
apps also need the `com.apple.security.network.client` entitlement.

## Testing

* `analytics/apple_desktop/build_apple_analytics_sdk_test.py` and
  `scripts/check_duplicate_symbols_test.py` run anywhere with Python 3.
* `firebase_analytics_desktop_apple_test` (macOS) checks backend selection and
  the stub fallback.
* The desktop integration test embeds the SDK with
  `-DFIREBASE_ANALYTICS_TEST_EMBED_APPLE_SDK=ON`. It builds an `.app` bundle
  and needs an Apple-app `google-services-desktop.json`.
