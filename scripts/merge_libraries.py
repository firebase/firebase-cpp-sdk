# Copyright 2018 Google LLC
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

"""Merge multiple static libraries into a single output library.

Optionally allows you to rename C and C++ symbols from certain libraries.
"""

import hashlib
import os
import fcntl
import pickle
import re
import shutil
import subprocess
import tempfile
from absl import app
from absl import flags
from absl import logging

FLAGS = flags.FLAGS

flags.DEFINE_string("output", None, "Library file to output merged library to")

flags.DEFINE_enum("platform", None,
                  ["linux", "darwin", "windows", "ios", "android"],
                  "Platform tools to use.")

flags.DEFINE_list(
    "hide_c_symbols", [], "Comma-separated list of strings, each of which is "
    "of the format 'libfilename.a:symbol_regex'. Any symbols defined in the "
    "given .a file that match the associated regex will be renamed. If the "
    "colon and regex are omitted, :. is assumed")
flags.DEFINE_list(
    "hide_cpp_namespaces", [],
    "C++ namespaces (top-level only) to rename, in a comma-separated list.")
flags.DEFINE_bool(
    "auto_hide_cpp_namespaces", False,
    "With this flag, all input libraries and --scan_libs will be scanned for "
    "top-level C++ namespaces to rename. You can ignore specific namespaces by "
    "using --ignore_cpp_namespaces, and 'std::' will never be renamed.")
flags.DEFINE_list(
    "ignore_cpp_namespaces", [],
    "C++ namespaces (top-level only) NOT to rename, in a comma-separated list. "
    "These will be ignored by --auto_hide_cpp_namespaces.")
flags.DEFINE_list(
    "scan_libs", [],
    "Additional libraries to scan for symbols (besides the input libraries)")
flags.DEFINE_string(
    "rename_string", "f_b_",
    "Name to prepend for C symbols or top-level C++ namespaces.")
# C++ name demangling is terrible. Some demanglers will simply give up on some
# symbols, so we may need to try more than one until a string gets properly
# demangled.
flags.DEFINE_list(
    "demangle_cmds", "c++filt",
    "C++ name demangling commands, which should accept symbols two ways: "
    "First, if --streaming_demanglers, they should accept one symbol as an "
    "argument, and output the demangled symbol to stdout. Or, if "
    "--nostreaming_demanglers, they should accept one symbol per line from "
    "stdin, and output each symbols to stdout, one per line. "
    "(In all cases the original symbol will be printed unchanged if it "
    "couldn't be demangled). "
    "C++ symbols will each be passed to these commands one at a time "
    "in sequence until a command is able to demangle the symbol.")
flags.DEFINE_string("binutils_ar_cmd", "ar", "Binutils 'ar' command.")
flags.DEFINE_string("binutils_objcopy_cmd", "objcopy",
                    "Binutils 'objcopy' command.")
flags.DEFINE_string("binutils_nm_cmd", "nm", "Binutils 'nm' command.")
flags.DEFINE_bool("strict_cpp", True,
                  "Be strict when renaming C++ namespaces - only rename "
                  "symbols that contain 'namespace::' when demangled.")
flags.DEFINE_bool("streaming_demanglers", True,
                  "Stream symbols to/from the C++ name demanglers, "
                  "rather than running them one-by-one on each symbol.")
flags.DEFINE_bool("strip_debug", False,
                  "Strip debugging information from output library.")
flags.DEFINE_string("cache", None, "Cache file. If set, symbols and other data "
                    "will be serialized to this file so they can be reused by "
                    "multiple runs of the script on the same sets of files.")
flags.DEFINE_bool(
    "rename_external_cpp_symbols", True,
    "If this is enabled, allows renaming of C++ symbols that are used across "
    "two libraries. C++ symbols in the appropriate namespaces will be renamed "
    "even if they are external. Otherwise, only symbols defined in the library "
    "are renamed.")
flags.DEFINE_bool(
    "skip_creating_archives", False,
    "Skip creating archive files (.a or .lib) and instead just leave the object "
    "files (.o or .obj) in the output directory.")
flags.DEFINE_string("force_binutils_target", None, "Force all binutils calls to "
                    "use the given target, via the --target flag. If not set, "
                    "will autodetect target format. If you want to specify "
                    "different input and output formats, separate them with a comma.")

# Never rename system namespaces by default when --auto_hide_cpp_namespaces is enabled.
IMPLICIT_CPP_NAMESPACES_TO_IGNORE = {"std", "type_info",
                                     "__gnu_cxx", "stdext",
                                     "cxxabiv1", "__cxxabiv1"}

DEFAULT_ENCODING = "ascii"

# Once a binutils command fails due to an ambiguous target, use this explicit target
# when running all subsequent binutils commands.
binutils_force_target_format = None


