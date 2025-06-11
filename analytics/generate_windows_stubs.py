#!/usr/bin/env python3
# Copyright 2025 Google LLC
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

"""Generate stubs and function pointers for Windows SDK"""

import argparse
import os
import re
import sys

HEADER_GUARD_PREFIX = "FIREBASE_ANALYTICS_SRC_WINDOWS_"
INCLUDE_PATH = "src/windows/"
INCLUDE_PREFIX = "analytics/" + INCLUDE_PATH
COPYRIGHT_NOTICE = """// Copyright 2025 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

"""

def generate_function_pointers(header_file_path, output_h_path, output_c_path):
    """
    Parses a C header file to generate a self-contained header with typedefs,
    extern function pointer declarations, and a source file with stub functions,
    initialized pointers, and a dynamic loading function for Windows.

    Args:
        header_file_path (str): The path to the input C header file.
        output_h_path (str): The path for the generated C header output file.
        output_c_path (str): The path for the generated C source output file.
    """
    print(f"Reading header file: {header_file_path}")
    try:
        with open(header_file_path, 'r', encoding='utf-8') as f:
            header_content = f.read()
    except FileNotFoundError:
        print(f"Error: Header file not found at '{header_file_path}'")
        return

    # --- Extract necessary definitions from the original header ---

    # Find all standard includes (e.g., <stdint.h>)
    includes = re.findall(r"#include\s+<.*?>", header_content)

    # Find all typedefs, including their documentation comments
    typedefs = re.findall(r"/\*\*(?:[\s\S]*?)\*/\s*typedef[\s\S]*?;\s*", header_content)

    # --- Extract function prototypes ---
    function_pattern = re.compile(
        r"ANALYTICS_API\s+([\w\s\*]+?)\s+(\w+)\s*\((.*?)\);",
        re.DOTALL
    )
    matches = function_pattern.finditer(header_content)

    extern_declarations = []
    stub_functions = []
    pointer_initializations = []
    function_details_for_loader = []

    for match in matches:
        return_type = match.group(1).strip()
        function_name = match.group(2).strip()
        params_str = match.group(3).strip()
        
        cleaned_params_for_decl = re.sub(r'\s+', ' ', params_str) if params_str else ""
        stub_name = f"Stub_{function_name}"

        # Generate return statement for the stub
        if "void" in return_type:
            return_statement = "    // No return value."
        elif "*" in return_type:
            return_statement = "    return NULL;"
        else: # bool, int64_t, etc.
            return_statement = "    return 0;"
            
        stub_function = (
            f"// Stub for {function_name}\n"
            f"static {return_type} {stub_name}({params_str}) {{\n"
            f"{return_statement}\n"
            f"}}"
        )
        stub_functions.append(stub_function)

        declaration = f"extern {return_type} (*ptr_{function_name})({cleaned_params_for_decl});"
        extern_declarations.append(declaration)

        pointer_init = f"{return_type} (*ptr_{function_name})({cleaned_params_for_decl}) = &{stub_name};"
        pointer_initializations.append(pointer_init)
        
        function_details_for_loader.append((function_name, return_type, cleaned_params_for_decl))

    print(f"Found {len(pointer_initializations)} functions. Generating output files...")

    # --- Write the self-contained Header File (.h) ---
    header_guard = f"{HEADER_GUARD_PREFIX}{os.path.basename(output_h_path).upper().replace('.', '_')}_"
    with open(output_h_path, 'w', encoding='utf-8') as f:
        f.write(f"{COPYRIGHT_NOTICE}")
        f.write(f"// Generated from {os.path.basename(header_file_path)} by {os.path.basename(sys.argv[0])}\n\n")
        f.write(f"#ifndef {header_guard}\n")
        f.write(f"#define {header_guard}\n\n")

        f.write("// --- Copied from original header ---\n")
        f.write("\n".join(includes) + "\n\n")
        f.write("".join(typedefs))
        f.write("// --- End of copied section ---\n\n")
        
        f.write("#ifdef __cplusplus\n")
        f.write('extern "C" {\n')
        f.write("#endif\n\n")
        f.write("// --- Function Pointer Declarations ---\n")
        f.write("\n".join(extern_declarations))
        f.write("\n\n// --- Dynamic Loader Declaration for Windows ---\n")
        f.write("#if defined(_WIN32)\n")
        f.write('#include <windows.h> // For HMODULE\n')
        f.write('// Load Google Analytics functions from the given DLL handle into function pointers.\n')
        f.write(f'// Returns the number of functions successfully loaded (out of {len(function_details_for_loader)}).\n')
        f.write("int FirebaseAnalytics_LoadAnalyticsFunctions(HMODULE dll_handle);\n\n")
        f.write('// Reset all function pointers back to stubs.\n')
        f.write("void FirebaseAnalytics_UnloadAnalyticsFunctions(void);\n\n")
        f.write("#endif // defined(_WIN32)\n")
        f.write("\n#ifdef __cplusplus\n")
        f.write("}\n")
        f.write("#endif\n\n")
        f.write(f"#endif  // {header_guard}\n")

    print(f"Successfully generated header file: {output_h_path}")

    # --- Write the Source File (.c) ---
    with open(output_c_path, 'w', encoding='utf-8') as f:
        f.write(f"{COPYRIGHT_NOTICE}")
        f.write(f"// Generated from {os.path.basename(header_file_path)} by {os.path.basename(sys.argv[0])}\n\n")
        f.write(f'#include "{INCLUDE_PREFIX}{os.path.basename(output_h_path)}"\n')
        f.write('#include <stddef.h> // For NULL\n\n')
        f.write("// --- Stub Function Definitions ---\n")
        f.write("\n\n".join(stub_functions))
        f.write("\n\n\n// --- Function Pointer Initializations ---\n")
        f.write("\n".join(pointer_initializations))
        f.write("\n\n// --- Dynamic Loader Function for Windows ---\n")
        loader_lines = [
            '#if defined(_WIN32)',
            'int FirebaseAnalytics_LoadAnalyticsFunctions(HMODULE dll_handle) {',
            '    int count = 0;\n',
            '    if (!dll_handle) {',
            '        return count;',
            '    }\n'
        ]
        for name, ret_type, params in function_details_for_loader:
            pointer_type_cast = f"({ret_type} (*)({params}))"
            proc_check = [
                f'    FARPROC proc_{name} = GetProcAddress(dll_handle, "{name}");',
                f'    if (proc_{name}) {{',
                f'        ptr_{name} = {pointer_type_cast}proc_{name};',
                f'        count++;',
                f'    }}'
            ]
            loader_lines.extend(proc_check)
        loader_lines.append('\n    return count;')
        loader_lines.append('}\n')
        loader_lines.append('void FirebaseAnalytics_UnloadAnalyticsFunctions(void) {')
        for name, ret_type, params in function_details_for_loader:
            loader_lines.append(f'    ptr_{name} = &Stub_{name};');
        loader_lines.append('}\n')
        loader_lines.append('#endif // defined(_WIN32)\n')
        f.write('\n'.join(loader_lines))

    print(f"Successfully generated C source file: {output_c_path}")


if __name__ == '__main__':
    parser = argparse.ArgumentParser(
        description="Generate C stubs and function pointers from a header file."
    )
    parser.add_argument(
        "--windows_header",
        default = os.path.join(os.path.dirname(sys.argv[0]), "windows/include/public/c/analytics.h"),
        #required=True,
        help="Path to the input C header file."
    )
    parser.add_argument(
        "--output_header",
        default = os.path.join(os.path.dirname(sys.argv[0]), INCLUDE_PATH, "analytics_dynamic.h"),
        #required=True,
        help="Path for the generated output header file."
    )
    parser.add_argument(
        "--output_source",
        default = os.path.join(os.path.dirname(sys.argv[0]), INCLUDE_PATH, "analytics_dynamic.c"),
        #required=True,
        help="Path for the generated output source file."
    )
    
    args = parser.parse_args()
    
    generate_function_pointers(
        args.windows_header, 
        args.output_header, 
        args.output_source
    )
