#!/bin/bash

# Copyright 2020 Google LLC
#
# Script to test merge_libraries script via merge_libraries_test.

set -e

usage(){
    echo "Usage: $0 [options]
options:
  -t, packaging tools directory                   default: ~/bin
  -P, python command                              default: python3
  -L, use LLVM binutils
example:
  scripts/merge_libraries_test.sh -t ~/llvm-binutils -L"
}

use_llvm_binutils=0
python_cmd=python3
tools_path=~/bin
script_dir=$(cd $(dirname $0); pwd -P)

while getopts "t:P:Lh" opt; do
    case $opt in
        L)
            use_llvm_binutils=1
            ;;
        P)
            python_cmd=$OPTARG
            ;;
        t)
            tools_path=$OPTARG
            ;;
        h)
            usage
            exit 0
            ;;
        *)
            usage
            exit 2
            ;;
    esac
done

readonly demangle_cmds=${tools_path}/c++filt,${tools_path}/demumble
if [[ ${use_llvm_binutils} -eq 1 ]]; then
    readonly binutils_objcopy=${tools_path}/llvm-objcopy
    readonly binutils_nm=${tools_path}/llvm-nm
    readonly binutils_ar=${tools_path}/llvm-ar
else    
    readonly binutils_objcopy=${tools_path}/objcopy
    if [[ -x ${tools_path}/nm-new ]] ; then
        readonly binutils_nm=${tools_path}/nm-new
    else
	readonly binutils_nm=${tools_path}/nm
    fi
    readonly binutils_ar=${tools_path}/ar
fi

declare -a merge_libraries_params
merge_libraries_params=(
    --binutils_nm_cmd=${binutils_nm}
    --binutils_ar_cmd=${binutils_ar}
    --binutils_objcopy_cmd=${binutils_objcopy}
    --demangle_cmds=${demangle_cmds}
)

if [[ $(uname) == "Darwin" ]]; then
    platform=darwin
else
    platform=linux
fi

rm -f merge_libraries_test_file.a test_file.o
set -x
"${python_cmd}" "${script_dir}/merge_libraries_test.py" --platform ${platform} ${merge_libraries_params[*]}
