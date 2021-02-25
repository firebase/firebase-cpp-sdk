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

"""Tests for generate_constants_lib.py."""

import datetime

from google3.testing.pybase import googletest
from google3.firebase.analytics.client.cpp import generate_constants_lib


class GenerateHeaderTest(googletest.TestCase):
  """Tests functions used to generate C++ header boilerplate."""

  def test_cpp_header_guard(self):
    """Verify header guards are formatted correctly."""
    self.assertEqual(
        'SOME_API_CPP_MYAPI_H_',
        generate_constants_lib.cpp_header_guard('SOME_API_CPP_', 'myapi'))

  def test_format_cpp_header_header(self):
    """Verify the header of C++ headers are formatted correctly."""
    self.assertEqual(
        '// Copyright %s Google Inc. All Rights Reserved.\n'
        '\n'
        '#ifndef SOME_API_CPP_MYAPI_H_\n'
        '#define SOME_API_CPP_MYAPI_H_\n'
        '\n'
        '/// @brief my package docs\n'
        'namespace mypackage {\n'
        '/// @brief my api docs\n'
        'namespace myapi {\n'
        '\n' % str(datetime.date.today().year),
        generate_constants_lib.format_cpp_header_header(
            'SOME_API_CPP_', 'myapi.h', [('mypackage', 'my package docs'),
                                         ('myapi', 'my api docs')]))

  def test_format_cpp_header_footer(self):
    """Verify the footer of C++ headers are formatted correctly."""
    self.assertEqual(
        '\n'
        '}  // namespace myapi\n'
        '}  // namespace mypackage\n'
        '\n'
        '#endif  // SOME_API_CPP_MYAPI_H_\n',
        generate_constants_lib.format_cpp_header_footer('SOME_API_CPP_',
                                                        'myapi.h',
                                                        ['mypackage', 'myapi']))


class DocStringParserTest(googletest.TestCase):
  """Tests for DocStringParser."""

  def test_parse_line(self):
    """Test successfully parsing a line."""
    parser = generate_constants_lib.DocStringParser()
    doc_line = '/// This is a test'
    self.assertTrue(parser.parse_line(doc_line))
    self.assertListEqual([doc_line], parser.doc_string_lines)

  def test_parse_line_no_docs(self):
    """Verify lines that don't contain docs are not parsed."""
    parser = generate_constants_lib.DocStringParser()
    self.assertFalse(parser.parse_line(
        'static NSString *const test = @"test";'))
    self.assertListEqual([], parser.doc_string_lines)

  def test_reset(self):
    """Verify it's possible to reset the state of the parser."""
    parser = generate_constants_lib.DocStringParser()
    self.assertTrue(parser.parse_line('/// This is a test'))
    parser.reset()
    self.assertListEqual([], parser.doc_string_lines)

  def test_apply_replacements(self):
    """Test transformation of parsed doc strings."""
    parser = generate_constants_lib.DocStringParser(replacements=(
        ('kT.XBish', 'kBish'), ('Bosh', ''), ('yo', 'hey')))
    self.assertEqual(
        '/// This is a test of kBish',
        generate_constants_lib.DocStringParser.apply_replacements(
            '/// This is a test of kTTXBishBosh',
            parser.replacements))

    self.assertEqual(
        '/// This is a hey of kBish hey',
        generate_constants_lib.DocStringParser.apply_replacements(
            '/// This is a yo of kTTXBishBosh yo',
            parser.replacements,
            replace_multiple_times=True))

  def test_wrap_lines(self):
    """Test line wrapping of parsed doc strings."""
    parser = generate_constants_lib.DocStringParser()
    wrapped_lines = parser.wrap_lines(
        ['/// this is a short paragraph',
         '///',
         '/// this is a' + (' very long line' * 10),
         '///',
         '/// more content',
         '/// and some html that should not be wrapped',
         '/// <li>',
         '///    <ul>some important stuff</ul>',
         '/// </li>',
         '/// <pre>',
         '///   int some_code = "that should not"',
         '///                   "be wrapped"',
         '///                   "even' + (' long lines' * 10) + '";',
         '/// </pre>'])
    self.assertListEqual(
        ['/// this is a short paragraph',
         '///',
         '/// this is a very long line very long line very long line very '
         'long line',
         '/// very long line very long line very long line very long line '
         'very long',
         '/// line very long line',
         '///',
         '/// more content and some html that should not be wrapped',
         '/// <li>',
         '///    <ul>some important stuff</ul>',
         '/// </li>',
         '/// <pre>',
         '///   int some_code = "that should not"',
         '///                   "be wrapped"',
         '///                   "even long lines long lines long lines long '
         'lines long lines long lines long lines long lines long lines long '
         'lines";',
         '/// </pre>'],
        wrapped_lines)

  def test_paragraph_replacements(self):
    """Test applying replacements to a paragraph."""
    parser = generate_constants_lib.DocStringParser(
        paragraph_replacements=[('testy test', 'bishy bosh')])
    wrapped_lines = parser.wrap_lines(['/// testy', '/// test'])
    self.assertListEqual(['/// bishy bosh'], wrapped_lines)

  def test_get_doc_string_lines(self):
    """Test retrival of processed lines."""
    parser = generate_constants_lib.DocStringParser()
    parser.parse_line('/// this is a test')
    parser.parse_line('/// with two paragraphs')
    parser.parse_line('///')
    parser.parse_line('/// second paragraph')
    self.assertListEqual(
        ['/// this is a test with two paragraphs',
         '///',
         '/// second paragraph'],
        parser.get_doc_string_lines())

  def test_get_doc_string_empty(self):
    """Verify an empty string is returned if no documentation is present."""
    parser = generate_constants_lib.DocStringParser()
    self.assertEqual('', parser.get_doc_string())

  def test_get_doc_string(self):
    """Verify doc string terminated in a newline is returned."""
    parser = generate_constants_lib.DocStringParser()
    parser.parse_line('/// this is a test')
    self.assertEqual('/// this is a test\n', parser.get_doc_string())


if __name__ == '__main__':
  googletest.main()
