#include "messaging/src/common.h"
#include "messaging/src/include/firebase/messaging.h"
namespace firebase::messaging {
Listener::~Listener() = default;
void Listener::OnTokenReceived([[maybe_unused]] const char* token) {}
void Listener::OnRegistrationReceived([[maybe_unused]] const char* installationId) {}
void Listener::OnUnregistrationReceived([[maybe_unused]] const char* installationId) {}
}  // namespace firebase::messaging
