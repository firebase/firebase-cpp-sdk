#include "firestore/src/jni/declaration.h"

#include "app/memory/unique_ptr.h"
#include "app/src/util_android.h"
#include "firestore/src/jni/env.h"
#include "firestore/src/jni/hash_map.h"
#include "firestore/src/jni/iterator.h"
#include "firestore/src/jni/loader.h"
#include "firestore/src/jni/set.h"
#include "firestore/src/tests/firestore_integration_test.h"
#include "testing/base/public/gmock.h"
#include "gtest/gtest.h"

namespace firebase {
namespace firestore {
namespace jni {
namespace {

using testing::UnorderedElementsAre;

constexpr char kString[] = "java/lang/String";
Method<String> kToLowerCase("toLowerCase", "()Ljava/lang/String;");
StaticField<Object> kCaseInsensitiveOrder("CASE_INSENSITIVE_ORDER",
                                          "Ljava/util/Comparator;");
StaticMethod<String> kValueOfInt("valueOf", "(I)Ljava/lang/String;");

constexpr char kInteger[] = "java/lang/Integer";
Constructor<Object> kNewInteger("(I)V");

class DeclarationTest : public FirestoreIntegrationTest {
 public:
  DeclarationTest() : loader_(app()) { loader_.LoadClass(kString); }

 protected:
  Loader loader_;
};

TEST_F(DeclarationTest, TypesAreTriviallyDestructible) {
  static_assert(std::is_trivially_destructible<Constructor<Object>>::value,
                "Constructor is trivially destructible");
  static_assert(std::is_trivially_destructible<Method<Object>>::value,
                "Method is trivially destructible");
  static_assert(std::is_trivially_destructible<StaticField<Object>>::value,
                "StaticField is trivially destructible");
  static_assert(std::is_trivially_destructible<StaticMethod<Object>>::value,
                "StaticMethod is trivially destructible");
}

TEST_F(DeclarationTest, ConstructsObjects) {
  loader_.LoadClass(kInteger);
  loader_.Load(kNewInteger);
  ASSERT_TRUE(loader_.ok());

  Env env;
  Local<Object> result = env.New(kNewInteger, 42);
  EXPECT_EQ("42", result.ToString(env));
}

TEST_F(DeclarationTest, CallsObjectMethods) {
  loader_.Load(kToLowerCase);
  ASSERT_TRUE(loader_.ok());

  Env env;
  Local<String> str = env.NewStringUtf("Foo");

  Local<String> result = env.Call(str, kToLowerCase);
  EXPECT_EQ("foo", result.ToString(env));
}

TEST_F(DeclarationTest, GetsStaticFields) {
  loader_.Load(kCaseInsensitiveOrder);

  const char* kComparator = "java/util/Comparator";
  Method<int32_t> kCompare("compare",
                           "(Ljava/lang/Object;Ljava/lang/Object;)I");
  loader_.LoadClass(kComparator);
  loader_.Load(kCompare);
  ASSERT_TRUE(loader_.ok());

  Env env;
  Local<Object> ordering = env.Get(kCaseInsensitiveOrder);
  EXPECT_NE(ordering.get(), nullptr);

  Local<String> uppercase = env.NewStringUtf("GOO");
  Local<String> lowercase = env.NewStringUtf("foo");
  EXPECT_EQ(0, env.Call(ordering, kCompare, uppercase, uppercase));
  EXPECT_EQ(1, env.Call(ordering, kCompare, uppercase, lowercase));
  EXPECT_EQ(-1, env.Call(ordering, kCompare, lowercase, uppercase));
}

TEST_F(DeclarationTest, CallsStaticObjectMethods) {
  loader_.Load(kValueOfInt);

  Env env;
  Local<String> result = env.Call(kValueOfInt, 42);
  EXPECT_EQ("42", result.ToString(env));
}

TEST_F(DeclarationTest, CanUseUnownedClasses) {
  Constructor<Object> ctor("()V");
  Method<bool> add_method("add", "(Ljava/lang/Object;)Z");
  Method<size_t> size_method("size", "()I");

  loader_.LoadFromExistingClass("java/util/ArrayList",
                                util::array_list::GetClass(), ctor, add_method,
                                size_method);

  Env env;
  Local<String> str = env.NewStringUtf("foo");
  Local<Object> list = env.New(ctor);
  EXPECT_TRUE(env.Call(list, add_method, str));
  EXPECT_EQ(1u, env.Call(list, size_method));
}

TEST_F(DeclarationTest, CanUseJavaCollections) {
  Env env;
  Local<String> key1 = env.NewStringUtf("key1");
  Local<String> key2 = env.NewStringUtf("key2");

  Local<HashMap> map = HashMap::Create(env);
  map.Put(env, key1, key1);
  map.Put(env, key2, key2);

  std::vector<std::string> actual_keys;
  Local<Iterator> iter = map.KeySet(env).Iterator(env);
  while (iter.HasNext(env)) {
    Local<Object> key = iter.Next(env);
    actual_keys.push_back(key.ToString(env));
  }

  EXPECT_THAT(actual_keys, UnorderedElementsAre("key1", "key2"));
}

}  // namespace
}  // namespace jni
}  // namespace firestore
}  // namespace firebase
