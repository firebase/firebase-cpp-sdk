// Copyright 2020 Google LLC

#ifndef FIREBASE_FIRESTORE_SRC_JNI_JNI_FWD_H_
#define FIREBASE_FIRESTORE_SRC_JNI_JNI_FWD_H_

#include <jni.h>

namespace firebase::firestore::jni {

JNIEnv* GetEnv();
JNIEnv* GetAttachedEnv();

class Env;
class Loader;

template <typename T> class Local;
template <typename T> class Global;
template <typename T> class Array;

class Class;
class Object;
class String;
class Throwable;

class ConstructorBase;
class MethodBase;
class StaticFieldBase;
class StaticMethodBase;

template <typename T> class Constructor;
template <typename T> class Method;
template <typename T> class StaticField;
template <typename T> class StaticMethod;

class Boolean;
class Double;
class Integer;
class Long;

class ArrayList;
class Collection;
class Iterator;
class List;
class HashMap;
class Map;
class Set;

}  // namespace firebase::firestore::jni

#endif  // FIREBASE_FIRESTORE_SRC_JNI_JNI_FWD_H_
