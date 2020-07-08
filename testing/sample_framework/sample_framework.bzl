"""Generates build rules for sample apps and Firebase integration test apps.

Used by //third_party/firebase_quickstart/cpp/${package}/sample and
//firebase/${package}/client/cpp/integration_test.
"""

load("//tools/build_defs/android:rules.bzl", "android_binary", "android_manifest_merge")
load("//third_party/firebase/ios:minimum_os.bzl", "IOS_MINIMUM_OS")
load("//tools/build_defs/apple:ios.bzl", "ios_application")
load("//tools/build_defs/build_test:build_test.bzl", "build_test")
load("//javatests/com/google/firebase/client/cpp:android_testapp_test.bzl", "android_testapp_test")

def sample_app(
        name,
        srcs,
        hdrs = None,
        includes = None,
        deps = None,
        defines = None,
        is_test = 0,
        android_package_name = None,
        android_deps = None,
        android_google_services = None,
        android_manifest = None,
        ios_package_name = None,
        ios_deps = None,
        ios_google_services = None,
        ios_infoplist = None,
        ios_entitlements = None,
        ios_provisioning_profile = None,
        desktop_deps = None,
        desktop_google_services = None):
    """Generate build targets for desktop, iOS and Android test apps.

    Generates the following targets:
      :ios_{name}
      :android_{name}
      :desktop_{name}
      :{name} (for build tests)

    If is_test=1, also includes gunit dependency and creates a cc_test target,
      :desktop_{name}_test.

    Args:
      name: Name of the sample app to create.
      srcs: List of platform-independent source files.
      hdrs: List of platform-independent header files.
      includes: List of include paths.
      deps: Library dependencies for all platforms.
      defines: Defines to set in all relevant targets.
      is_test: Set to 1 to create an integration test target.
      android_package_name: Android package name. Omit if you are not building
        an Android app.
      android_manifest: Android manifest filename, "AndroidManifest-{name}.xml"
        by default. The manifest must set "android.app.lib_name" to
        "android_{name}_main".
      android_deps: Additional dependencies for the Android binary.
      android_google_services: File to use instead of "google-services.json".
      ios_package_name: iOS package name. Omit if you are not building an iOS
        app.
      ios_infoplist: iOS Info.plist file, or "Info.plist" if omitted.
      ios_entitlements: iOS entitlements file, if any.
      ios_provisioning_profile: Custom iOS provisioning profile, required for
        entitlements.
      ios_deps: Additional dependencies for the iOS binary.
      ios_google_services: File to use instead of "GoogleService-Info.plist".
      desktop_deps: Additional dependencies for the desktop binary.
      desktop_google_services: File to use instead of "google-services.json" on
        desktop.
    """
    includes = includes if includes else []
    hdrs = hdrs if hdrs else []
    deps = deps if deps else []
    defines = defines if defines else []
    native.cc_library(
        name = name + "_lib",
        srcs = srcs,
        hdrs = hdrs + ["src/app_framework.h"] + (["src/firebase_test_framework.h"] if is_test else []),
        defines = defines,
        includes = includes,
        linkstatic = 1,
        alwayslink = is_test,
        testonly = is_test,
        deps = deps + [
            "//third_party/firebase_quickstart_cpp/sample_framework:app_framework",
        ] + ([
            "//firebase/app/client/cpp/test_framework:firebase_test_framework",
            "//firebase/app/client/cpp/test_framework:googletest_public",
        ] if is_test else []),
    )
    if android_package_name:
        android_manifest = android_manifest if android_manifest else "AndroidManifest.xml"
        android_deps = android_deps if android_deps else []
        android_google_services = android_google_services if android_google_services else "google-services.json"

        # Builds a shared library to include in the test application.
        native.cc_binary(
            name = "libandroid_" + name + "_main.so",
            defines = defines,
            srcs = [],
            linkopts = select({
                "//tools/cc_target_os:android": [
                    # Needed for __android_log_print
                    "-llog",
                    "-landroid",
                    "-Wl,-z,defs",
                    "-Wl,--no-undefined",
                ],
                "//conditions:default": [],
            }),
            linkshared = 1,
            linkstatic = 1,
            testonly = is_test,
            deps = [
                ":" + name + "_lib",
                "//third_party/firebase_quickstart_cpp/sample_framework:app_framework",
            ],
        )

        # Allows the shared library to be included as a dependency of the test
        # application library (see ":{name}_lib").
        native.cc_library(
            name = "android_" + name + "_main",
            testonly = is_test,
            srcs = [":libandroid_" + name + "_main.so"],
        )

        # Merge JarManifest to pick up required permissions to work with Firebase
        # This works around b/28599920.
        android_manifest_merge(
            name = "android_" + name + "_manifest",
            srcs = [android_manifest],
            deps = ["//firebase/app/client/cpp/gms:JarManifest.xml"],
        )

        # Generate string resources for the application.
        native.genrule(
            name = "android_" + name + "_resources",
            srcs = [android_google_services],
            outs = ["res/values/values-" + name + ".xml"],
            cmd = (
                "$(location //firebase/app/client/cpp:generate_xml_from_google_services_json.par) " +
                " -i $(location " + android_google_services + ") " +
                " -o $(location res/values/values-" + name + ".xml)"
            ),
            tools = [
                "//firebase/app/client/cpp:generate_xml_from_google_services_json.par",
            ],
        )

        # Builds a test application that uses the C++ library.
        android_binary(
            name = "android_" + name,
            # Workaround the restriction of java sources needing to live under
            # //{java,javatest}
            custom_package = android_package_name,
            manifest = ":android_" + name + "_manifest",
            multidex = "legacy",
            testonly = is_test,
            manifest_merger = "android",
            manifest_values = {"applicationId": android_package_name},
            resource_files = [
                ":android_" + name + "_resources",
                "res/values/strings.xml",
                "res/layout/main.xml",
            ],
            deps = [
                ":android_" + name + "_main",
                "//third_party/firebase_quickstart_cpp/sample_framework:app_framework_android",
            ] + (["//firebase/app/client/cpp/test_framework:android_firebase_test_framework"] if is_test else []) + android_deps,
        )

        # Must be built using android_x86 in order to run on emulators.
        android_testapp_test(
            name = "android_" + name + "_test",
            package_name = android_package_name + ".test",
            binary_under_test = ":android_" + name,
            package_under_test = android_package_name,
        )

    if ios_package_name:
        ios_infoplist = ios_infoplist if ios_infoplist else "Info.plist"
        ios_deps = ios_deps if ios_deps else []
        ios_google_services = ios_google_services if ios_google_services else "GoogleService-Info.plist"

        # ios application binary.
        native.objc_library(
            name = "ios_" + name + "_binary",
            srcs = [],
            defines = defines,
            hdrs = [],
            data = [ios_google_services],
            testonly = is_test,
            deps = [
                ":" + name + "_lib",
                "//third_party/firebase_quickstart_cpp/sample_framework:app_framework",
            ] + deps,
        )

        # Application package.
        ios_application(
            testonly = is_test,
            name = "ios_" + name,
            bundle_id = ios_package_name,
            families = [
                "iphone",
                "ipad",
            ],
            infoplists = [ios_infoplist],
            entitlements = ios_entitlements,
            provisioning_profile = ios_provisioning_profile if ios_provisioning_profile else "//tools/build_defs/apple:google_ios_development_profile",
            minimum_os_version = IOS_MINIMUM_OS,
            deps = [":ios_" + name + "_binary"] + ios_deps,
        )

    desktop_google_services = desktop_google_services if desktop_google_services else "google-services.json"
    desktop_deps = desktop_deps if desktop_deps else []

    native.cc_binary(
        name = "desktop_" + name,
        testonly = is_test,
        srcs = select({
            # Include an empty main() to workaround go/builder errors.
            "//tools/cc_target_os:android": ["//firebase/app/client/cpp:main.cc"],
            "//firebase/app/client/cpp:ios": ["//firebase/app/client/cpp:main.cc"],
            "//conditions:default": [],
        }),
        defines = defines,
        data = [desktop_google_services],
        linkstatic = 1,
        deps = [":" + name + "_lib"] + desktop_deps,
    )

    # Even though desktop_* will build for windows, osx, and linux, it helps to
    # define separate target names so the Builder can check all 3 desktop
    # platforms.
    native.cc_binary(
        name = "windesktop_" + name,
        testonly = is_test,
        srcs = select({
            # Include an empty main() to workaround go/builder errors.
            "//tools/cc_target_os:windows": [],
            "//conditions:default": ["//firebase/app/client/cpp:main.cc"],
        }),
        defines = defines,
        data = [desktop_google_services],
        linkstatic = 1,
        deps = select({
            "//tools/cc_target_os:windows": [":" + name + "_lib"] + desktop_deps,
            "//conditions:default": [],
        }),
    )

    # Even though desktop_* will build for windows, osx, and linux, it helps to
    # define separate target names so the Builder can check all 3 desktop
    # platforms.
    native.cc_binary(
        name = "osxdesktop_" + name,
        testonly = is_test,
        srcs = select({
            # Include an empty main() to workaround go/builder errors.
            "//tools/cc_target_os:darwin": [],
            "//conditions:default": ["//firebase/app/client/cpp:main.cc"],
        }),
        defines = defines,
        data = [desktop_google_services],
        linkstatic = 1,
        deps = select({
            "//tools/cc_target_os:darwin": [":" + name + "_lib"] + desktop_deps,
            "//conditions:default": [],
        }),
    )

    native.filegroup(
        name = name + "_files",
        srcs = native.glob(
            [
                "**",
                "*",
            ],
            # The BUILD file interferes with a default build path.
            exclude = ["BUILD"],
        ),
        visibility = ["//firebase/app/client/cpp/testapp_builder:__pkg__"],
    )

    # blaze test version of automated test application.
    if is_test:
        native.cc_test(
            name = "desktop_" + name + "_test",
            size = "large",
            srcs = [],
            data = [desktop_google_services],
            tags = ["local"],
            deps = [
                ":" + name + "_lib",
                "//firebase/app/client/cpp/test_framework:googletest_public",
            ] + desktop_deps,
        )

    # Exoblaze does not have a pkg_library implementation, so we check first.
    if hasattr(native, "pkg_library"):
        # It's not possible to use select() in build_test() so use a package to
        # conditionally include the app build target based upon target OS.
        native.pkg_library(
            name = name,
            testonly = is_test,
            srcs = select({
                "//tools/cc_target_os:android": [":" + name + "_android"],
                "//firebase/app/client/cpp:ios": [":" + name + "_ios"],
                "//conditions:default": [":" + name + "_desktop"],
            }),
        )
        build_test(
            name = name + "_build_test",
            targets = [":" + name],
        )
