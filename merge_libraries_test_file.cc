/*
 * Copyright 2020 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/* This is a test file for merge_libraries.py tests. It contains some C and C++
 * symbols that merge_libraries can rename. */

#include <memory>
#include <string>
#include <utility>
#include <vector>

extern "C" {
int test_symbol(void) { return 1; }  // NOLINT

int test_another_symbol(void) { return 2; }  // NOLINT

int test_yet_one_more_symbol(void) { return 3; }  // NOLINT

int global_c_symbol = 789;  // NOLINT

extern int not_in_this_file();  // NOLINT
}

namespace test_namespace {

class TestClass {
 public:
  TestClass();
  TestClass(const TestClass&);   // not in this file
  TestClass(const TestClass&&);  // not in this file
  ~TestClass();                  // not in this file
  int TestMethod();
  int TestMethodNotInThisfile();
  static int TestStaticMethod();
  static int TestStaticMethodNotInThisFile();
  static int test_static_field;                   // NOLINT
  static int test_static_field_not_in_this_file;  // NOLINT
};

int global_cpp_symbol = 12345;  // NOLINT

int TestClass::test_static_field;

TestClass::TestClass() {}

int TestClass::TestMethod() {
  return TestMethodNotInThisfile() + not_in_this_file();
}

int TestClass::TestStaticMethod() { return TestStaticMethodNotInThisFile(); }

}  // namespace test_namespace

void GlobalFunctionWithParameter(test_namespace::TestClass const&, int) {}

void GlobalFunctionWithMultipleParameters(
    test_namespace::TestClass* p1, std::vector<test_namespace::TestClass> p2,
    std::unique_ptr<test_namespace::TestClass> p3,
    std::vector<std::unique_ptr<test_namespace::TestClass>> p4, std::string) {
  p2.push_back(*p1);
  p2.pop_back();
  p4.push_back(std::move(p3));
  p4.pop_back();
}

extern void ExternFunctionWithParameter(test_namespace::TestClass&&, int);

extern void ExternFunctionWithMultipleParameters(
    const test_namespace::TestClass&,
    std::unique_ptr<test_namespace::TestClass>, std::string);

namespace another_namespace {

extern void ExternFunctionNotUsingNamespace(std::string);

}  // namespace another_namespace
