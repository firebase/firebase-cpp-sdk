#include "firebase/app.h"
#include "firebase/app_check/firebase_app_check.h"

// Create a custom AppCheck provider.

class YourCustomAppCheckProvider
    : public ::firebase::app_check::AppCheckProvider {
 public:
  Future<::firebase::app_check::AppCheckToken> GetToken() override;
}

Future<::firebase::app_check::AppCheckToken>
YourCustomAppCheckProvider::GetToken() {
  // Logic to exchange proof of authenticity for an App Check token and
  //   expiration time.
  // ...

  // Refresh the token early to handle clock skew.
  int64_t exp_millis = expiration_from_server * 1000 - 60000;

  // Create and return AppCheckToken struct.
  ::firebase::app_check::AppCheckToken app_check_token(token_from_server,
                                                       exp_millis);
  return app_check_token;
}

// Create a factory for a custom provider.

class YourCustomAppCheckProviderFactory
    : public ::firebase::app_check::AppCheckProviderFactory {
 public:
  static DebugAppCheckProviderFactory GetInstance();

  ::firebase::app_check::AppCheckProvider* CreateProvider(
      const ::firebase::App& app) override;

}

::firebase::app_check::AppCheckProvider*
YourCustomAppCheckProviderFactory::CreateProvider(const ::firebase::App& app) {
  // Create and return an AppCheckProvider object.
  return new YourCustomAppCheckProvider(app);
}

// Initialize App Check (with a given provider factory)

// Note: SetAppCheckProviderFactory must be called before App::Create()
// to be compatible with iOS

::firebase::app_check::AppCheck.SetAppCheckProviderFactory(
    YourCustomAppCheckProviderFactory.GetInstance());
::firebase::App* app = ::firebase::App::Create();
::firebase::app_check::AppCheck* app_check =
    ::firebase::app_check::AppCheck.getInstance();

// Add a listener for token changes.

class MyAppCheckListener : public ::firebase::app_check::AppCheckListener {
 public:
  void OnAppCheckTokenChanged(
      const ::firebase::app_check::AppCheckToken& token) override {
    // Use the token to authorize requests to non-firebase backends.
    // ...
  }
};

MyAppCheckListener app_check_listener;
app_check->addAppCheckListener(&state_change_listener);