class Demangler(object):
  """Spins up a C++ demangler and pipes symboles to/from it to demangle them.

  Attributes:
    cmdline: Command-line for the demangler, in a list. e.g. ["c++filt"]
    timeout: Timeout for demangler output, in milliseconds.
    pipe: Subprocess pipe for the demangler command.
  """

  def __init__(self, cmdline, timeout=100):
    """Initialize a demangler.

    Args:
      cmdline: List containing the command line to run.
      timeout: Timeout in milliseconds.
    """
    # Spin up the subprocess
    self.cmdline = cmdline
    if not FLAGS.streaming_demanglers:
      return None
    self.timeout = timeout
    logging.debug("Start demangler process %s", self.cmdline)
    self.start()

  def readbyte(self):
    """Read a byte from the pipe.

    If a byte is not available within the specified timeout, increase the
    timeout and restart the pipe process.

    Returns:
      A single byte, or None if the read timed out.
    """
    if not FLAGS.streaming_demanglers:
      return None
    try:
      return self.pipe.stdout.read(1).decode(DEFAULT_ENCODING)
    except Exception as e:  # pylint: disable=broad-except
      logging.error("Demangler returned error on read: %s", e)
      return None

  def start(self):
    """Start the demangler subprocess we are piping data to/from."""
    if not FLAGS.streaming_demanglers:
      return None
    self.pipe = subprocess.Popen(
        self.cmdline, stdout=subprocess.PIPE, stdin=subprocess.PIPE)

  def demangle(self, symbol):
    """Pass a symbol into the demangler subprocess and return the results.

    Args:
      symbol: C++ symbol to run through name mangling.
    Returns:
      Demangled symbol, or the original symbol if it couldn't be demangled.
    """
    if not FLAGS.streaming_demanglers:
      errors = []
      output = run_command(self.cmdline + [symbol], errors, True)
      demangled_bin = output[0] if len(output) else symbol
      return demangled_bin if demangled_bin else symbol

    if self.pipe.poll() is not None:
      # Process crashed, restart it now.
      self.start()

    to_write = symbol + "\n"
    self.pipe.stdin.write(to_write.encode())
    self.pipe.stdin.flush()
    # Read a line of output from the pipe.
    demangled_str = ""
    self.pipe.stdout.flush()
    if self.pipe.poll() is not None:
      # Process crashed, return the original symbol.
      self.start()
    c = self.readbyte()
    while c != "\n":
      if c is None:
        # The process crashed, return the undemangled symbol.
        return symbol
      demangled_str += str(c)
      c = self.readbyte()
    return demangled_str if demangled_str else symbol

  def restart(self):
    """Close the subprocess, then start it again."""
    self.close()
    self.start()

  def close(self):
    """Terminate the subprocess, if it is open."""
    if not FLAGS.streaming_demanglers:
      return
    if self.pipe:
      logging.debug("Stop demangler process %s", self.cmdline)
      if self.pipe.poll() is not None:
        self.pipe.terminate()
        self.pipe.wait()
      self.pipe = None


def extract_archive(archive_file):
  """Extract all object files from a library archive into the current directory.

  Args:
    archive_file: Library archive file to extract.
  Returns:
    Tuple of extracted file list and error list.
  """
  (file_list, errors) = list_objects_in_archive(archive_file)
  if len(file_list) == len(set(file_list)):
    # No duplicate files, extract all at once.
    run_command([FLAGS.binutils_ar_cmd, "x", archive_file], errors)
    return (file_list, errors)
  else:
    # Duplicate filenames, so we must extract file by file and rename as we go.
    file_counts = {}
    output_file_list = []
    for f in file_list:
      file_counts[f] = file_counts[f] + 1 if f in file_counts else 1

      run_command(
          [FLAGS.binutils_ar_cmd, "xN",
           str(file_counts[f]), archive_file, f], errors)
      new_f = os.path.join(
          os.path.dirname(f), "%d_%s" % (file_counts[f], os.path.basename(f)))
      os.rename(f, new_f)
      output_file_list.append(new_f)
    return (output_file_list, errors)


def list_objects_in_archive(archive_file):
  """List all the object files contained in a library archive.

  Args:
    archive_file: Library archive file to list.
  Returns:
    Tuple of list of object files inside the library archive, and error.
  """
  errors = []
  output = run_command([FLAGS.binutils_ar_cmd, "t", archive_file], errors)
  return (list(filter(RE_OBJECT_FILE_PLATFORM[FLAGS.platform].match,
                      output)), errors)


def create_archive(output_archive_file, object_files, old_archive=None):
  """Create a library file from a collection of object files.

  Args:
    output_archive_file: Output library archive to write.
    object_files: List of object files to include in the library.
    old_archive: Old archive file to modify, so we preserve archive format.
  Returns:
    Empty list if there are no errors, or error text if there was an error.
  """
  errors = []
  if old_archive and FLAGS.platform != "windows" and FLAGS.platform != "darwin":
    # Copy the old archive to the new archive, then clear the files from it.
    # This preserves the file format of the old archive file.
    # On Windows, we'll always create a new archive.
    shutil.copy(old_archive, output_archive_file)
    (old_contents, errors) = list_objects_in_archive(output_archive_file)
    run_binutils_command(
        [FLAGS.binutils_ar_cmd, "d", output_archive_file] + old_contents,
        errors)
    run_binutils_command(
        [FLAGS.binutils_ar_cmd, "rs", output_archive_file] + object_files,
        errors)
  else:
    run_binutils_command(
        [FLAGS.binutils_ar_cmd, "rcs", output_archive_file] + object_files,
        errors)


