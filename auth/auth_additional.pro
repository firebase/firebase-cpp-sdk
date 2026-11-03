# Additional ProGuard rules needed for the Auth library.
-keep class com.google.firebase.auth.internal.cpp.** { *; }
-keepattributes *Annotation*,Signature
