#include "firebase/app.h"
#include "firebase/app_check/firebase_app_check.h"

// Create a custom AppCheck provider.

class YourCustomAppCheckProvider
    : public ::firebase::app_check::AppCheckProvider {
 public:
  void GetToken(std::function<void(AppCheckToken, int, string)>
                    completion_callback) override;
}

void YourCustomAppCheckProvider::GetToken(
  std::function<void(AppCheckToken, int, string)> completion_callback) {
  // Logic to exchange proof of authenticity for an App Check token and
  //   expiration time.
  // ...

  // Refresh the token early to handle clock skew.
  int64_t exp_millis = expiration_from_server * 1000 - 60000;

  // Create an AppCheckToken struct.
  ::firebase::app_check::AppCheckToken app_check_token(token_from_server,
                                                       exp_millis);
  // Call the callback with either a token or an error code and error message.
  completion_callback(app_check_token, 0, "");
}

// Create a factory for a custom provider.

class YourCustomAppCheckProviderFactory
    : public ::firebase::app_check::AppCheckProviderFactory {
 public:
  static YourCustomAppCheckProviderFactory GetInstance();

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
    ::firebase::app_check::AppCheck.GetInstance();

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
app_check->AddAppCheckListener(&app_check_listener);
