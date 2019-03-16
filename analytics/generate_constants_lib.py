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

"""Library to generate a C++ header from an Objective-C of constants."""

import datetime
import os
import re
import textwrap


def cpp_header_guard(header_guard_prefix, cpp_header_filename):
  """Format a header guard string from a prefix.

  Args:
    header_guard_prefix: Prefix for the header guard.
    cpp_header_filename: Name of the cpp file this is writing to.

  Returns:
    Header guard string.
  """
  return ('%s%s_H_' % (
      header_guard_prefix,
      os.path.basename(cpp_header_filename.split('.h')[0].upper())))


def format_cpp_header_header(header_guard_prefix,
                             cpp_header_filename,
                             namespace_docs):
  """Generate the header of a cpp header :).

  Args:
    header_guard_prefix: Prefix for the header guard.
    cpp_header_filename: Name of the cpp file this is writing to.
    namespace_docs: List of (namespace, doc) tuples that describe the nested
      namespace for the header.  Where "namespace" is a string name of the
      namespace and "doc" is the doxgyen comment to be placed prior to the
      namespace.

  Returns:
    A string containing the formatted header.
  """
  header_guard = cpp_header_guard(header_guard_prefix, cpp_header_filename)
  lines = ['// Copyright %s Google Inc. All Rights Reserved.' %
           str(datetime.date.today().year),
           '',
           '#ifndef %s' % header_guard,
           '#define %s' % header_guard,
           '']
  for namespace, doc in namespace_docs:
    if doc:
      lines.append('/// @brief %s' % doc)
    lines.append('namespace %s {' % namespace)
  return '\n'.join(lines) + '\n\n'


def format_cpp_header_footer(header_guard_prefix, cpp_header_filename,
                             namespaces):
  """Write the footer of a cpp header.

  Args:
    header_guard_prefix: Prefix for the header guard.
    cpp_header_filename: Name of the cpp file this is writing to.
    namespaces: List of strings that describe the nested namespace for
      the header.


  Returns:
    A string containing the formatted footer.
  """
  lines = ['']
  lines.extend(['}  // namespace %s' % namespace
                for namespace in reversed(namespaces)])
  lines.extend(['',
                '#endif  // %s' % cpp_header_guard(header_guard_prefix,
                                                   cpp_header_filename)])
  return '\n'.join(lines) + '\n'


