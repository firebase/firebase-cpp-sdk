# Begin additional Proguard rules for App.
# Keep these classes because we only access them via reflection in TaskCallback.
-keep,includedescriptorclasses public class com.google.android.gms.tasks.OnFailureListener { *; }
-keep,includedescriptorclasses public class com.google.android.gms.tasks.OnSuccessListener { *; }
-keep,includedescriptorclasses public class com.google.android.gms.tasks.Task { *; }
# Keep these classes because GMS is currently broken.
-keep,includedescriptorclasses public class com.google.android.gms.crash.internal.api.CrashApiImpl { *; }
# End additional Proguard rules for App.
