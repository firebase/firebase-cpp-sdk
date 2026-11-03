#!/bin/bash -e

# Copyright 2020 Google LLC
#
# Script to build iOS XCFrameworks
# If built for all architectures (arm64 x86_64),
# it will build universal framework as well
#

set -e

readonly SUPPORTED_PLATFORMS=(device simulator)
readonly SUPPORTED_ARCHITECTURES=(arm64 x86_64)
readonly DEVICE_ARCHITECTURES=(arm64)
readonly SIMULATOR_ARCHITECTURES=(arm64 x86_64)
readonly SUPPORTED_TARGETS=(
    firebase_ai
    firebase_analytics
    firebase_app
    firebase_app_check
    firebase_auth
    firebase_database
    firebase_firestore
    firebase_functions
    firebase_gma
    firebase_installations
    firebase_messaging
    firebase_performance
    firebase_remote_config
    firebase_storage
    firebase_ump
)

usage() {
    echo "Usage: $0 [options]
 options:
   -b, build path              default: ios_build
   -s, source path             default: .
   -p, framework platform      default: ${SUPPORTED_PLATFORMS[*]}
   -a, framework architecture  default: ${SUPPORTED_ARCHITECTURES[*]}
   -t, CMake target            default: ${SUPPORTED_TARGETS[*]}
   -g, generate Makefiles      default: true
   -c, CMake build             default: true
 example:
   build_scripts/ios/build.sh -b ios_build -s . -a arm64,x86_64 -t firebase_gma,firebase_auth -c false"
}

contains_element() {
    local needle="$1"
    shift
    local item
    for item in "$@"; do
        if [[ "${item}" == "${needle}" ]]; then
            return 0
        fi
    done
    return 1
}

# build default value
buildpath="ios_build"
sourcepath="."
platforms=("${SUPPORTED_PLATFORMS[@]}")
architectures=("${SUPPORTED_ARCHITECTURES[@]}")
targets=("${SUPPORTED_TARGETS[@]}")
generateMakefiles=true
cmakeBuild=true

# check options
while getopts ":b:s:p:a:t:g:c:h" opt; do
    case "${opt}" in
        h)
            usage
            exit 0
            ;;
        b)
            buildpath="${OPTARG}"
            ;;
        s)
            sourcepath="${OPTARG}"
            if [[ ! -d "${sourcepath}" ]]; then
                echo "Source path ${sourcepath} not found."
                exit 2
            fi
            ;;
        p)
            IFS=',' read -r -a platforms <<< "${OPTARG}"
            for platform in "${platforms[@]}"; do
                if ! contains_element "${platform}" "${SUPPORTED_PLATFORMS[@]}"; then
                    echo "invalid platform: ${platform}"
                    echo "Supported platforms are: ${SUPPORTED_PLATFORMS[*]}"
                    exit 2
                fi
            done
            ;;
        a)
            IFS=',' read -r -a architectures <<< "${OPTARG}"
            for arch in "${architectures[@]}"; do
                if ! contains_element "${arch}" "${SUPPORTED_ARCHITECTURES[@]}"; then
                    echo "invalid architecture: ${arch}"
                    echo "Supported architectures are: ${SUPPORTED_ARCHITECTURES[*]}"
                    exit 2
                fi
            done
            ;;
        t)
            IFS=',' read -r -a targets <<< "${OPTARG}"
            for t in "${targets[@]}"; do
                if ! contains_element "${t}" "${SUPPORTED_TARGETS[@]}"; then
                    echo "invalid target: ${t}"
                    echo "Supported targets are: ${SUPPORTED_TARGETS[*]}"
                    exit 2
                fi
            done
            ;;
        g)
            if [[ "${OPTARG}" == "true" ]]; then
                generateMakefiles=true
            else
                generateMakefiles=false
            fi
            ;;
        c)
            if [[ "${OPTARG}" == "true" ]]; then
                cmakeBuild=true
            else
                cmakeBuild=false
            fi
            ;;
        *)
            echo "unknown parameter"
            exit 2
            ;;
    esac
done

