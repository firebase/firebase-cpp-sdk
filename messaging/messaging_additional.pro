# Additional Proguard rules for Messaging.
-keep,includedescriptorclasses public class com.google.firebase.messaging.MessageForwardingService { *; }
-keep,includedescriptorclasses public class com.google.firebase.messaging.cpp.DebugLogging { *; }
-keep,includedescriptorclasses public class com.google.firebase.messaging.cpp.ListenerService { *; }
-keep,includedescriptorclasses public class com.google.firebase.messaging.cpp.MessageWriter { *; }
-keep,includedescriptorclasses public class com.google.firebase.messaging.cpp.RegistrationIntentService { *; }