def replace_strings_in_archive(archive_path, replacements):
  """Rename object files within a library archive.

  We do this in an extremely simple way: by binary replacing the
  strings specified in "renames". Make sure all your replacements are
  the same length as the originals!

  Args:
    archive_path: Library archive path.
    replacements: Dictionary of replacements, key = regex, value = replacement
      string.
  Returns:
    New library archive filename, or None if there was an error.
  """
  new_filename = None
  with open(archive_path, "rb") as binary_read:
    file_bytes = binary_read.read()
  for match, replace in replacements.items():
    if len(match) != len(replace):
      logging.error("Can't binary replace '%s' with '%s' (unequal length).",
                    match, replace)
      return None
    file_bytes = re.sub(match, replace, file_bytes)
  named_tempfile = tempfile.NamedTemporaryFile(suffix=".a")
  _tempfiles.append(named_tempfile)  # Will be cleaned up on program exit.
  new_filename = os.path.abspath(named_tempfile.name)
  with open(new_filename, "wb") as binary_write:
    binary_write.write(file_bytes)
  return new_filename


# Running C++ name demangler processes, in the order we will query them.
_demanglers = []


def init_demanglers():
  """Initialize the demanglers.

  Returns:
    List of Demangle objects. Also sets the _demanglers global.
  """
  for c in FLAGS.demangle_cmds:
    cmdline = [c]
    if "demumble" in c:
      # Special rule, demumble needs -u to use unbuffered.
      cmdline.append("-u")
    _demanglers.append(Demangler(cmdline))
  return _demanglers


def shutdown_demanglers():
  """Shutdown the demanglers."""
  for demangler in _demanglers:
    demangler.close()
  del _demanglers[:]


def demangle_symbol(symbol):
  """Run the C++ demangler on this symbol.

  Args:
    symbol: Symbol to demangle, if possible.
  Returns:
    Demangled symbol.
  """
  if "demangle" not in _cache: return symbol  # No demanglers active.
  if FLAGS.platform == "windows":
    symbol = re.sub(r"^\$[^$]*\$", "", symbol)  # Filter out "$pdata$" etc.
  if symbol not in _cache["demangle"][FLAGS.platform]:
    _cache["demangle"][FLAGS.platform][symbol] = symbol  # Default: no change
    for demangler in _demanglers:
      demangled_symbol = demangler.demangle(symbol)
      if demangled_symbol and (symbol != demangled_symbol):
        _cache["demangle"][FLAGS.platform][symbol] = demangled_symbol
        break
      if FLAGS.platform == "darwin" or FLAGS.platform == "ios":
        # On Darwin, try demangling again without the leading _.
        demangled_symbol = demangler.demangle(symbol[1:])
        if demangled_symbol and (symbol[1:] != demangled_symbol):
          _cache["demangle"][FLAGS.platform][symbol] = demangled_symbol
          break
  return _cache["demangle"][FLAGS.platform][symbol]


def is_cpp_symbol(symbol):
  """Returns True if the given symbol is a C++ symbol, False otherwise.

  Args:
    symbol: Symbol to check.
  Returns:
    True if the symbol is a C++ symbol, False otherwise.
  """
  # Linux uses the IA64 ABI for its name mangling scheme:
  #   https://itanium-cxx-abi.github.io/cxx-abi/abi.html#mangling
  # MachO binaries, used on Darwin, follow this but prepend an extra underscore:
  #   https://developer.apple.com/library/content/documentation/DeveloperTools\
  #   /Conceptual/MachOTopics/1-Articles/executing_files.html#//apple_ref/doc\
  #   /uid/TP40001829-97182
  # Windows uses a completely different name mangling scheme:
  #  https://msdn.microsoft.com/en-us/library/56h2zst2.aspx
  if FLAGS.platform == "linux" or FLAGS.platform == "android":
    return symbol.startswith("_Z")
  if FLAGS.platform == "darwin" or FLAGS.platform == "ios":
    return symbol.startswith("__Z") or symbol.startswith("l__Z")
  if FLAGS.platform == "windows":
    return "@" in symbol or "?" in symbol


def get_top_level_namespaces(demangled_symbols):
  """Gets all top-level C++ namespaces from a set of demangled symbols.

  Args:
    demangled_symbols: Set of demangled_symbols to scan for namespaces.
  Returns:
    A set of all top-level namespaces found.
  """
  # Alphanumeric followed by ::, but only if not prefixed with :: or another
  # alphanumeric. Also the :: must be followed by another alphanumeric.
  #
  # This should match the top-level namespaces in the following demangled names:
  #   namespace::Class::Method()
  #   Function(namespace::Type param)
  #   TemplatedFunction<namespace::Type>::Method()
  # etc., and we use findall() to match all of them in one symbol, e.g.
  #   namespace1::Function<namespace2::Type>::Method(namespace3::Type param)
  # It will specifically not match second-level or deeper namespaces:
  #   matched_namespace::unmatched_namespace::Class::Method()
  regex_top_level_namespaces = re.compile(
      r"(?<![a-zA-Z0-9_])(?<!::)([a-zA-Z_~\{\$][a-zA-Z0-9_\{\}\#]*)::(?=[a-zA-Z_~` \{\$])")
  found_namespaces = set()
  for cppsymbol in demangled_symbols:
    m = regex_top_level_namespaces.findall(cppsymbol)
    for ns in m:
      if ns not in found_namespaces:
        found_namespaces.add(ns)
  return found_namespaces