echo "build path: ${buildpath}"
echo "source path: ${sourcepath}"
echo "build platforms: ${platforms[*]}"
echo "build architectures: ${architectures[*]}"
echo "build targets: ${targets[*]}"
echo "generate Makefiles: ${generateMakefiles}"
echo "CMake Build: ${cmakeBuild}"
sourcepath="$(cd "${sourcepath}" && pwd)"
buildpath="$(mkdir -p "${buildpath}" && cd "${buildpath}" && pwd)"

# generate Makefiles for each architecture and target
frameworkspath="frameworks/ios"
if [[ "${generateMakefiles}" == "true" ]]; then
    for platform in "${platforms[@]}"; do
        for arch in "${architectures[@]}"; do
            sysroot_arg=()
            if [[ "${platform}" == "device" ]] && contains_element "${arch}" "${DEVICE_ARCHITECTURES[@]}"; then
                sysroot_arg=() # Default iphoneos sysroot is correct for iOS devices
            elif [[ "${platform}" == "simulator" ]] && contains_element "${arch}" "${SIMULATOR_ARCHITECTURES[@]}"; then
                sysroot_arg=("-DCMAKE_OSX_SYSROOT=iphonesimulator")
            else
                continue # Skip invalid platform/OS combinations
            fi

            echo "generate Makefiles start"
            mkdir -p "${buildpath}/ios_build_file/${platform}-${arch}"
            cd "${buildpath}/ios_build_file/${platform}-${arch}"
            cmake -G Xcode -DCMAKE_SYSTEM_NAME=iOS \
                "${sysroot_arg[@]}" \
                "-DCMAKE_OSX_ARCHITECTURES=${arch}" \
                "-DCMAKE_ARCHIVE_OUTPUT_DIRECTORY=${buildpath}/${frameworkspath}/${platform}-${arch}" \
                "-DCMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG=${buildpath}/${frameworkspath}/${platform}-${arch}" \
                "-DCMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE=${buildpath}/${frameworkspath}/${platform}-${arch}" \
                "${sourcepath}"
            echo "generate Makefiles end"
        done
    done
fi

