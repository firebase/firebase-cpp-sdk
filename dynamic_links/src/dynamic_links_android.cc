// Copyright 2017 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <assert.h>
#include <jni.h>

#include "app/src/assert.h"
#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/internal/common.h"
#include "app/src/include/firebase/version.h"
#include "app/src/reference_counted_future_impl.h"
#include "app/src/util.h"
#include "app/src/util_android.h"
#include "dynamic_links/src/common.h"
#include "dynamic_links/src/include/firebase/dynamic_links.h"
#include "dynamic_links/src/include/firebase/dynamic_links/components.h"

namespace firebase {
namespace dynamic_links {

DEFINE_FIREBASE_VERSION_STRING(FirebaseDynamicLinks);

// Methods of the FirebaseDynamicLinks class.
// clang-format off
#define DYNAMIC_LINKS_METHODS(X)                                            \
  X(GetInstance, "getInstance",                                             \
    "()Lcom/google/firebase/dynamiclinks/FirebaseDynamicLinks;",            \
    util::kMethodTypeStatic),                                               \
  X(GetDynamicLinkFromIntent, "getDynamicLink",                             \
    "(Landroid/content/Intent;)Lcom/google/android/gms/tasks/Task;"),       \
  X(GetDynamicLinkFromUri, "getDynamicLink",                                \
    "(Landroid/net/Uri;)Lcom/google/android/gms/tasks/Task;"),              \
  X(CreateDynamicLink, "createDynamicLink",                                 \
    "()Lcom/google/firebase/dynamiclinks/DynamicLink$Builder;")
// clang-format on

METHOD_LOOKUP_DECLARATION(dynamic_links, DYNAMIC_LINKS_METHODS)
METHOD_LOOKUP_DEFINITION(
    dynamic_links,
    PROGUARD_KEEP_CLASS "com/google/firebase/dynamiclinks/FirebaseDynamicLinks",
    DYNAMIC_LINKS_METHODS)

// Methods of the DynamicLink class.
// clang-format off
#define DLINK_METHODS(X)                                                    \
  X(GetUri, "getUri", "()Landroid/net/Uri;")
// clang-format on

METHOD_LOOKUP_DECLARATION(dlink, DLINK_METHODS)
METHOD_LOOKUP_DEFINITION(dlink,
                         PROGUARD_KEEP_CLASS
                         "com/google/firebase/dynamiclinks/DynamicLink",
                         DLINK_METHODS)

// Methods of the DynamicLink.Builder class.
// clang-format off
#define DLINK_BUILDER_METHODS(X)                                            \
  X(SetLongLink, "setLongLink",                                             \
    "(Landroid/net/Uri;)"                                                   \
    "Lcom/google/firebase/dynamiclinks/DynamicLink$Builder;"),              \
  X(SetLink, "setLink",                                                     \
    "(Landroid/net/Uri;)"                                                   \
    "Lcom/google/firebase/dynamiclinks/DynamicLink$Builder;"),              \
  X(SetDomainUriPrefix, "setDomainUriPrefix",                               \
    "(Ljava/lang/String;)"                                                  \
    "Lcom/google/firebase/dynamiclinks/DynamicLink$Builder;"),              \
  X(SetAndroidParameters, "setAndroidParameters",                           \
    "(Lcom/google/firebase/dynamiclinks/DynamicLink$AndroidParameters;)"    \
    "Lcom/google/firebase/dynamiclinks/DynamicLink$Builder;"),              \
  X(SetIosParameters, "setIosParameters",                                   \
    "(Lcom/google/firebase/dynamiclinks/DynamicLink$IosParameters;)"        \
    "Lcom/google/firebase/dynamiclinks/DynamicLink$Builder;"),              \
  X(SetGoogleAnalyticsParameters, "setGoogleAnalyticsParameters",           \
    "(Lcom/google/firebase/dynamiclinks/DynamicLink$"                       \
    "GoogleAnalyticsParameters;)"                                           \
    "Lcom/google/firebase/dynamiclinks/DynamicLink$Builder;"),              \
  X(SetItunesConnectAnalyticsParameters,                                    \
    "setItunesConnectAnalyticsParameters",                                  \
    "(Lcom/google/firebase/dynamiclinks/DynamicLink$"                       \
    "ItunesConnectAnalyticsParameters;)"                                    \
    "Lcom/google/firebase/dynamiclinks/DynamicLink$Builder;"),              \
  X(SetSocialMetaTagParameters, "setSocialMetaTagParameters",               \
    "(Lcom/google/firebase/dynamiclinks/DynamicLink$"                       \
    "SocialMetaTagParameters;)"                                             \
    "Lcom/google/firebase/dynamiclinks/DynamicLink$Builder;"),              \
  X(BuildDynamicLink, "buildDynamicLink",                                   \
    "()Lcom/google/firebase/dynamiclinks/DynamicLink;"),                    \
  X(BuildShortDynamicLink, "buildShortDynamicLink",                         \
    "()Lcom/google/android/gms/tasks/Task;"),                               \
  X(BuildShortDynamicLinkWithOption, "buildShortDynamicLink",               \
    "(I)Lcom/google/android/gms/tasks/Task;")
// clang-format on

METHOD_LOOKUP_DECLARATION(dlink_builder, DLINK_BUILDER_METHODS)
METHOD_LOOKUP_DEFINITION(dlink_builder,
                         PROGUARD_KEEP_CLASS
                         "com/google/firebase/dynamiclinks/DynamicLink$Builder",
                         DLINK_BUILDER_METHODS)

// Methods of the DynamicLink.AndroidParameters.Builder class.
// clang-format off
#define DLINK_ANDROID_PARAMS_BUILDER_METHODS(X)                             \
  X(Constructor, "<init>", "()V"),                                          \
  X(ConstructorFromPackageName, "<init>", "(Ljava/lang/String;)V"),         \
  X(SetFallbackUrl, "setFallbackUrl",                                       \
    "(Landroid/net/Uri;)Lcom/google/firebase/dynamiclinks/DynamicLink$"     \
    "AndroidParameters$Builder;"),                                          \
  X(SetMinimumVersion, "setMinimumVersion",                                 \
    "(I)Lcom/google/firebase/dynamiclinks/DynamicLink$"                     \
    "AndroidParameters$Builder;"),                                          \
  X(Build, "build", "()Lcom/google/firebase/dynamiclinks/DynamicLink$"      \
    "AndroidParameters;")
// clang-format on

METHOD_LOOKUP_DECLARATION(dlink_android_params_builder,
                          DLINK_ANDROID_PARAMS_BUILDER_METHODS)
METHOD_LOOKUP_DEFINITION(
    dlink_android_params_builder,
    PROGUARD_KEEP_CLASS
    "com/google/firebase/dynamiclinks/DynamicLink$AndroidParameters$Builder",
    DLINK_ANDROID_PARAMS_BUILDER_METHODS)

// Methods of the DynamicLink.GoogleAnalyticsParameters.Builder class.
// clang-format off
#define DLINK_GOOGLE_ANALYTICS_PARAMATERS_BUILDER_METHODS(X)                \
  X(Constructor, "<init>", "()V"),                                          \
  X(SetSource, "setSource", "(Ljava/lang/String;)"                          \
    "Lcom/google/firebase/dynamiclinks/DynamicLink$"                        \
    "GoogleAnalyticsParameters$Builder;"),                                  \
  X(SetMedium, "setMedium", "(Ljava/lang/String;)"                          \
    "Lcom/google/firebase/dynamiclinks/DynamicLink$"                        \
    "GoogleAnalyticsParameters$Builder;"),                                  \
  X(SetCampaign, "setCampaign", "(Ljava/lang/String;)"                      \
    "Lcom/google/firebase/dynamiclinks/DynamicLink$"                        \
    "GoogleAnalyticsParameters$Builder;"),                                  \
  X(SetTerm, "setTerm", "(Ljava/lang/String;)"                              \
    "Lcom/google/firebase/dynamiclinks/DynamicLink$"                        \
    "GoogleAnalyticsParameters$Builder;"),                                  \
  X(SetContent, "setContent", "(Ljava/lang/String;)"                        \
    "Lcom/google/firebase/dynamiclinks/DynamicLink$"                        \
    "GoogleAnalyticsParameters$Builder;"),                                  \
  X(Build, "build", "()Lcom/google/firebase/dynamiclinks/DynamicLink$"      \
    "GoogleAnalyticsParameters;")
// clang-format on

METHOD_LOOKUP_DECLARATION(dlink_google_analytics_params_builder,
                          DLINK_GOOGLE_ANALYTICS_PARAMATERS_BUILDER_METHODS)
METHOD_LOOKUP_DEFINITION(dlink_google_analytics_params_builder,
                         PROGUARD_KEEP_CLASS
                         "com/google/firebase/dynamiclinks/DynamicLink$"
                         "GoogleAnalyticsParameters$Builder",
                         DLINK_GOOGLE_ANALYTICS_PARAMATERS_BUILDER_METHODS)

// Methods of the DynamicLink.IosParameters.Builder class.
// clang-format off
#define DLINK_IOS_PARAMETERS_BUILDER_METHODS(X)                              \
  X(Constructor, "<init>", "(Ljava/lang/String;)V"),                         \
  X(SetFallbackUrl, "setFallbackUrl", "(Landroid/net/Uri;)"                  \
    "Lcom/google/firebase/dynamiclinks/DynamicLink$IosParameters$Builder;"), \
  X(SetCustomScheme, "setCustomScheme", "(Ljava/lang/String;)"               \
    "Lcom/google/firebase/dynamiclinks/DynamicLink$IosParameters$Builder;"), \
  X(SetIpadFallbackUrl, "setIpadFallbackUrl", "(Landroid/net/Uri;)"          \
    "Lcom/google/firebase/dynamiclinks/DynamicLink$IosParameters$Builder;"), \
  X(SetIpadBundleId, "setIpadBundleId", "(Ljava/lang/String;)"               \
    "Lcom/google/firebase/dynamiclinks/DynamicLink$IosParameters$Builder;"), \
  X(SetAppStoreId, "setAppStoreId", "(Ljava/lang/String;)"                   \
    "Lcom/google/firebase/dynamiclinks/DynamicLink$IosParameters$Builder;"), \
  X(SetMinimumVersion, "setMinimumVersion", "(Ljava/lang/String;)"           \
    "Lcom/google/firebase/dynamiclinks/DynamicLink$IosParameters$Builder;"), \
  X(Build, "build",                                                          \
    "()Lcom/google/firebase/dynamiclinks/DynamicLink$IosParameters;")
// clang-format on

METHOD_LOOKUP_DECLARATION(dlink_ios_params_builder,
                          DLINK_IOS_PARAMETERS_BUILDER_METHODS)
METHOD_LOOKUP_DEFINITION(
    dlink_ios_params_builder,
    PROGUARD_KEEP_CLASS
    "com/google/firebase/dynamiclinks/DynamicLink$IosParameters$Builder",
    DLINK_IOS_PARAMETERS_BUILDER_METHODS)

// Methods of the DynamicLink.ItunesConnectAnalyticsParameters.Builder class.
// clang-format off
#define DLINK_ITUNES_CONNECT_PARAMETERS_BUILDER_METHODS(X)                   \
  X(Constructor, "<init>", "()V"),                                           \
  X(SetProviderToken, "setProviderToken", "(Ljava/lang/String;)"             \
    "Lcom/google/firebase/dynamiclinks/DynamicLink$"                         \
    "ItunesConnectAnalyticsParameters$Builder;"),                            \
  X(SetAffiliateToken, "setAffiliateToken", "(Ljava/lang/String;)"           \
    "Lcom/google/firebase/dynamiclinks/DynamicLink$"                         \
    "ItunesConnectAnalyticsParameters$Builder;"),                            \
  X(SetCampaignToken, "setCampaignToken", "(Ljava/lang/String;)"             \
    "Lcom/google/firebase/dynamiclinks/DynamicLink$"                         \
    "ItunesConnectAnalyticsParameters$Builder;"),                            \
  X(Build, "build", "()Lcom/google/firebase/dynamiclinks/DynamicLink$"       \
    "ItunesConnectAnalyticsParameters;")
// clang-format on

METHOD_LOOKUP_DECLARATION(dlink_itunes_params_builder,
                          DLINK_ITUNES_CONNECT_PARAMETERS_BUILDER_METHODS)
METHOD_LOOKUP_DEFINITION(dlink_itunes_params_builder,
                         PROGUARD_KEEP_CLASS
                         "com/google/firebase/dynamiclinks/DynamicLink$"
                         "ItunesConnectAnalyticsParameters$Builder",
                         DLINK_ITUNES_CONNECT_PARAMETERS_BUILDER_METHODS)

// Methods of the DynamicLink.SocialMetaTagParameters.Builder class.
// clang-format off
#define DLINK_SOCIAL_META_TAG_PARAMETERS_BUILDER_METHODS(X)                  \
  X(Constructor, "<init>", "()V"),                                           \
  X(SetTitle, "setTitle", "(Ljava/lang/String;)"                             \
    "Lcom/google/firebase/dynamiclinks/DynamicLink$"                         \
    "SocialMetaTagParameters$Builder;"),                                     \
  X(SetDescription, "setDescription", "(Ljava/lang/String;)"                 \
    "Lcom/google/firebase/dynamiclinks/DynamicLink$"                         \
    "SocialMetaTagParameters$Builder;"),                                     \
  X(SetImageUrl, "setImageUrl", "(Landroid/net/Uri;)"                        \
    "Lcom/google/firebase/dynamiclinks/DynamicLink$"                         \
    "SocialMetaTagParameters$Builder;"),                                     \
  X(Build, "build", "()Lcom/google/firebase/dynamiclinks/DynamicLink$"       \
    "SocialMetaTagParameters;")
// clang-format on

METHOD_LOOKUP_DECLARATION(dlink_social_meta_params_builder,
                          DLINK_SOCIAL_META_TAG_PARAMETERS_BUILDER_METHODS)
METHOD_LOOKUP_DEFINITION(dlink_social_meta_params_builder,
                         PROGUARD_KEEP_CLASS
                         "com/google/firebase/dynamiclinks/DynamicLink$"
                         "SocialMetaTagParameters$Builder",
                         DLINK_SOCIAL_META_TAG_PARAMETERS_BUILDER_METHODS)

// Methods of the PendingDynamicLinkData class.
// clang-format off
#define PENDING_DYNAMIC_LINK_DATA_METHODS(X)                                \
  X(GetLink, "getLink", "()Landroid/net/Uri;"),                             \
  X(GetMinimumAppVersion, "getMinimumAppVersion", "()I"),                   \
  X(GetClickTimestamp, "getClickTimestamp", "()J"),                         \
  X(GetUpdateAppIntent, "getUpdateAppIntent",                               \
    "(Landroid/content/Context;)Landroid/content/Intent;")
// clang-format on

METHOD_LOOKUP_DECLARATION(pending_dynamic_link_data,
                          PENDING_DYNAMIC_LINK_DATA_METHODS)
METHOD_LOOKUP_DEFINITION(
    pending_dynamic_link_data,
    PROGUARD_KEEP_CLASS
    "com/google/firebase/dynamiclinks/PendingDynamicLinkData",
    PENDING_DYNAMIC_LINK_DATA_METHODS)

// Methods of the ShortDynamicLink interface.
// clang-format off
#define SHORT_DYNAMIC_LINK_METHODS(X)                                       \
  X(GetShortLink, "getShortLink", "()Landroid/net/Uri;"),                   \
  X(GetPreviewLink, "getPreviewLink", "()Landroid/net/Uri;"),               \
  X(GetWarnings, "getWarnings", "()Ljava/util/List;")
// clang-format on

METHOD_LOOKUP_DECLARATION(short_dynamic_link, SHORT_DYNAMIC_LINK_METHODS)
METHOD_LOOKUP_DEFINITION(short_dynamic_link,
                         PROGUARD_KEEP_CLASS
                         "com/google/firebase/dynamiclinks/ShortDynamicLink",
                         SHORT_DYNAMIC_LINK_METHODS)

// Methods of the ShortDynamicLinkWarning interface.
// clang-format off
#define SHORT_DYNAMIC_LINK_WARNING_METHODS(X)                               \
  X(GetCode, "getCode", "()Ljava/lang/String;"),                            \
  X(GetMessage, "getMessage", "()Ljava/lang/String;")
// clang-format on

METHOD_LOOKUP_DECLARATION(short_dynamic_link_warning,
                          SHORT_DYNAMIC_LINK_WARNING_METHODS)
METHOD_LOOKUP_DEFINITION(
    short_dynamic_link_warning,
    PROGUARD_KEEP_CLASS
    "com/google/firebase/dynamiclinks/ShortDynamicLink$Warning",
    SHORT_DYNAMIC_LINK_WARNING_METHODS)

// Fields of the ShortDynamicLink$Suffix interface.
// clang-format off
#define SHORT_DYNAMIC_LINK_SUFFIX_FIELDS(X)                               \
  X(Unguessable, "UNGUESSABLE", "I", util::kFieldTypeStatic),             \
  X(Short, "SHORT", "I", util::kFieldTypeStatic)
// clang-format on

METHOD_LOOKUP_DECLARATION(short_dynamic_link_suffix, METHOD_LOOKUP_NONE,
                          SHORT_DYNAMIC_LINK_SUFFIX_FIELDS)
METHOD_LOOKUP_DEFINITION(
    short_dynamic_link_suffix,
    PROGUARD_KEEP_CLASS
    "com/google/firebase/dynamiclinks/ShortDynamicLink$Suffix",
    METHOD_LOOKUP_NONE, SHORT_DYNAMIC_LINK_SUFFIX_FIELDS)

// Map c++ PathLength constants to java path length constants.
static struct {
  PathLength path_length_code;
  short_dynamic_link_suffix::Field java_path_length_field;
  jint value;
} g_path_length_codes[] = {
    {kPathLengthShort, short_dynamic_link_suffix::kShort},
    {kPathLengthUnguessable, short_dynamic_link_suffix::kUnguessable},
};

// Global reference to the Android FirebaseDynamicLinks class instance.
// This is initialized in dynamic_links::Initialize() and never released
// during the lifetime of the application.
static jobject g_dynamic_links_class_instance = nullptr;

// Used to retrieve the JNI environment in order to call methods on the
// Android Analytics class.
static const ::firebase::App* g_app = nullptr;

static const char* kApiIdentifier = "Dynamic Links";

static void ReleaseClasses(JNIEnv* env) {
  dynamic_links::ReleaseClass(env);
  dlink::ReleaseClass(env);
  dlink_builder::ReleaseClass(env);
  dlink_android_params_builder::ReleaseClass(env);
  dlink_google_analytics_params_builder::ReleaseClass(env);
  dlink_ios_params_builder::ReleaseClass(env);
  dlink_itunes_params_builder::ReleaseClass(env);
  dlink_social_meta_params_builder::ReleaseClass(env);
  pending_dynamic_link_data::ReleaseClass(env);
  short_dynamic_link::ReleaseClass(env);
  short_dynamic_link_warning::ReleaseClass(env);
  short_dynamic_link_suffix::ReleaseClass(env);
}

InitResult Initialize(const App& app, Listener* listener) {
  if (g_app) {
    LogWarning("%s API already initialized", kApiIdentifier);
    return kInitResultSuccess;
  }
  FIREBASE_UTIL_RETURN_FAILURE_IF_GOOGLE_PLAY_UNAVAILABLE(app);

  LogDebug("%s API Initializing", kApiIdentifier);
  assert(!g_dynamic_links_class_instance);

  if (!CreateReceiver(app)) {
    return kInitResultFailedMissingDependency;
  }

  JNIEnv* env = app.GetJNIEnv();
  jobject activity = app.activity();

  // Cache method pointers.
  if (!(dynamic_links::CacheMethodIds(env, activity) &&
        dlink::CacheMethodIds(env, activity) &&
        dlink_builder::CacheMethodIds(env, activity) &&
        dlink_android_params_builder::CacheMethodIds(env, activity) &&
        dlink_google_analytics_params_builder::CacheMethodIds(env, activity) &&
        dlink_ios_params_builder::CacheMethodIds(env, activity) &&
        dlink_itunes_params_builder::CacheMethodIds(env, activity) &&
        dlink_social_meta_params_builder::CacheMethodIds(env, activity) &&
        pending_dynamic_link_data::CacheMethodIds(env, activity) &&
        short_dynamic_link::CacheMethodIds(env, activity) &&
        short_dynamic_link_warning::CacheMethodIds(env, activity) &&
        short_dynamic_link_suffix::CacheFieldIds(env, activity))) {
    ReleaseClasses(env);
    DestroyReceiver();
    return kInitResultFailedMissingDependency;
  }

  g_app = &app;

  // Create the dynamic links class.
  jclass dynamic_links_class = dynamic_links::GetClass();
  jobject dynamic_links_instance_local = env->CallStaticObjectMethod(
      dynamic_links_class,
      dynamic_links::GetMethodId(dynamic_links::kGetInstance));
  assert(dynamic_links_instance_local);
  g_dynamic_links_class_instance =
      env->NewGlobalRef(dynamic_links_instance_local);
  env->DeleteLocalRef(dynamic_links_instance_local);

  // Cache PathLength codes
  {
    int i;
    for (i = 0; i < FIREBASE_ARRAYSIZE(g_path_length_codes); ++i) {
      g_path_length_codes[i].value = env->GetStaticIntField(
          short_dynamic_link_suffix::GetClass(),
          short_dynamic_link_suffix::GetFieldId(
              g_path_length_codes[i].java_path_length_field));
    }
    // The map from FieldLengths to suffix field IDs doesn't match the number of
    // fields defined on the Suffix interface.
    assert(i == short_dynamic_link_suffix::kFieldCount);
  }

  FutureData::Create();
  SetListener(listener);

  LogInfo("%s API Initialized", kApiIdentifier);
  return kInitResultSuccess;
}

namespace internal {

bool IsInitialized() { return g_app != nullptr; }

}  // namespace internal

void Terminate() {
  if (!g_app) {
    LogWarning("%s already shut down", kApiIdentifier);
    return;
  }
  DestroyReceiver();
  JNIEnv* env = g_app->GetJNIEnv();
  g_app = nullptr;
  env->DeleteGlobalRef(g_dynamic_links_class_instance);
  g_dynamic_links_class_instance = nullptr;
  util::CancelCallbacks(env, kApiIdentifier);

  FutureData::Destroy();
  ReleaseClasses(env);
}

// Creates a Uri from the string passed in and sets it on the builder using
// the builder_set_method_id.
// Deletes and clears the reference to the builder passed in.
// Returns the new builder which can be used for additional calls.
static jobject SetBuilderUri(JNIEnv* jni_env, jobject builder,
                             const char* value,
                             jmethodID builder_set_method_id) {
  // The builder is null. Did you forget to take the builder result of a
  // previous set call?
  assert(builder);
  // If there's no value to set, we can just return the original builder.
  if (!value) return builder;
  jobject uri_local = util::ParseUriString(jni_env, value);
  jobject builder_morphed =
      jni_env->CallObjectMethod(builder, builder_set_method_id, uri_local);
  jni_env->DeleteLocalRef(uri_local);
  jni_env->DeleteLocalRef(builder);
  return builder_morphed;
}

// Creates a jstring from the string passed in and sets it on the builder using
// the builder_set_method_id.
// Deletes and clears the reference to the builder passed in.
// Returns the new builder which can be used for additional calls.
static jobject SetBuilderString(JNIEnv* jni_env, jobject builder,
                                const char* value,
                                jmethodID builder_set_method_id) {
  // The builder is null. Did you forget to take the builder result of a
  // previous set call?
  assert(builder);
  if (!value) return builder;
  jstring string_value = jni_env->NewStringUTF(value);
  jobject builder_morphed =
      jni_env->CallObjectMethod(builder, builder_set_method_id, string_value);
  jni_env->DeleteLocalRef(string_value);
  jni_env->DeleteLocalRef(builder);
  return builder_morphed;
}

// Sets an object reference on a builder
static jobject SetBuilderObject(JNIEnv* jni_env, jobject builder, jobject obj,
                                jmethodID builder_set_method_id) {
  // The builder is null. Did you forget to take the builder result of a
  // previous set call?
  assert(builder);
  jobject builder_morphed =
      jni_env->CallObjectMethod(builder, builder_set_method_id, obj);
  jni_env->DeleteLocalRef(builder);
  return builder_morphed;
}

// Sets a native type on the builder using the builder_set_method_id.
// T should be cast to the appropriate base java type:
//   jboolean, jbyte, jchar, jshort, jint, jlong, jfloat, jdouble
// This is mainly to help all the builder calls be consistent.
// Deletes and clears the reference to the builder passed in.
// Returns the new builder which can be used for additional calls.
template <typename T>
static jobject SetBuilderBaseType(JNIEnv* jni_env, jobject builder, T arg,
                                  jmethodID builder_set_method_id) {
  // The builder is null. Did you forget to take the builder result of a
  // previous set call?
  assert(builder);
  jobject builder_morphed =
      jni_env->CallObjectMethod(builder, builder_set_method_id, arg);
  jni_env->DeleteLocalRef(builder);
  return builder_morphed;
}

// Calls the builder.Build function (method_id passed in).
// This also deletes the local ref to the builder.
// Returns a local ref to the constructed object. (You must call DeleteLocalRef
// on the returned object.)
static jobject BuildBuilder(JNIEnv* jni_env, jobject builder,
                            jmethodID builder_build_method_id) {
  // The builder is null. Did you forget to take the builder result of a
  // previous set call?
  assert(builder);
  jobject built_obj =
      jni_env->CallObjectMethod(builder, builder_build_method_id);
  jni_env->DeleteLocalRef(builder);
  return built_obj;
}

// You must call DeleteLocalRef on the returned object.
static jobject CreateAndroidParameters(JNIEnv* jni_env,
                                       const AndroidParameters& params,
                                       std::string* error_out) {
  if (!params.package_name || !*params.package_name) {
    *error_out = "Android Package Name is missing.";
    return nullptr;
  }

  jstring package_name_local = jni_env->NewStringUTF(params.package_name);
  jobject builder = jni_env->NewObject(
      dlink_android_params_builder::GetClass(),
      dlink_android_params_builder::GetMethodId(
          dlink_android_params_builder::kConstructorFromPackageName),
      package_name_local);
  jni_env->DeleteLocalRef(package_name_local);

  if (params.fallback_url) {
    builder = SetBuilderUri(jni_env, builder, params.fallback_url,
                            dlink_android_params_builder::GetMethodId(
                                dlink_android_params_builder::kSetFallbackUrl));
  }
  builder =
      SetBuilderBaseType(jni_env, builder, (jint)params.minimum_version,
                         dlink_android_params_builder::GetMethodId(
                             dlink_android_params_builder::kSetMinimumVersion));
  return BuildBuilder(jni_env, builder,
                      dlink_android_params_builder::GetMethodId(
                          dlink_android_params_builder::kBuild));
}

// You must call DeleteLocalRef on the returned object.
static jobject CreateGoogleAnalyticsParameters(
    JNIEnv* jni_env, const GoogleAnalyticsParameters& params) {
  jobject builder = jni_env->NewObject(
      dlink_google_analytics_params_builder::GetClass(),
      dlink_google_analytics_params_builder::GetMethodId(
          dlink_google_analytics_params_builder::kConstructor));

  builder =
      SetBuilderString(jni_env, builder, params.source,
                       dlink_google_analytics_params_builder::GetMethodId(
                           dlink_google_analytics_params_builder::kSetSource));
  builder =
      SetBuilderString(jni_env, builder, params.medium,
                       dlink_google_analytics_params_builder::GetMethodId(
                           dlink_google_analytics_params_builder::kSetMedium));
  builder = SetBuilderString(
      jni_env, builder, params.campaign,
      dlink_google_analytics_params_builder::GetMethodId(
          dlink_google_analytics_params_builder::kSetCampaign));
  builder =
      SetBuilderString(jni_env, builder, params.term,
                       dlink_google_analytics_params_builder::GetMethodId(
                           dlink_google_analytics_params_builder::kSetTerm));
  builder =
      SetBuilderString(jni_env, builder, params.content,
                       dlink_google_analytics_params_builder::GetMethodId(
                           dlink_google_analytics_params_builder::kSetContent));

  return BuildBuilder(jni_env, builder,
                      dlink_google_analytics_params_builder::GetMethodId(
                          dlink_google_analytics_params_builder::kBuild));
}

// You must call DeleteLocalRef on the returned object.
static jobject CreateIOSParameters(JNIEnv* jni_env, const IOSParameters& params,
                                   std::string* error_out) {
  if (!params.bundle_id || !*params.bundle_id) {
    *error_out = "IOS Bundle ID is missing.";
    return nullptr;
  }

  jstring bundle_id_local = jni_env->NewStringUTF(params.bundle_id);
  jobject builder =
      jni_env->NewObject(dlink_ios_params_builder::GetClass(),
                         dlink_ios_params_builder::GetMethodId(
                             dlink_ios_params_builder::kConstructor),
                         bundle_id_local);
  jni_env->DeleteLocalRef(bundle_id_local);

  builder = SetBuilderUri(jni_env, builder, params.fallback_url,
                          dlink_ios_params_builder::GetMethodId(
                              dlink_ios_params_builder::kSetFallbackUrl));
  builder = SetBuilderString(jni_env, builder, params.custom_scheme,
                             dlink_ios_params_builder::GetMethodId(
                                 dlink_ios_params_builder::kSetCustomScheme));
  builder = SetBuilderUri(jni_env, builder, params.ipad_fallback_url,
                          dlink_ios_params_builder::GetMethodId(
                              dlink_ios_params_builder::kSetIpadFallbackUrl));
  builder = SetBuilderString(jni_env, builder, params.ipad_bundle_id,
                             dlink_ios_params_builder::GetMethodId(
                                 dlink_ios_params_builder::kSetIpadBundleId));
  builder = SetBuilderString(jni_env, builder, params.app_store_id,
                             dlink_ios_params_builder::GetMethodId(
                                 dlink_ios_params_builder::kSetAppStoreId));
  builder = SetBuilderString(jni_env, builder, params.minimum_version,
                             dlink_ios_params_builder::GetMethodId(
                                 dlink_ios_params_builder::kSetMinimumVersion));

  return BuildBuilder(
      jni_env, builder,
      dlink_ios_params_builder::GetMethodId(dlink_ios_params_builder::kBuild));
}

// You must call DeleteLocalRef on the returned object.
static jobject CreateItunesAnalyticsParameters(
    JNIEnv* jni_env, const ITunesConnectAnalyticsParameters& params) {
  jobject builder =
      jni_env->NewObject(dlink_itunes_params_builder::GetClass(),
                         dlink_itunes_params_builder::GetMethodId(
                             dlink_itunes_params_builder::kConstructor));

  builder =
      SetBuilderString(jni_env, builder, params.provider_token,
                       dlink_itunes_params_builder::GetMethodId(
                           dlink_itunes_params_builder::kSetProviderToken));
  builder =
      SetBuilderString(jni_env, builder, params.affiliate_token,
                       dlink_itunes_params_builder::GetMethodId(
                           dlink_itunes_params_builder::kSetAffiliateToken));
  builder =
      SetBuilderString(jni_env, builder, params.campaign_token,
                       dlink_itunes_params_builder::GetMethodId(
                           dlink_itunes_params_builder::kSetCampaignToken));

  return BuildBuilder(jni_env, builder,
                      dlink_itunes_params_builder::GetMethodId(
                          dlink_itunes_params_builder::kBuild));
}

// You must call DeleteLocalRef on the returned object.
static jobject CreateSocalMetaParameters(
    JNIEnv* jni_env, const SocialMetaTagParameters& params) {
  jobject builder =
      jni_env->NewObject(dlink_social_meta_params_builder::GetClass(),
                         dlink_social_meta_params_builder::GetMethodId(
                             dlink_social_meta_params_builder::kConstructor));

  builder = SetBuilderString(jni_env, builder, params.title,
                             dlink_social_meta_params_builder::GetMethodId(
                                 dlink_social_meta_params_builder::kSetTitle));
  builder =
      SetBuilderString(jni_env, builder, params.description,
                       dlink_social_meta_params_builder::GetMethodId(
                           dlink_social_meta_params_builder::kSetDescription));
  builder = SetBuilderUri(jni_env, builder, params.image_url,
                          dlink_social_meta_params_builder::GetMethodId(
                              dlink_social_meta_params_builder::kSetImageUrl));

  return BuildBuilder(jni_env, builder,
                      dlink_social_meta_params_builder::GetMethodId(
                          dlink_social_meta_params_builder::kBuild));
}

// You must call DeleteLocalRef on the returned object.
// if there is an error, error_out will be written and jobject will be nullptr.
static jobject PopulateLinkBuilder(JNIEnv* jni_env,
                                   const DynamicLinkComponents& components,
                                   std::string* error_out) {
  assert(error_out != nullptr);
  if (!components.link || !*components.link) {
    *error_out = "Link is missing.";
    return nullptr;
  }
  if (!components.domain_uri_prefix || !*components.domain_uri_prefix) {
    *error_out =
        "DynamicLinkComponents.domain_uri_prefix is required and cannot be "
        "empty.";
    return nullptr;
  }

  jobject link_builder = jni_env->CallObjectMethod(
      g_dynamic_links_class_instance,
      dynamic_links::GetMethodId(dynamic_links::kCreateDynamicLink));

  link_builder =
      SetBuilderUri(jni_env, link_builder, components.link,
                    dlink_builder::GetMethodId(dlink_builder::kSetLink));
  *error_out = util::GetAndClearExceptionMessage(jni_env);
  if (error_out->size()) {
    // setLink() threw an exception.
    jni_env->DeleteLocalRef(link_builder);
    return nullptr;
  }

  link_builder = SetBuilderString(
      jni_env, link_builder, components.domain_uri_prefix,
      dlink_builder::GetMethodId(dlink_builder::kSetDomainUriPrefix));
  *error_out = util::GetAndClearExceptionMessage(jni_env);
  if (error_out->size()) {
    // setDomainUriPrefix() threw an exception.
    jni_env->DeleteLocalRef(link_builder);
    return nullptr;
  }

  if (components.android_parameters) {
    jobject android_parameters_local = CreateAndroidParameters(
        jni_env, *components.android_parameters, error_out);
    if (!android_parameters_local) {
      // some required inputs were missing.
      jni_env->DeleteLocalRef(link_builder);
      return nullptr;
    }

    link_builder = SetBuilderObject(
        jni_env, link_builder, android_parameters_local,
        dlink_builder::GetMethodId(dlink_builder::kSetAndroidParameters));
    jni_env->DeleteLocalRef(android_parameters_local);
  }

  // GoogleAnalyticsParameters
  if (components.google_analytics_parameters) {
    jobject google_analytics_parameters_local = CreateGoogleAnalyticsParameters(
        jni_env, *components.google_analytics_parameters);
    // There no required parameters, so no exceptions or errors to find here.
    link_builder = SetBuilderObject(
        jni_env, link_builder, google_analytics_parameters_local,
        dlink_builder::GetMethodId(
            dlink_builder::kSetGoogleAnalyticsParameters));
    jni_env->DeleteLocalRef(google_analytics_parameters_local);
  }

  // IOSParameters
  if (components.ios_parameters) {
    jobject ios_parameters_local =
        CreateIOSParameters(jni_env, *components.ios_parameters, error_out);
    if (!ios_parameters_local) {
      // some required inputs were missing.
      jni_env->DeleteLocalRef(link_builder);
      return nullptr;
    }
    link_builder = SetBuilderObject(
        jni_env, link_builder, ios_parameters_local,
        dlink_builder::GetMethodId(dlink_builder::kSetIosParameters));
    jni_env->DeleteLocalRef(ios_parameters_local);
  }

  // ITunesConnectAnalyticsParameters
  if (components.itunes_connect_analytics_parameters) {
    jobject itunes_analytics_parameters_local = CreateItunesAnalyticsParameters(
        jni_env, *components.itunes_connect_analytics_parameters);
    // There no required parameters, so no exceptions or errors to find here.
    link_builder = SetBuilderObject(
        jni_env, link_builder, itunes_analytics_parameters_local,
        dlink_builder::GetMethodId(
            dlink_builder::kSetItunesConnectAnalyticsParameters));
    jni_env->DeleteLocalRef(itunes_analytics_parameters_local);
  }

  // SocialMetaTagParameters
  if (components.social_meta_tag_parameters) {
    jobject social_meta_parameters_local = CreateSocalMetaParameters(
        jni_env, *components.social_meta_tag_parameters);
    // There no required parameters, so no exceptions or errors to find here.
    link_builder = SetBuilderObject(
        jni_env, link_builder, social_meta_parameters_local,
        dlink_builder::GetMethodId(dlink_builder::kSetSocialMetaTagParameters));
    jni_env->DeleteLocalRef(social_meta_parameters_local);
  }

  return link_builder;
}

static jobject PopulateLinkBuilder(JNIEnv* jni_env, const char* long_link,
                                   std::string* error_out) {
  jobject link_builder = jni_env->CallObjectMethod(
      g_dynamic_links_class_instance,
      dynamic_links::GetMethodId(dynamic_links::kCreateDynamicLink));
  *error_out = util::GetAndClearExceptionMessage(jni_env);
  if (error_out->size()) {
    jni_env->DeleteLocalRef(link_builder);
    return nullptr;
  }

  link_builder =
      SetBuilderUri(jni_env, link_builder, long_link,
                    dlink_builder::GetMethodId(dlink_builder::kSetLongLink));

  return link_builder;
}

// Converts a `java.util.List<Warning>` to a `std::vector<std::string>`, where
// Warning contains 2 strings, one for the warning code and one for the message.
// These are concatenated together in the form "<code>: <message>".
void JavaWarningListToStdStringVector(JNIEnv* env,
                                      std::vector<std::string>* vector,
                                      jobject java_list_obj) {
  int size = env->CallIntMethod(java_list_obj,
                                util::list::GetMethodId(util::list::kSize));
  vector->clear();
  vector->reserve(size);
  for (int i = 0; i < size; i++) {
    jobject warning_element = env->CallObjectMethod(
        java_list_obj, util::list::GetMethodId(util::list::kGet), i);

    jobject code_string_local = env->CallObjectMethod(
        warning_element, short_dynamic_link_warning::GetMethodId(
                             short_dynamic_link_warning::kGetCode));
    jobject message_string_local = env->CallObjectMethod(
        warning_element, short_dynamic_link_warning::GetMethodId(
                             short_dynamic_link_warning::kGetMessage));

    env->DeleteLocalRef(warning_element);

    // these consume the local ref.
    std::string code = util::JniStringToString(env, code_string_local);
    std::string msg = util::JniStringToString(env, message_string_local);

    vector->push_back(code + ": " + msg);
  }
}

GeneratedDynamicLink GetLongLink(const DynamicLinkComponents& components) {
  GeneratedDynamicLink gen_link;
  FIREBASE_ASSERT_RETURN(gen_link, internal::IsInitialized());

  JNIEnv* jni_env = g_app->GetJNIEnv();
  jobject link_builder =
      PopulateLinkBuilder(jni_env, components, &gen_link.error);
  if (!link_builder) {
    return gen_link;
  }

  jobject dlink_local = jni_env->CallObjectMethod(
      link_builder,
      dlink_builder::GetMethodId(dlink_builder::kBuildDynamicLink));
  gen_link.error = util::GetAndClearExceptionMessage(jni_env);
  if (gen_link.error.size()) {
    jni_env->DeleteLocalRef(dlink_local);
    jni_env->DeleteLocalRef(link_builder);
    return gen_link;
  }

  jobject uri_local = jni_env->CallObjectMethod(
      dlink_local, dlink::GetMethodId(dlink::kGetUri));
  gen_link.error = util::GetAndClearExceptionMessage(jni_env);
  if (gen_link.error.size()) {
    jni_env->DeleteLocalRef(uri_local);
    jni_env->DeleteLocalRef(dlink_local);
    jni_env->DeleteLocalRef(link_builder);
    return gen_link;
  }

  gen_link.url = util::JniUriToString(jni_env, uri_local);

  jni_env->DeleteLocalRef(dlink_local);
  jni_env->DeleteLocalRef(link_builder);
  return gen_link;
}

static void FutureShortLinkCallback(JNIEnv* jni_env, jobject result,
                                    util::FutureResult result_code,
                                    const char* status_message,
                                    void* callback_data) {
  if (result_code == util::kFutureResultSuccess) {
    // There should only one type that this callback currently handles on
    // success.
    assert(jni_env->IsInstanceOf(result, short_dynamic_link::GetClass()));

    GeneratedDynamicLink result_link;

    jobject uri_local = jni_env->CallObjectMethod(
        result,
        short_dynamic_link::GetMethodId(short_dynamic_link::kGetShortLink));
    result_link.url = util::JniUriToString(jni_env, uri_local);

    jobject warnings_list_local = jni_env->CallObjectMethod(
        result,
        short_dynamic_link::GetMethodId(short_dynamic_link::kGetWarnings));
    if (warnings_list_local != nullptr) {
      // TODO(butterfield): NOTE: Currently the Java code does not return any
      // warnings, so this code is untested!
      JavaWarningListToStdStringVector(jni_env, &result_link.warnings,
                                       warnings_list_local);
      jni_env->DeleteLocalRef(warnings_list_local);
    }

    FutureData* future_data = FutureData::Get();
    if (future_data) {
      future_data->api()->CompleteWithResult(
          FutureHandle(reinterpret_cast<FutureHandleId>(callback_data)),
          kErrorCodeSuccess, result_link);
    }
  } else {  // result_code != Success
    GeneratedDynamicLink result_link;
    FutureData* future_data = FutureData::Get();
    if (future_data) {
      result_link.error = status_message;
      future_data->api()->CompleteWithResult(
          FutureHandle(reinterpret_cast<FutureHandleId>(callback_data)),
          kErrorCodeFailed, status_message, result_link);
    }
  }
}

static jint GetSuffixOption(const PathLength& path_length) {
  for (int i = 0; i < FIREBASE_ARRAYSIZE(g_path_length_codes); ++i) {
    if (g_path_length_codes[i].path_length_code == path_length)
      return g_path_length_codes[i].value;
  }
  // Couldn't find the value in the map, must be default.
  return kPathLengthDefault;
}

// Common code for short links to set up the callback to handle the result.
static Future<GeneratedDynamicLink> HandleShortLinkTask(
    JNIEnv* jni_env, jobject link_builder,
    const DynamicLinkOptions& dynamic_link_options, const std::string& error) {
  ReferenceCountedFutureImpl* api = FutureData::Get()->api();
  const SafeFutureHandle<GeneratedDynamicLink> handle =
      api->SafeAlloc<GeneratedDynamicLink>(kDynamicLinksFnGetShortLink);

  if (!link_builder) {
    GeneratedDynamicLink gen_link;
    gen_link.error = error;
    api->CompleteWithResult(handle, kErrorCodeFailed, error.c_str(), gen_link);
    return MakeFuture(api, handle);
  }
  jobject task;
  if (dynamic_link_options.path_length != kPathLengthDefault) {
    task = jni_env->CallObjectMethod(
        link_builder,
        dlink_builder::GetMethodId(
            dlink_builder::kBuildShortDynamicLinkWithOption),
        GetSuffixOption(dynamic_link_options.path_length));
  } else {
    task = jni_env->CallObjectMethod(
        link_builder,
        dlink_builder::GetMethodId(dlink_builder::kBuildShortDynamicLink));
  }

  std::string exception_message = util::GetAndClearExceptionMessage(jni_env);
  if (exception_message.size()) {
    GeneratedDynamicLink gen_link;
    gen_link.error = exception_message;
    LogError("Couldn't build short link: %s", exception_message.c_str());
    api->CompleteWithResult(handle, kErrorCodeFailed, exception_message.c_str(),
                            gen_link);
  } else {
    util::RegisterCallbackOnTask(
        jni_env, task, FutureShortLinkCallback,
        reinterpret_cast<void*>(handle.get().id()), kApiIdentifier);
  }

  jni_env->DeleteLocalRef(link_builder);
  jni_env->DeleteLocalRef(task);
  return MakeFuture(api, handle);
}

Future<GeneratedDynamicLink> GetShortLink(
    const DynamicLinkComponents& components,
    const DynamicLinkOptions& options) {
  FIREBASE_ASSERT_RETURN(Future<GeneratedDynamicLink>(),
                         internal::IsInitialized());
  JNIEnv* jni_env = g_app->GetJNIEnv();

  // Temporary workaround: Get short link from long link rather than from
  // components. TODO(b/113628371): remove this when the "Error 8" bug is fixed.
  //
  // First, get the long link. If that returns an error, pass that error and
  // a null link builder to HandleShortLinkTask(), which will return a Future
  // that propagates the error to the caller.
  //
  // If there was no error getting the long link, PopulateLinkBuilder handles
  // getting the short link the same way that GetShortLink(long_link) does.
  GeneratedDynamicLink long_link = GetLongLink(components);
  std::string error = long_link.error;
  jobject link_builder = nullptr;
  if (error.empty()) {
    link_builder = PopulateLinkBuilder(jni_env, long_link.url.c_str(), &error);
  }
  return HandleShortLinkTask(jni_env, link_builder, options, error);
}

Future<GeneratedDynamicLink> GetShortLink(
    const DynamicLinkComponents& components) {
  return GetShortLink(components, DynamicLinkOptions());
}

Future<GeneratedDynamicLink> GetShortLink(const char* long_dynamic_link,
                                          const DynamicLinkOptions& options) {
  FIREBASE_ASSERT_RETURN(Future<GeneratedDynamicLink>(),
                         internal::IsInitialized());
  JNIEnv* jni_env = g_app->GetJNIEnv();
  std::string error;
  jobject link_builder =
      PopulateLinkBuilder(jni_env, long_dynamic_link, &error);
  return HandleShortLinkTask(jni_env, link_builder, options, error);
}

Future<GeneratedDynamicLink> GetShortLink(const char* long_dynamic_link) {
  return GetShortLink(long_dynamic_link, DynamicLinkOptions());
}

Future<GeneratedDynamicLink> GetShortLinkLastResult() {
  FIREBASE_ASSERT_RETURN(Future<GeneratedDynamicLink>(),
                         internal::IsInitialized());
  ReferenceCountedFutureImpl* api = FutureData::Get()->api();
  return static_cast<const Future<GeneratedDynamicLink>&>(
      api->LastResult(kDynamicLinksFnGetShortLink));
}

}  // namespace dynamic_links
}  // namespace firebase
