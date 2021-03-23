# Copyright 2021 Google LLC
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

"""Patches uWebsockets Socket.h to fix a crash sending large
payloads with ssl encoding.
"""
from absl import app
from absl import flags

FLAGS = flags.FLAGS

flags.DEFINE_string("file", None, "Sockets.h file to patch.")
flags.DEFINE_string("cmakefile", None, "cmake file which downloaded uWebSockets")

# The line of Socket.h that we intend to start overwriting, and the 
# number of lines to overwite. 
# See: https://github.com/uNetworking/uWebSockets/blob/e402f022a43d283c4fb7c809332951c3108d6906/src/Socket.h#L309
REPLACE_BLOCK_START_LINE = 309
REPLACE_BLOCK_OVERWRITE_LENGTH = 17

# The code which fixes the crash which will replace the existing
# error handling implementation with one that retries the writes
# when the SSL encoding buffer is full.
REPLACE_BLOCK_CODE_LINES = [
  "              bool continue_loop = true;\n",
  "              do {\n",
  "                sent = SSL_write(ssl, message->data, message->length);\n",
  "                if (sent == (ssize_t) message->length) {\n",
  "                  wasTransferred = false;\n",
  "                  return true;\n",
  "                } else if (sent < 0) {\n",
  "                  switch (SSL_get_error(ssl, sent)) {\n",
  "                  case SSL_ERROR_WANT_READ:\n",
  "                    continue_loop = false;\n",
  "                    break;\n",
  "                  case SSL_ERROR_WANT_WRITE:\n",
  "                    break;\n",
  "                  default:\n",
  "                    return false;\n",
  "                }\n",
  "              }\n",
  "            }  while (continue_loop);\n"
]

# Confirms that the version of uWebsockets that cmake checked-out
# is the same that version that this patch is meant for.
UWEBSOCKETS_COMMIT = "4d94401b9c98346f9afd838556fdc7dce30561eb"
VERSION_MISMATCH_ERROR = \
  "\n\n ERROR patch_websockets.py: patching wrong uWebsockets Version!\n\n"

def main(argv):
  """Patches uWebSockets' Socket.h file to fix a crash when attempting
  large writes.
  
  This code simply replaces one block of code (lines 308-325) with our
  custom code defined in REPLACE_BLOCK_CODE_LINES above.
  """

  with open(FLAGS.cmakefile,'r') as cmake_file:
    if UWEBSOCKETS_COMMIT not in cmake_file.read():
        raise Exception(VERSION_MISMATCH_ERROR)

  with open(FLAGS.file,'r') as sockets_file:
    lines = sockets_file.readlines()
    sockets_file.close()

  ignore_lines_count = 0
  with open(FLAGS.file,'w') as sockets_file:
    index = 0
    for line in lines:
      if(ignore_lines_count != 0):
        ignore_lines_count -= 1
        index+=1
        continue

      if( index+1 == REPLACE_BLOCK_START_LINE ):
        sockets_file.writelines(REPLACE_BLOCK_CODE_LINES)
        ignore_lines_count = REPLACE_BLOCK_OVERWRITE_LENGTH
        index+=1
        continue
      
      sockets_file.write(line)
      index+=1
    
    sockets_file.close()  

if __name__ == "__main__":
  flags.mark_flag_as_required("file")
  flags.mark_flag_as_required("cmakefile")
  app.run(main)