def add_automatic_namespaces(symbols):
  """Scan symbols for top-level namespaces and add them to the namespace list.

  Adds any newly found namespaces to FLAGS.hide_cpp_namespaces, but will
  ignore any mentioned in FLAGS.ignore_cpp_namespaces and in
  IMPLICIT_CPP_NAMESPACES_TO_IGNORE above.

  Note that the order of FLAGS.hide_cpp_namespaces will not be preserved.

  Args:
    symbols: Set of symbols to scan for namespaces.
  """
  logging.debug("Scanning for top-level namespaces.")
  demangled_symbols = [demangle_symbol(symbol) for symbol in symbols]
  hide_namespaces = get_top_level_namespaces(demangled_symbols)
  # strip out top-level namespaces beginning with an underscore
  hide_namespaces = set([n for n in hide_namespaces if n[0] != "_"])
  ignore_namespaces = set(FLAGS.ignore_cpp_namespaces)
  ignore_namespaces.update(IMPLICIT_CPP_NAMESPACES_TO_IGNORE)
  logging.debug("Ignoring namespaces: %s", " ".join(
      hide_namespaces.intersection(ignore_namespaces)))
  hide_namespaces = hide_namespaces.difference(ignore_namespaces)
  logging.debug("Found namespaces to hide: %s", " ".join(hide_namespaces))
  FLAGS.hide_cpp_namespaces = list(
      set(FLAGS.hide_cpp_namespaces).union(hide_namespaces))

# Regex to filter the output of "nm" to get symbols.
# The first field is the memory address, the second is a single letter
# showing the symbol definition mode, and the last field is the symbol name.
RE_NM_SYMBOLS_PLATFORM = {
    "windows":
        re.compile(
            r"^(?P<addr>[0-9a-fA-F ]{8,16}) (?P<def>.) (?P<symbol>[^.].*)$"),
    "linux":
        re.compile(
            r"^(?P<addr>[0-9a-fA-F ]{8,16}) (?P<def>.) (?P<symbol>[^.].*)$"),
    "android":
        re.compile(
            r"^(?P<addr>[0-9a-fA-F ]{8,16}) (?P<def>.) (?P<symbol>[^.].*)$"),
    "darwin":
        re.compile(
            r"^(?P<addr>[0-9a-fA-F ]{8,16}) (?P<def>.) (?P<symbol>[^.].*)$"),
    "ios":
        re.compile(
            r"^(?P<addr>[0-9a-fA-F ]{8,16}) (?P<def>.) (?P<symbol>[^.].*)$"),
}

# Regex that matches the name of an object file in an archive, when
# output via "ar t" on the file.
RE_OBJECT_FILE_PLATFORM = {
    "windows": re.compile(r"^.*\.(o(bj)?|res)$"),
    "linux": re.compile(r"^.*\.o$"),
    "android": re.compile(r"^.*\.o$"),
    "darwin": re.compile(r"^.*\.o$"),
    "ios": re.compile(r"^.*\.o$"),
}

# On Windows, when renaming symbols, rename an additional copy of the symbol
# with this prefix added. For example, if you rename hello_world to
# new_hello_world, also add a rule to rename __imp_hello_world to
# __imp_new_hello_world.
_additional_symbol_prefixes = {
    # "windows": ["__imp_"]
}

# If a binutils tool returns a "file format is ambiguous" error,
# prefer matching formats that begin with the given prefix (depending
# on the platform). It will select the first format that starts with the
# given prefix. (A blank prefix just means use the first format no
# matter what.)
BINUTILS_PREFERRED_FORMAT_PREFIX_IF_AMBIGUOUS = {
    "windows": "pe-",
    "linux": "",
    "android": "",
    "darwin": "",
    "ios": "",
}

def read_symbols_from_archive(archive_file):
  """Read the symbols defined in a library archive.

  Args:
    archive_file: Library archive file to read symbols from.
  Returns:
    Tuple containing two entries: a set of symbols defined in the archive, and a
    set of ALL symbols (including external symbols) in the archive. C++ symbols
    are not demangled in either..
  """
  errors = []
  raw_output = run_binutils_command([FLAGS.binutils_nm_cmd, archive_file],
                                    errors, True)
  all_symbols = set()
  defined_symbols = set()
  for line in raw_output:
    m = RE_NM_SYMBOLS_PLATFORM[FLAGS.platform].match(line)
    if m:
      symbol = m.group("symbol")
      # Ignore any Objective-C or Objective-C++ methods.
      if FLAGS.platform == "darwin" and ('[' in symbol or ']' in symbol):
        continue

      all_symbols.add(symbol)

      addr = m.group("addr")
      if addr.isalnum():
        defined_symbols.add(symbol)
  return (defined_symbols, all_symbols)


def symbol_includes_top_level_cpp_namespace(cpp_symbol, namespace):
  """Returns true if the C++ symbol contains the namespace in it.

  This means the symbol is within the namespace, or the namespace as
  an argument type, return type, template type, etc.

  If FLAGS.strict_cpp == True, this will only return true if the namespace
  is at the top level of the symbol.

  Args:
    cpp_symbol: C++ symbol to check.
    namespace: Namespace to look for in the C++ symbol.
  Returns:
    True if the symbol includes the C++ namespace at the top level (or anywhere,
    if FLAGS.strict_cpp == False), False otherwise.
  """
  # Early out if the namespace isn't in the mangled symbol.
  if namespace not in cpp_symbol:
    return False
  if not FLAGS.strict_cpp:
    # If we aren't being fully strict about C++ symbol renaming,
    # we can use this placeholder method.
    if FLAGS.platform == "windows" and re.search(r"[^a-z_]%s@@" % namespace, cpp_symbol):
      return True
    elif (FLAGS.platform != "windows" and
          ("%d%s" % (len(namespace), namespace)) in cpp_symbol):
      return True
    else:
      return False
  # Since we are being strict about C++ symbols, we need to ensure that
  # the symbol is really within the namespace, so demangle it first.
  demangled = demangle_symbol(cpp_symbol)
  # Check if the demangled symbol starts with "namespace::".
  if demangled.startswith(namespace + "::"):
    return True
  # Or, check if the demangled symbol has "namespace::" preceded by a non-
  # alphanumeric character. This avoids a false positive on "notmynamespace::".
  # Also don't allow a namespace :: right before the name.
  regex = re.compile("[^0-9a-zA-Z_:]%s::" % namespace)
  if re.search(regex, demangled):
    return True

  return False

