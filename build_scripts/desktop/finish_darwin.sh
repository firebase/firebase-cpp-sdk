#!/bin/bash

# Copyright 2020 Google LLC
#
# Script to finish packaging individual iOS/Mac libraries into
# universal libraries, and then into frameworks.
set -e

if [[ "$(uname)" != "Darwin" ]]; then
    echo "Error: Frameworks and universal libraries can only be built on a Mac host."
    exit 2
fi

packagepath="$1"
includepath="$2"
binutils="$3"
os="$4"

if [[ -z "${os}" ]]; then
    os=darwin
fi

if [[ -z "${packagepath}" || -z "${includepath}" || -z "${binutils}" ]]; then
    echo "Usage: $0 <packaged iOS/Mac SDK path> <include file path> <binutils path> [darwin|ios]"
    exit 2
fi

if [[ ! -d "${packagepath}/libs/${os}" ]]; then
    echo "Can't find packaged iOS/Mac SDK in ${packagepath}/libs/${os}"
    exit 2
fi

if [[ ! -r "${includepath}/firebase/app.h" ]]; then
    echo "Can't find include files in ${includepath}"
    exit 2
fi

if [[ ! -x "${binutils}/ar" ]]; then
    echo "Packaging tools not found at path '${binutils}'."
    exit 2
fi

packagepath="$(cd "${packagepath}" && pwd -P)"
includepath="$(cd "${includepath}" && pwd -P)"
binutils="$(cd "${binutils}" && pwd -P)"

declare -a architectures=()
for arch_dir in "${packagepath}/libs/${os}"/*; do
    if [[ -d "${arch_dir}" ]]; then
        a="$(basename "${arch_dir}")"
        if [[ "${a}" != "universal" ]]; then
            architectures+=("${a}")
        fi
    fi
done

if [[ "${#architectures[@]}" -eq 0 ]]; then
    echo "Error: No architecture directories found in ${packagepath}/libs/${os}"
    exit 2
fi

# Don't lipo if we already have universal libraries, as that
# means that the lipo step has already occurred.
if [[ ! -d "${packagepath}/libs/${os}/universal" ]]; then
    echo "Repackaging libraries using Mac libtool..."
    for arch in "${architectures[@]}"; do
        for lib in "${packagepath}/libs/${os}/${arch}"/*.a; do
            [[ -f "${lib}" ]] || continue
            pushd "$(dirname "${lib}")" > /dev/null
            libname="$(basename "${lib}")"
            rm -rf "${libname}.dir"
            mkdir -p "${libname}.dir"
            cd "${libname}.dir"
            member_index=0
            seen_members=()
            is_gnu_ar=0
            if "${binutils}/ar" --version 2>/dev/null | grep -Eqi "GNU|LLVM"; then
                is_gnu_ar=1
            else
                cp -f "../${libname}" "tmp_${libname}"
            fi
            while IFS= read -r member; do
                [[ -n "${member}" && "${member}" != __.SYMDEF* ]] || continue
                member_index=$((member_index + 1))
                member_base="$(basename "${member}")"
                seen_members+=("${member}")
                # Count which occurrence of this member name we are extracting.
                occurrence=0
                for m in "${seen_members[@]}"; do
                    if [[ "${m}" == "${member}" ]]; then
                        occurrence=$((occurrence + 1))
                    fi
                done
                if [[ "${is_gnu_ar}" -eq 1 ]]; then
                    "${binutils}/ar" xN "${occurrence}" "../${libname}" "${member}"
                else
                    if [[ "${occurrence}" -gt 1 ]]; then
                        "${binutils}/ar" d "tmp_${libname}" "${member}"
                    fi
                    "${binutils}/ar" x "tmp_${libname}" "${member}"
                fi
                mv -f "${member_base}" "${member_index}_${member_base}"
            done < <("${binutils}/ar" t "../${libname}")
            rm -f "tmp_${libname}"
            mv -f "../${libname}" "../${libname}.bak"
            xcrun libtool -static -o "../${libname}" -no_warning_for_no_symbols -s ./*.o
            cd ..
            rm -rf "${libname}.dir"
            popd > /dev/null
        done
    done

    # Merge into universal libs
    mkdir -p "${packagepath}/libs/${os}/universal"
    echo "Creating universal libraries..."
    for lib in "${packagepath}/libs/${os}/${architectures[0]}"/*.a; do
        [[ -f "${lib}" ]] || continue
        libname="$(basename "${lib}")"
        arch_libs=()
        for arch in "${architectures[@]}"; do
            if [[ -f "${packagepath}/libs/${os}/${arch}/${libname}" ]]; then
                arch_libs+=("${packagepath}/libs/${os}/${arch}/${libname}")
            fi
        done
        xcrun lipo "${arch_libs[@]}" -create -output "${packagepath}/libs/${os}/universal/${libname}"
    done

    # Done, remove old library backups.
    rm -f "${packagepath}/libs/${os}"/*/*.bak

else
    echo "Universal libraries already found, skipping..."
fi

# Now make frameworks.
echo "Creating frameworks..."
rm -rf "${packagepath}/frameworks/${os}"
mkdir -p "${packagepath}/frameworks/${os}"
for arch in universal "${architectures[@]}"; do
    for library in "${packagepath}/libs/${os}/${arch}/"*.a; do
        [[ -f "${library}" ]] || continue
        framework="$(basename "${library}" | sed 's|^lib||' | sed 's|\.a$||')"
        add_headers=0
        if [[ "${framework}" == "firebase_app" ]]; then
            framework="firebase"
            add_headers=1
            echo "Need to add extra Headers"
        fi
        framework_dir="${packagepath}/frameworks/${os}/${arch}/${framework}.framework"
        mkdir -p "${framework_dir}"
        cp -f "${library}" "${framework_dir}/${framework}"
        if [[ "${add_headers}" -eq 1 ]]; then
            echo "Adding extra Headers to framework ${framework_dir}"
            mkdir -p "${framework_dir}/Headers"
            cp -af "${includepath}/firebase/"* "${framework_dir}/Headers/"
            if [[ -d "${packagepath}/include/firebase" && "${includepath}" != "${packagepath}/include" ]]; then
                cp -af "${packagepath}/include/firebase/"* "${framework_dir}/Headers/"
            fi
        fi
    done
done
echo "Done!"
