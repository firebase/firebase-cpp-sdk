# Copyright 2016 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Convert Scion Objective-C header consisting of constants to C++."""

import os
import re
import subprocess

from absl import app
from absl import flags
import generate_constants_lib

FLAGS = flags.FLAGS

# Required args
flags.DEFINE_string('objc_header', '', 'Objective-C header file containing a '
                    'set of constants to convert to C++.')
flags.register_validator(
    'objc_header',
    lambda x: x and os.path.exists(x),
    message=('Must reference an existing Objective-C '
             'header file'))
flags.DEFINE_string('cpp_header', '', 'Name of the C++ header file to write '
                    'out.')
flags.register_validator(
    'cpp_header', lambda x: x, message='Output c++ header must be specified')
# optional args
flags.DEFINE_string('clang_format', '', 'Path to the clang format executable '
                    'used to format the generated C++ header files.  If this '
                    'is empty, clang-format will not be executed')

# Prefix added to generated header guards.
HEADER_GUARD_PREFIX = (
    'FIREBASE_ANALYTICS_CLIENT_CPP_INCLUDE_FIREBASE_ANALYTICS_')

# Namespace for the generated header.
NAMESPACE_DOCS_LIST = [
    ('firebase', 'Namespace that encompasses all Firebase APIs.'),
    ('analytics', 'Firebase Analytics API.')
]

# Matches static const Objective-C string.
STRING_CONSTANT_LINE_REPLACEMENTS = [
    (r'NSString *\*', r'const char*'),
    (r'kFIR([^ =]*)', r'k\1'),
    (r'@"([^"]*)"', r'"\1"'),
    (r'NS_SWIFT_NAME\(.*\)', r''),
    (r'FIR_SWIFT_NAME\(.*\)', r'')
]

# Replacements to apply to doc strings (see DocStringParser.replacements).
DOC_STRING_LINE_REPLACEMENTS = [
    ('kFIR', 'k'),
    ('NSString', 'string'),
    (r'@file (.*)\.h', r'@defgroup \1'),
    (r'(@defgroup parameter_names)', r'\1 Analytics Parameters'),
    (r'(@defgroup event_names)', r'\1 Analytics Events'),
    (r'(@defgroup user_property_names)', r'\1 Analytics User Properties'),
    (r'@"([^"]*)"', r'"\1"'),
    (r'@\(([0-9.]*)\)', r'\1'),
    (r' *NSDictionary \*params *= *@{',
     ' using namespace firebase::analytics;\n'
     '/// Parameter parameters[] = {'),
    (r'(k[^: ]*)[^:]*: *(.*),', r'Parameter(\1, \2),'),
    (r'\((kParameter)', r'(\1'),
    (r'(?i)<pre[^>]*>', r'@code'),
    (r'(?i)</pre>', r'@endcode'),
    (r'^///     ', '///  '),
]

DOC_STRING_PARAGRAPH_REPLACEMENTS = [
    ('  *as NSNumber  *', ' '),
    (' *as NSNumber *', ''),
    (r'\{@link *([^}]*)\}', r'@ref \1'),
]

DOC_STRING_GLOBAL_REPLACEMENTS = [
    # Expand the C++ sample code into two examples, C++ and C#.
    (r'(?s)(@code)([^{].*using namespace firebase::analytics;'
     r'.*Parameter [^{]+[^}]+}.*@endcode)',
     '\n'
     '///\n'
     '/// @if cpp_examples\n'
     '/// \\1{.cpp}'
     '\\2\n\n'
     '/// @endif\n'
     '/// <SWIG>\n'
     '/// @if swig_examples\n'
     '/// \\1{.cs}'
     '\\2\n\n'
     '/// @endif\n'
     '/// </SWIG>\n'),
    # Change the C# example to use the Firebase.Analytics namespace.
    (r'(?s)(@if swig_examples.*using)( namespace firebase::analytics)',
     r'\1 Firebase.Analytics'),
    # Fix the C# example to create new Parameter instances in the array.
    (r'(?s)(@if swig_examples.*)((?<!new) Parameter\()',
     r'\1new\2'),
    # Change the C# example to use properties rather than constants.
    (r'(?s)(@if swig_examples.*)(kParameter)',
     r'\1FirebaseAnalytics.Parameter'),
]


def main(unused_argv):
  """Convert the specified Objective-C header to C++."""
  cpp_header_basename = os.path.basename(FLAGS.cpp_header)
  replacements = list(DOC_STRING_LINE_REPLACEMENTS)
  replacements.insert(0, (r'FIR.*\.h', cpp_header_basename))
  parser = generate_constants_lib.DocStringParser(
      replacements=replacements,
      paragraph_replacements=DOC_STRING_PARAGRAPH_REPLACEMENTS,
      global_replacements=DOC_STRING_GLOBAL_REPLACEMENTS)
  with open(FLAGS.objc_header) as input_file:
    with open(FLAGS.cpp_header, 'wt') as output_file:
      output_file.write(generate_constants_lib.format_cpp_header_header(
          HEADER_GUARD_PREFIX, cpp_header_basename, NAMESPACE_DOCS_LIST))
      for line in input_file:
        line = line.rstrip()
        if not line:
          doc_string = parser.get_doc_string()
          output_file.write(doc_string)
          parser.reset()
          # If the doc string defines a group, include all following items in
          # the group.
          if '@defgroup' in doc_string:
            output_file.write('/// @{\n')
          output_file.write('\n')
          continue
        if parser.parse_line(line):
          continue

        # Apply replacements to non-comment lines.
        replaced_line = False
        for replace_from, replace_to in STRING_CONSTANT_LINE_REPLACEMENTS:
          if re.search(replace_from, line):
            line = re.sub(replace_from, replace_to, line)
            replaced_line = True

        if replaced_line:
          output_file.write(parser.get_doc_string())
          parser.reset()
          output_file.write(line + '\n')

      output_file.write(parser.get_doc_string())
      parser.reset()
      output_file.write('/// @}\n')
      output_file.write(generate_constants_lib.format_cpp_header_footer(
          HEADER_GUARD_PREFIX, cpp_header_basename,
          [ns[0] for ns in NAMESPACE_DOCS_LIST]))
  if FLAGS.clang_format:
    # NOTE: Comment reflow is disabled here to preserve <pre> delimited code
    # blocks.  Comments have already been wrapped by DocStringParser.
    subprocess.check_call([FLAGS.clang_format, '-i',
                           '-style', '{ReflowComments: false}',
                           FLAGS.cpp_header])

  return 0

if __name__ == '__main__':
  app.run(main)