# Regex for stripping the prefix from Windows symbols.
# Strip $pdata$, $pdata$0$, $pdata$1$, etc, and same with $unwind$ and $chain$.
RE_WINDOWS_PREFIX = re.compile(
    r"^(?P<prefix>\$(unwind|pdata|chain)\$([0-9]+\$)?)(?P<remainder>.*)$")


def split_symbol(symbol):
  """Split a symbol into a prefix (per platform) and remainder of the symbol.

  Args:
    symbol: Symbol to split.
  Returns:
    Tuple of (prefix, remainder). If no prefix was found, this will
  be ("", symbol).
  """
  # On Windows, when renaming C symbols, strip this prefixes from symbols,
  # add the prefix, then add the prefix back. For example, if you are
  # renaming $pdata$hello_world, the new name would be $pdata$new_hello_world.
  if FLAGS.platform == "windows":
    m = RE_WINDOWS_PREFIX.match(symbol)
    if m and m.group("prefix") and m.group("remainder"):
      return (m.group("prefix"), m.group("remainder"))
    else:
      return ("", symbol)
  elif FLAGS.platform == "darwin" or FLAGS.platform == "ios":
    # On Mach-O platforms, remove leading underscore.
    if symbol.startswith("_"):
      return ("_", symbol[1:])
    else:
      return ("", symbol)
  else:
    return ("", symbol)


def rename_symbol(symbol):
  """Rename the given symbol.

    If it is a C symbol, prepend FLAGS.rename_string to the symbol, but
    account for the symbol possibly having a prefix via split_symbol().

    If it is a C++ symbol, prepend FLAGS.rename_string to all instances of the
    given namespace.

  Args:
    symbol: C or C++ symbol to rename.
  Returns:
    Dictionary, keys = old symbols, values = renamed symbols.
  """
  new_renames = {}
  if is_cpp_symbol(symbol):
    # Scan through the symbol looking for the namespace name, then modify it.
    new_symbol = symbol
    if FLAGS.platform in ["linux", "android", "darwin", "ios"]:
      for ns in FLAGS.hide_cpp_namespaces:
        if symbol_includes_top_level_cpp_namespace(symbol, ns):
          # Linux and Darwin: To rename "namespace" to "prefixnamespace",
          # change all instances of "9namespace" to "15prefixnamespace".
          # (the number is the length of the namespace name)
          # See https://itanium-cxx-abi.github.io/cxx-abi/abi.html#mangling
          new_ns = FLAGS.rename_string + ns
          new_symbol = re.sub("(?<=[^0-9])%d%s" % (len(ns), ns),
                              "%d%s" % (len(new_ns), new_ns), new_symbol)
      new_renames[symbol] = new_symbol
    elif FLAGS.platform == "windows":
      for ns in FLAGS.hide_cpp_namespaces:
        if symbol_includes_top_level_cpp_namespace(symbol, ns):
          # Windows: To rename "namespace" to "prefixnamespace",
          # change all instances of "[^a-z_]namespace@@" to "[^a-z]prefixnamespace@@",
          # See https://msdn.microsoft.com/en-us/library/56h2zst2.aspx
          new_ns = FLAGS.rename_string + ns
          new_symbol = re.sub(r"(?<=[^a-z_])%s@@" % ns, r"%s@@" % new_ns, new_symbol)
      new_renames[symbol] = new_symbol
  else:
    if FLAGS.platform == "windows" and symbol.startswith("$LN"):
      # Don't rename $LN*, those are local symbols.
      return new_renames
    # C symbol. Just split, rename, and re-join.
    (prefix, remainder) = split_symbol(symbol)
    new_symbol = prefix + FLAGS.rename_string + remainder
    new_renames[symbol] = new_symbol
    for added_prefix in _additional_symbol_prefixes.get(FLAGS.platform, []):
      new_renames[added_prefix + symbol] = new_renames[symbol]

  return new_renames


def create_symbol_redefinition_file(rename_symbols):
  """Create a helper file that will be used to redefine symbols.

  Args:
    rename_symbols: Dictionary of symbols to rename.
  Returns:
    NamedTemporaryFile object containing the helper data for redefining symbols,
    or None if there are no symbols to rename.
  """
  if not rename_symbols:
    return None
  # Write the symbol redefinition file. It will be cleaned up when the program
  # exits.
  redefinition_file = tempfile.NamedTemporaryFile()
  redefinition_data = open(redefinition_file.name, "w")
  any_written = False
  for key, val in rename_symbols.items():
    if key != val:
      any_written = True
      redefinition_data.write("%s %s\n" % (key, val))
  redefinition_data.close()
  return redefinition_file if any_written else None


