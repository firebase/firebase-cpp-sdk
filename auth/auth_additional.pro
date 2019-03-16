# Additional ProGuard rules needed for the Auth library.
# Keep Retrofit annotations, needed for IdentityToolkitHttpApi.
# For some reason the Identity library doesn't do this for us.
-keepattributes *Annotation*
-keep class retrofit.** { *; }
-keepclasseswithmembers class * { @retrofit.http.* <methods>; }
-keepattributes Signature
