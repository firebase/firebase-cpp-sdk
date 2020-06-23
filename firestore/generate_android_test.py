#!/usr/grte/v4/bin/python2.7
"""Generate JUnit4 tests from gtest files.

This script reads a template and fills in test-specific information such as .so
library name and Java class name. This script also goes over the gtest files and
finds all test methods of the pattern TEST_F(..., ...) and converts each into a
@Test-annotated test method.
"""

# We will be open-source this. So please do not introduce google3 dependency
# unless absolutely necessary.

import argparse
import re

GTEST_METHOD_RE = (r'TEST_F[(]\s*(?P<test_class>[A-Za-z]+)\s*,\s*'
                   r'(?P<test_method>[A-Za-z]+)\s*[)]')

JAVA_TEST_METHOD = r"""
  @Test
  public void {test_class}{test_method}() {{
    run("{test_class}.{test_method}");
  }}
"""


def generate_fragment(gtests):
  """Generate @Test-annotated test method code from the provided gtest files."""
  fragments = []
  gtest_method_pattern = re.compile(GTEST_METHOD_RE)
  for gtest in gtests:
    with open(gtest, 'r') as gtest_file:
      gtest_code = gtest_file.read()
      for matched in re.finditer(gtest_method_pattern, gtest_code):
        fragments.append(
            JAVA_TEST_METHOD.format(
                test_class=matched.group('test_class'),
                test_method=matched.group('test_method')))
  return ''.join(fragments)


def generate_file(template, out, **kwargs):
  """Generate a Java file from the provided template and parameters."""
  with open(template, 'r') as template_file:
    template_string = template_file.read()
    java_code = template_string.format(**kwargs)
    with open(out, 'w') as out_file:
      out_file.write(java_code)


def main():
  parser = argparse.ArgumentParser(
      description='Generates JUnit4 tests from gtest files.')
  parser.add_argument(
      '--template',
      help='the filename of the template to use in the generation',
      required=True)
  parser.add_argument(
      '--java_package',
      help='which package test Java class belongs to',
      required=True)
  parser.add_argument(
      '--java_class',
      help='specifies the name of the class to generate',
      required=True)
  parser.add_argument(
      '--so_lib',
      help=('specifies the name of the native library without prefix lib and '
            'suffix .so. You must compile the C++ test code together with the '
            'firestore_android_test_main.cc as a shared library, say libfoo.so '
            'and pass the name foo here.'),
      required=True)
  parser.add_argument('--out', help='the output file path', required=True)
  parser.add_argument('srcs', nargs='+', help='the input gtest file paths')
  args = parser.parse_args()

  fragment = generate_fragment(args.srcs)
  generate_file(
      args.template,
      args.out,
      package_name=args.java_package,
      java_class_name=args.java_class,
      so_lib_name=args.so_lib,
      tests=fragment)


if __name__ == '__main__':
  main()