class DocStringParser(object):
  """Parses doc strings from an Objective-C header file.

  Attributes:
    doc_string_lines: List of lines aggregated in the parser.
    replacements: List of replacement tuples to be applied to the doc string.
      For example, [('kFIR', 'k')] replaces all instances of 'kFIR' in the
      doc string with 'k'.  The first item in each tuple is a regular
      expression.
    paragraph_replacements: Same as the replacements attribute except each
      replacement is applied globally to an entire paragraph.
    global_replacements: Same as the replacements attribute except each
      replacemnet is applied globally to an entire doc string.

  Class Attributes:
    CHARACTERS_PER_LINE: Maximum number of characters per line used by
      wrap_lines() and get_doc_string_lines().
    DOC_STRING_LINE_RE: Regular expression which matches an Objective-C doc
      string line.
    DOC_STRING_PREFIX: Start of non-whitespace that indicates a doc string.
    DOC_STRING_WRAP_DISABLED_RE: Matches lines that should *not* be wrapped
      to CHARACTERS_PER_LINE.
    DOC_STRING_WRAP_DISABLED_START_RE: Matches the first of a set of lines
      that should *not* be wrapped to CHARACTERS_PER_LINE.
    DOC_STRING_WRAP_DISABLED_END_RE: Matches the last of a set of lines
      that should *not* be wrapped to CHARACTERS_PER_LINE.
  """

  DOC_STRING_PREFIX = '/// '
  CHARACTERS_PER_LINE = 80 - len(DOC_STRING_PREFIX)
  DOC_STRING_LINE_RE = re.compile(r'^///.*')
  DOC_STRING_WRAP_DISABLED_RE = re.compile(r'(<[^>]+>|^$)')
  DOC_STRING_WRAP_DISABLED_START_RE = re.compile(r'(?i)(<pre[^>]*>|@code)')
  DOC_STRING_WRAP_DISABLED_END_RE = re.compile(r'(?i)(</pre>|@endcode)')

  def __init__(self, replacements=None, paragraph_replacements=None,
               global_replacements=None):
    """Initialize this instance.

    Args:
      replacements: See the replacements attribute.
      paragraph_replacements: See the paragraph_replacements attribute.
    """
    self.replacements = replacements if replacements else []
    self.paragraph_replacements = (
        paragraph_replacements if paragraph_replacements else [])
    self.global_replacements = (
        global_replacements if global_replacements else [])
    self.reset()

  def reset(self):
    """Clear all lines from the parser."""
    self.doc_string_lines = []

  def parse_line(self, line):
    """Attempt to parse a doc string from a line.

    If a doc string is parsed from the provided line it's added to
    the doc_string_lines attribute.

    Args:
      line: Line to parse.

    Returns:
      True if a doc string was parsed from the line, False otherwise.
    """
    doc_string_match = DocStringParser.DOC_STRING_LINE_RE.match(line)
    if not doc_string_match:
      return False
    self.doc_string_lines.append(line)
    return True

  @staticmethod
  def apply_replacements(line, replacements, replace_multiple_times=False):
    """Apply replacements in the replacements attribtue to a string.

    Args:
      line: String to process.
      replacements: See the replacements attribute.
      replace_multiple_times: Repeatedly apply the replacement until the string
        is stable.

    Returns:
      Processed string.
    """
    output_line = str(line)
    output_line_prev = output_line
    while True:
      for replace_from, replace_to in replacements:
        output_line = re.sub(replace_from, replace_to, output_line)
      # If the set of regular expressions applied a replacement, try replacing
      # again.
      if output_line != output_line_prev and replace_multiple_times:
        output_line_prev = output_line
        continue
      break
    return output_line

  def wrap_lines(self, lines):
    """Wrap a list of lines so they're less than CHARACTERS_PER_LINE.

    Args:
      lines: List of strings to wrap.

    Returns:
      Processed lines in the form of a list of strings.
    """
    wrapped_lines = []
    input_lines = [line[len(DocStringParser.DOC_STRING_PREFIX):]
                   for line in lines]
    enable_wrap = True
    while input_lines:
      end_of_paragraph = len(input_lines)
      if not enable_wrap:
        if DocStringParser.DOC_STRING_WRAP_DISABLED_END_RE.search(
            input_lines[0]) is None:
          end_of_paragraph = 0
        else:
          enable_wrap = True

      if enable_wrap:
        for line_index, line in enumerate(input_lines):
          if DocStringParser.DOC_STRING_WRAP_DISABLED_START_RE.search(line):
            end_of_paragraph = line_index
            enable_wrap = False
            break
          if DocStringParser.DOC_STRING_WRAP_DISABLED_RE.search(line):
            end_of_paragraph = line_index
            break

      paragraph_lines = input_lines[:end_of_paragraph]
      if paragraph_lines:
        paragraph = ' '.join(paragraph_lines)
        paragraph = DocStringParser.apply_replacements(
            paragraph, self.paragraph_replacements)
        wrapped_lines.extend(textwrap.wrap(paragraph,
                                           break_long_words=False,
                                           break_on_hyphens=False))
      if end_of_paragraph < len(input_lines):
        wrapped_lines.append(DocStringParser.apply_replacements(
            input_lines[end_of_paragraph], self.paragraph_replacements))
      input_lines = input_lines[end_of_paragraph + 1:]
    return [(DocStringParser.DOC_STRING_PREFIX + line).rstrip()
            for line in wrapped_lines]

  def get_doc_string_lines(self):
    """Get scrubbed and reformatted doc string lines from the parser.

    Each line is processed by apply_replacements() and reformatted to
    fit 80 characters per line.

    Returns:
      List of strings (one per line) for the doc string.
    """
    replaced_lines = []
    for line in self.doc_string_lines:
      replaced_lines.append(DocStringParser.apply_replacements(
          line, self.replacements))

    replaced_lines = DocStringParser.apply_replacements(
        '\n'.join(replaced_lines),
        self.global_replacements, replace_multiple_times=True).splitlines()
    return self.wrap_lines(replaced_lines)

  def get_doc_string(self):
    """Get scrubbed and reformatted doc string from the parser.

    Returns:
      Multi-line string from the list of lines returned by
      get_doc_string_lines().
    """
    lines = self.get_doc_string_lines()
    if not lines:
      return ''
    return '\n'.join(lines) + '\n'