# build framework for each architecture and target
if [[ "${cmakeBuild}" == "true" ]]; then
    pids=()
    for platform in "${platforms[@]}"; do
        for arch in "${architectures[@]}"; do
            if [[ -d "${buildpath}/ios_build_file/${platform}-${arch}" ]]; then
                (
                    cd "${buildpath}/ios_build_file/${platform}-${arch}"
                    echo "build ${platform} ${arch} ${targets[*]} framework start"
                    cmake --build . --config Release --target "${targets[@]}"
                    echo "build ${platform} ${arch} ${targets[*]} framework end"
                ) &
                pids+=("$!")
            fi
        done
    done
    subprocess_fail=0
    for pid in "${pids[@]}"; do
        if ! wait "${pid}"; then
            subprocess_fail=$((subprocess_fail + 1))
        fi
    done
    if [[ "${subprocess_fail}" -eq 0 ]]; then
        echo "frameworks build end"
    else
        echo "frameworks build error, ${subprocess_fail} architecture(s) build failed"
        exit 2
    fi

    # Normalize target list for packaging (firebase_app is packaged as firebase).
    pkg_targets=()
    for target in "${targets[@]}"; do
        if [[ "${target}" != "firebase_app" ]]; then
            pkg_targets+=("${target}")
        fi
    done
    pkg_targets+=("firebase")

    # arrange the framework
    cd "${buildpath}/${frameworkspath}"
    header_source_dir=""
    for platform in "${platforms[@]}"; do
        for arch in "${architectures[@]}"; do
            slice_dir="${platform}-${arch}"
            if [[ ! -d "${slice_dir}" ]]; then
                continue
            fi

            # rename firebase_app to firebase
            if [[ -d "${slice_dir}/firebase_app.framework" ]]; then
                rm -rf "${slice_dir}/firebase.framework"
                mv "${slice_dir}/firebase_app.framework" "${slice_dir}/firebase.framework"
                mv "${slice_dir}/firebase.framework/firebase_app" "${slice_dir}/firebase.framework/firebase"
                rm -f "${slice_dir}/firebase.framework/Info.plist"
            fi

            if [[ -z "${header_source_dir}" && -d "${slice_dir}/firebase.framework/Headers" ]]; then
                header_source_dir="${buildpath}/${frameworkspath}/${slice_dir}/firebase.framework/Headers"
            fi

            # delete useless Info.plist
            for target in "${pkg_targets[@]}"; do
                if [[ -f "${slice_dir}/${target}.framework/Info.plist" ]]; then
                    rm -f "${slice_dir}/${target}.framework/Info.plist"
                fi
            done

            # delete non-framework dir
            for entry in "${slice_dir}"/*; do
                [[ -e "${entry}" ]] || continue
                if [[ "${entry}" != *.framework ]]; then
                    rm -rf "${entry}"
                fi
            done
        done
    done

    # Build legacy universal framework only when both device-arm64 and simulator-x86_64 slices were built.
    if [[ -d "device-arm64" && -d "simulator-x86_64" ]]; then
        for target in "${pkg_targets[@]}"; do
            mkdir -p "universal/${target}.framework"
            libsubpath="${target}.framework/${target}"
            lipo -create "device-arm64/${libsubpath}" \
                         "simulator-x86_64/${libsubpath}" \
                -output "universal/${libsubpath}"
        done
        if [[ -n "${header_source_dir}" ]]; then
            rm -rf "universal/firebase.framework/Headers"
            cp -R "${header_source_dir}" "universal/firebase.framework/Headers"
        fi
        echo "universal frameworks build end & ready to use"
    fi

    # Convert built framework slices into xcframeworks.
    cd "${buildpath}"
    xcframeworkspath="xcframeworks"
    mkdir -p "${xcframeworkspath}"
    for platform in "${platforms[@]}"; do
        for target in "${pkg_targets[@]}"; do
            libsubpath="${target}.framework/${target}"
            if [[ "${platform}" == "device" && -f "${frameworkspath}/device-arm64/${libsubpath}" ]]; then
                outputdir="${xcframeworkspath}/${target}.xcframework/ios-arm64/${target}.framework"
                mkdir -p "${outputdir}"
                lipo -create "${frameworkspath}/device-arm64/${libsubpath}" \
                    -output "${outputdir}/${target}"
            elif [[ "${platform}" == "simulator" ]]; then
                sim_inputs=()
                for sim_arch in "${SIMULATOR_ARCHITECTURES[@]}"; do
                    if [[ -f "${frameworkspath}/simulator-${sim_arch}/${libsubpath}" ]]; then
                        sim_inputs+=("${frameworkspath}/simulator-${sim_arch}/${libsubpath}")
                    fi
                done
                if [[ "${#sim_inputs[@]}" -gt 0 ]]; then
                    outputdir="${xcframeworkspath}/${target}.xcframework/ios-arm64_x86_64-simulator/${target}.framework"
                    mkdir -p "${outputdir}"
                    lipo -create "${sim_inputs[@]}" \
                        -output "${outputdir}/${target}"
                fi
            fi
        done
    done

    # create Info.plist for xcframework
    for target in "${pkg_targets[@]}"; do
        if [[ -d "${xcframeworkspath}/${target}.xcframework" ]]; then
            cp "${sourcepath}/build_scripts/ios/Info.plist" "${xcframeworkspath}/${target}.xcframework/Info.plist"
            sed -i "" "s/LIBRARY_PATH/${target}.framework/" "${xcframeworkspath}/${target}.xcframework/Info.plist"
        fi
    done

    # Copy Headers into all built firebase.xcframework slices.
    if [[ -n "${header_source_dir}" ]]; then
        for xc_slice in "ios-arm64" "ios-arm64_x86_64-simulator"; do
            target_fw_dir="${xcframeworkspath}/firebase.xcframework/${xc_slice}/firebase.framework"
            if [[ -d "${target_fw_dir}" ]]; then
                rm -rf "${target_fw_dir}/Headers"
                cp -R "${header_source_dir}" "${target_fw_dir}/Headers"
            fi
        done
    fi
    echo "xcframeworks build end & ready to use"
fi