def move_object_file(src_obj_file, dest_obj_file, redefinition_file=None):
  """Move an object file from src to dest, renaming symbols as requested.

  Args:
    src_obj_file: Object file to move from.
    dest_obj_file: Object file to move to.
    redefinition_file: Filename to use for redefining symbols.
  Returns:
    Blank list on success, error text list on failure.
  """
  if not redefinition_file and not FLAGS.strip_debug:
    # If nothing to rename and no debug symbols to strip, just rename the file.
    os.rename(src_obj_file, dest_obj_file)
    return []

  # Remove the output file if it already exists.
  if os.path.isfile(dest_obj_file):
    os.unlink(dest_obj_file)
  errors = []
  output = run_binutils_command(
      [FLAGS.binutils_objcopy_cmd] +
      (["--strip-debug"] if FLAGS.strip_debug else []) +
      (["--remove-section", ".pdata"] if FLAGS.platform == "windows" else []) +
      (["--redefine-syms=%s" % redefinition_file]
       if redefinition_file else []) +
      [src_obj_file, dest_obj_file], errors, True)

  # If we created the output file, remove the input file.
  if os.path.isfile(dest_obj_file):
    os.unlink(src_obj_file)
  else:
    logging.error("Failed to create '%s':", dest_obj_file)
    logging.error("\n".join(errors))
  return output


class Error(Exception):
  """Exception raised by methods in this module."""
  pass


def run_binutils_command(cmdline, error_output=None, ignore_errors=False):
  """Run the given binutils command and return its output as a list of lines.

  This is a wrapper for run_command(), which will capture an expected class of
  error message ("ambiguous file format") and re-run the command with a
  specified target file format. It otherwise acts the same as run_command.

  Args:
    cmdline: List of command line arguments; cmdline[0] is the command.
    error_output: Optional list to add the command's stderr text lines to.
    ignore_errors: If true, does not fatally log errors if the process returns
    non-zero.
  Returns:
    A list of lines of text of the command's stdout.
  """
  global binutils_force_target_format
  if binutils_force_target_format:
    # Once we've had to force the format once, assume all subsequent
    # files use the same format. Also we will need to explicitly specify this
    # format when creating an archive with "ar".
    # If we've never had to force a format, let binutils autodetect.
    # Also, we can force a separate input and output target for objcopy, splitting on comma.
    target_list = binutils_force_target_format.split(",")
    if cmdline[0] == FLAGS.binutils_objcopy_cmd and len(target_list) > 1:
      target_params = ["--input-target=%s" % target_list[0], "--output-target=%s" % target_list[1]]
    else:
      target_params = ["--target=%s" % target_list[0]]
    output = run_command([cmdline[0]] + target_params + cmdline[1:],
                         error_output, ignore_errors)
  else:
    # Otherwise, if we've never had to force a format, use the default.
    output = run_command(cmdline, error_output, True)
  if error_output and not output:
    # There are some expected errors: "Bad value" or "File format is ambiguous".
    #
    # For some reason, when working with a MIPS file, the autodetected target
    # causes an output of "Bad value". When this happens, we need to re-run
    # the command and force a binary format of "elf32-little" or "elf64-little",
    # depending on whether the file is 32-bit or 64-bit.
    #
    # Line 0: filename.o: Bad value
    if not binutils_force_target_format and error_output and "Bad value" in error_output[0]:
      # Workaround for MIPS, force elf32-little and/or elf64-little.
      error_output = []
      logging.debug("Bad value when running %s %s",
                    os.path.basename(cmdline[0]), " ".join(cmdline[1:]))
      logging.debug("Retrying with --target=elf32-little")
      output = run_command([cmdline[0]] + ["--target=elf32-little"] +
                           cmdline[1:], error_output, True)
      binutils_force_target_format='elf32-little'
      if error_output:
        # Oops, it wasn't 32-bit, try 64-bit instead.
        error_output = []
        logging.debug("Retrying with --target=elf64-little")
        output = run_command([cmdline[0]] + ["--target=elf64-little"] +
                             cmdline[1:], error_output, ignore_errors)
        binutils_force_target_format='elf64-little'
    # In other cases, we sometimes get an expected error about ambiguous file
    # format, which also includes a list of matching formats:
    #
    # Line 0: filename.o: File format is ambiguous
    # Line 1: Matching formats: format1 format2 [...]
    #
    # If this occurs, we will run the command again, passing in the
    # target format that we believe we should use instead.
    elif not binutils_force_target_format and (len(error_output) >= 2 and
          "ile format is ambiguous" in error_output[0]):
      m = re.search("Matching formats: (.+)", error_output[1])
      if m:
        all_formats = m.group(1).split(" ")
        preferred_formats = [
          fmt
          for fmt
          in all_formats
          if fmt.startswith(
              BINUTILS_PREFERRED_FORMAT_PREFIX_IF_AMBIGUOUS[FLAGS.platform])
        ]
        # Or if for some reason none was found, just take the default (first).
        binutils_force_target_format=(preferred_formats[0]
                                      if len(preferred_formats) > 0
                                      else all_formats[0])
        error_output = []
        logging.debug("Ambiguous file format when running %s %s (%s)",
                      os.path.basename(cmdline[0]), " ".join(cmdline[1:]), ", ".join(all_formats))
        logging.debug("Retrying with --target=%s", binutils_force_target_format)
        output = run_command([cmdline[0]] + ["--target=%s" % binutils_force_target_format] + cmdline[1:],
                             error_output, ignore_errors)
    if error_output and not ignore_errors:
      # If we failed any other way, or if the second run failed, bail.
      logging.fatal("Error running binutils command: %s %s\n%s",
                    os.path.basename(cmdline[0]), " ".join(cmdline[1:]),
                    "\n".join(error_output))
  return output


