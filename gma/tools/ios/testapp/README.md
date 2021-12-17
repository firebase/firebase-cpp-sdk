Firebase AdMob iOS Test App
===========================

The Firebase AdMob iOS test app is designed to enable implementing, modifying,
and debugging API features directly in Xcode.

Getting Started
---------------

- Get the code:

        git5 init
        git5-track-p4-depot-paths //depot_firebase_cpp/admob/client/cpp/tools/ios/testapp/p4_depot_paths
        git5 sync

- Create the following symlinks (DO NOT check these in to google3 -- they should be added to your
  .gitignore):

    NOTE: Firebase changed their includes from `include` to `src/include`.
    These soft links work around the issue.

        GOOGLE3_PATH=~/path/to/git5/repo/google3 # Change to your google3 path
        ln -s $GOOGLE3_PATH/firebase/app/client/cpp/src/include/ $GOOGLE3_PATH/firebase/app/client/cpp/include
        ln -s $GOOGLE3_PATH/firebase/admob/client/cpp/src/include/ $GOOGLE3_PATH/firebase/admob/client/cpp/include

Setting up the App
------------------

- In Project Navigator, add the GoogleMobileAds.framework to the Frameworks
  testapp project.
- Update the following files:
   - google3/firebase/admob/client/cpp/src/common/admob_common.cc
     - Comment out the following code:

                /*
                FIREBASE_APP_REGISTER_CALLBACKS(admob,
                                                {
                                                  if (app == ::firebase::App::GetInstance()) {
                                                    return firebase::admob::Initialize(*app);
                                                  }
                                                  return kInitResultSuccess;
                                                },
                                                {
                                                  if (app == ::firebase::App::GetInstance()) {
                                                    firebase::admob::Terminate();
                                                  }
                                                });
                */

   - google3/firebase/admob/client/cpp/src/include/firebase/admob.h
     - Comment out the following code:

                /*
                #if !defined(DOXYGEN) && !defined(SWIG)
                FIREBASE_APP_REGISTER_CALLBACKS_REFERENCE(admob)
                #endif  // !defined(DOXYGEN) && !defined(SWIG)
                */
