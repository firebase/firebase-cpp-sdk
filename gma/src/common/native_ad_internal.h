#ifndef THIRD_PARTY_FIREBASE_CPP_GMA_SRC_COMMON_NATIVE_AD_INTERNAL_H_
#define THIRD_PARTY_FIREBASE_CPP_GMA_SRC_COMMON_NATIVE_AD_INTERNAL_H_

#include "app/src/include/firebase/future.h"
#include "app/src/include/firebase/internal/mutex.h"
#include "gma/src/common/gma_common.h"
#include "gma/src/include/firebase/gma/internal/native_ad.h"

namespace firebase {
namespace gma {
namespace internal {

  // Constants representing each NativeAd function that returns a Future.
enum NativeAdFn {
  kNativeAdFnInitialize,
  kNativeAdFnLoadAd,
  kNativeAdFnCount
};

class NativeAdInternal {
 public:
  // Create an instance of whichever subclass of NativeAdInternal is
  // appropriate for the current platform.
  static NativeAdInternal* CreateInstance(NativeAd* base);

  // Virtual destructor is required.
  virtual ~NativeAdInternal() = default;

  // Initializes this object and any platform-specific helpers that it uses.
  virtual Future<void> Initialize(AdParent parent) = 0;

  // Initiates an ad request.
  virtual Future<AdResult> LoadAd(const char* ad_unit_id,
                                  const AdRequest& request) = 0;

  // Retrieves the most recent Future for a given function.
  Future<void> GetLastResult(NativeAdFn fn);

  // Retrieves the most recent AdResult future for the LoadAd function.
  Future<AdResult> GetLoadAdLastResult();

  // Returns true if the NativeAd has been initialized.
  virtual bool is_initialized() const = 0;

 protected:
  friend class firebase::gma::NativeAd;

  // Used by CreateInstance() to create an appropriate one for the current
  // platform.
  explicit NativeAdInternal(NativeAd* base);

  // A pointer back to the NativeAd class that created us.
  NativeAd* base_;

  // Future data used to synchronize asynchronous calls.
  FutureData future_data_;
};

}  // namespace internal
}  // namespace gma
}  // namespace firebase

#endif  // THIRD_PARTY_FIREBASE_CPP_GMA_SRC_COMMON_NATIVE_AD_INTERNAL_H_