def run_command(cmdline, error_output=None, ignore_errors=False):
  """Run the given command line and return its output as a list of lines.

  Args:
    cmdline: List of command line arguments; cmdline[0] is the command.
    error_output: Optional list to add the command's stderr text lines to.
    ignore_errors: If true, does not log errors if the process returns non-zero.
  Returns:
    A list of lines of text of the command's stdout.
  """
  child = subprocess.Popen(
      cmdline, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
  (output, error) = child.communicate()
  if child.returncode != 0 and not ignore_errors:
    logging.warning("Subprocess returned non-zero value (%d)", child.returncode)
    logging.warning("Command-line: %s", " ".join(cmdline))
    if output:
      logging.warning("Standard output: %s", output)
    if error:
      logging.warning("Standard error: %s", error)
  if error_output is not None:
    error_output.extend(error.decode(DEFAULT_ENCODING).splitlines())
  return output.decode(DEFAULT_ENCODING).splitlines()


def add_abspath(command):
  """Returns the absolute path to the command, if a path is specified at all.

  Args:
    command: Command to get the full path of. If it does not contain a "/", it's
    considered to be in the PATH.
  Returns:
    The original string (if it doesn't contain a "/"), or the absolute path to
    the command.
  """
  return command if "/" not in command else os.path.abspath(command)

# Cache that will be saved between runs if --cache is specified.
_cache = {}

# List of NamedTemporaryFile objects to keep open until we finish running.
# These will be deleted on program exit.
_tempfiles = []


def init_cache():
  """Initialize the cache, loading from FLAGS.cache or creating a new one."""
  if FLAGS.cache and os.path.isfile(
      FLAGS.cache) and os.path.getsize(FLAGS.cache) > 0:
    # If a data cache was specified, load it now.
    with open(FLAGS.cache, "rb") as handle:
      fcntl.lockf(handle, fcntl.LOCK_SH)  # For reading, shared lock is OK.
      _cache.update(pickle.load(handle))
      fcntl.lockf(handle, fcntl.LOCK_UN)
  else:
    # Set up a default cache dictionary.
    # _cache["symbols"] is indexed by abspath of library file
    # _cache["demangle"] is indexed by platform, then by symbol
    _cache.update({"symbols": {}, "demangle": {}})
  if FLAGS.platform not in _cache["demangle"]:
    _cache["demangle"][FLAGS.platform] = {}


def shutdown_cache():
  """If FLAGS.cache is set, write the cache data to disk."""
  if FLAGS.cache:
    if os.path.isfile(FLAGS.cache):
      os.unlink(FLAGS.cache)
    with open(FLAGS.cache, "wb") as handle:
      fcntl.lockf(handle, fcntl.LOCK_EX)  # For writing, need exclusive lock.
      pickle.dump(_cache, handle, protocol=pickle.HIGHEST_PROTOCOL)
      fcntl.lockf(handle, fcntl.LOCK_UN)


def main(argv):
  global binutils_force_target_format
  binutils_force_target_format = FLAGS.force_binutils_target
  try:
    working_root = None
    input_paths = []
    for filename in argv[1:]:
      input_paths.append([filename, os.path.abspath(filename)])
    if not input_paths:
      logging.fatal("No input files specified")
    output_path = os.path.abspath(FLAGS.output)
    additional_input_paths = []
    for filename in FLAGS.scan_libs:
      additional_input_paths.append([filename, os.path.abspath(filename)])

    # Create the working directory.
    working_root = tempfile.mkdtemp(suffix="merge")
    if not working_root:
      raise Error("Couldn't create temp directory")

    # Add absolute paths to these commands if they specify any directory.
    FLAGS.binutils_ar_cmd = add_abspath(FLAGS.binutils_ar_cmd)
    FLAGS.binutils_nm_cmd = add_abspath(FLAGS.binutils_nm_cmd)
    FLAGS.binutils_objcopy_cmd = add_abspath(FLAGS.binutils_objcopy_cmd)
    FLAGS.demangle_cmds = [add_abspath(c) for c in FLAGS.demangle_cmds]
    FLAGS.cache = (os.path.abspath(FLAGS.cache) if FLAGS.cache else None)

    init_cache()

    # Scan through subset of libs in FLAGS.hide_c_symbols to find C symbols to
    # rename.
    rename_symbols = {}
    for libfile_and_regex in FLAGS.hide_c_symbols:
      if ':' not in libfile_and_regex:
        libfile_and_regex += ':.'
      [libfile, regex] = libfile_and_regex.split(":", 1)
      logging.debug("Scanning for C symbols in %s", libfile)
      regex_compiled = re.compile(regex)
      if os.path.abspath(libfile) in _cache["symbols"]:
        (defined_symbols,
         all_symbols) = _cache["symbols"][os.path.abspath(libfile)]
      else:
        (defined_symbols, all_symbols) = read_symbols_from_archive(libfile)
        _cache["symbols"][os.path.abspath(libfile)] = (defined_symbols,
                                                       all_symbols)
      for symbol in defined_symbols:
        # For C symbols, only use them if they are defined.
        if (not is_cpp_symbol(symbol)) and regex_compiled.match(symbol):
          rename_symbols.update(rename_symbol(symbol))

    os.chdir(working_root)

    if FLAGS.hide_cpp_namespaces or FLAGS.auto_hide_cpp_namespaces:
      init_demanglers()
      # Scan through all input libraries for C++ symbols matching any of the
      # hide_cpp_namespaces.
      cpp_symbols = set()
      all_defined_symbols = set()
      for input_path in input_paths + additional_input_paths:
        logging.debug("Scanning for C++ symbols in %s", input_path[1])
        if os.path.abspath(input_path[1]) in _cache["symbols"]:
          (defined_symbols,
           all_symbols) = _cache["symbols"][os.path.abspath(input_path[1])]
        else:
          (defined_symbols,
           all_symbols) = read_symbols_from_archive(input_path[1])
          _cache["symbols"][os.path.abspath(input_path[1])] = (defined_symbols,
                                                               all_symbols)
        cpp_symbols.update(all_symbols
                           if FLAGS.rename_external_cpp_symbols
                           else defined_symbols)
        all_defined_symbols.update(defined_symbols)

      # If we are set to scan for namespaces, do that now.
      if FLAGS.auto_hide_cpp_namespaces:
        add_automatic_namespaces(all_defined_symbols)

      for symbol in cpp_symbols:
        if is_cpp_symbol(symbol):
          rename_symbols.update(rename_symbol(symbol))
      shutdown_demanglers()

    logging.debug("Creating symbol redefinition file for %d symbols",
                  len(rename_symbols))
    symbol_redefinition_file = create_symbol_redefinition_file(rename_symbols)

    # List of all object files, which will be renamed to have unique names.
    all_obj_files = []
    for input_path in input_paths:
      logging.debug("Checking input archive %s", input_path[1])
      # Create a unique directory name from a hash of the input filename.
      lib_name_hash = hashlib.md5(
          input_path[0].encode(DEFAULT_ENCODING)).hexdigest()
      (obj_file_list, errors) = list_objects_in_archive(input_path[1])
      if errors:
        logging.fatal("Error listing archive %s: %s", input_path[1], errors)

      # If any of the filenames in this archive contain a "/" or "\", we need to
      # replace them with "_".
      obj_file_renames = {}
      for obj_file in obj_file_list:
        if "/" in obj_file or "\\" in obj_file:
          # Extracting this will fail because it contains subdirectories.
          obj_file_bin = obj_file.encode(DEFAULT_ENCODING)
          new_obj_file_bin = re.sub(b"[^0-9a-zA-Z_.]", b"_", obj_file_bin)
          old_obj_regex_bin = re.sub(b"[^0-9a-zA-Z_.]", b".", obj_file_bin)
          obj_file_renames[old_obj_regex_bin] = new_obj_file_bin
      if obj_file_renames:
        # We renamed some object files within the .a file.
        input_path[1] = replace_strings_in_archive(input_path[1],
                                                   obj_file_renames)

      logging.debug("Extracting input archive %s", input_path[1])
      (obj_file_list, errors) = extract_archive(input_path[1])
      if errors:
        logging.fatal("Error extracting archive %s: %s", input_path[1], errors)

      if symbol_redefinition_file:
        logging.debug("Redefining symbols in %d files", len(obj_file_list))

      for obj_file in obj_file_list:
        new_obj_file = "%s_%s" % (lib_name_hash, obj_file)
        if len(new_obj_file) > 255:
          # Filename is above the limit, hash the first N characters to bring
          # the length down to 255 or less.
          # Hashes are 32 characters plus an underscore.
          # If len = 256 hash the first 34 chars.
          # If len = 257 hash the first 35 chars.
          split = len(new_obj_file) - (255 - 32 - 1)
          new_obj_file = "%s_%s" % (hashlib.md5(
              new_obj_file[:split].encode(DEFAULT_ENCODING)).hexdigest(),
                                    new_obj_file[split:])

        # Move the .o file to its new name, renaming symbols if needed.
        move_object_file(obj_file, new_obj_file, symbol_redefinition_file.name
                         if symbol_redefinition_file else None)
        all_obj_files.append(new_obj_file)

    symbol_redefinition_file = None

    # Remove any existing output lib so we can create a new one from scratch.
    if os.path.isfile(output_path):
      os.remove(output_path)

    if (FLAGS.skip_creating_archives):
      output_path_dir = output_path + ".dir"
      logging.debug("Copying object files to %s", output_path_dir)
      if not os.path.exists(output_path_dir):
        os.makedirs(output_path_dir)
      for obj_file in all_obj_files:
        logging.debug("Copy %s to %s" % (obj_file, os.path.join(output_path_dir, os.path.basename(obj_file))))
        shutil.copyfile(obj_file, os.path.join(output_path_dir, os.path.basename(obj_file)))
    else:
      logging.debug("Creating output archive %s", output_path)
      create_archive(output_path, all_obj_files, input_path[1])

    shutdown_cache()

  except Exception as e:
    logging.error("Got error: %s", e)
    raise
  finally:
    if working_root:
      shutil.rmtree(working_root)

  return 0


if __name__ == "__main__":
  flags.mark_flag_as_required("output")
  flags.mark_flag_as_required("platform")
  app.run(main)
