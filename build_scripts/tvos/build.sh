#!/bin/bash -e

# Copyright 2021 Google LLC
#
# Script to build iOS XCFrameworks
# If built for all architectures (arm64 x86_64),
# it will build universal framework as well
#

usage(){
    echo "Usage: $0 [options]
 options:
   -b, build path              default: tvos_build
   -s, source path             default: .
   -p, framework platform      default: ${SUPPORTED_PLATFORMS[@]}
   -a, framework architecture  default: ${SUPPORTED_ARCHITECTURES[@]}
   -t, CMake target            default: ${SUPPORTED_TARGETS[@]}
   -g, generate Makefiles      default: true
   -c, CMake build             default: true
 example: 
   build_scripts/tvos/build.sh -b tvos_build -s . -a arm64,x86_64 -t firebase_database,firebase_auth -c false"
}

set -e

readonly SUPPORTED_PLATFORMS=(device simulator)
readonly SUPPORTED_ARCHITECTURES=(arm64 x86_64)
readonly DEVICE_ARCHITECTURES=(arm64)
readonly SIMULATOR_ARCHITECTURES=(x86_64)
readonly SUPPORTED_TARGETS=(firebase_analytics firebase_auth firebase_database firebase_firestore firebase_functions firebase_installations firebase_messaging firebase_remote_config firebase_storage)

# build default value
buildpath="tvos_build"
sourcepath="."
platforms=("${SUPPORTED_PLATFORMS[@]}")
architectures=("${SUPPORTED_ARCHITECTURES[@]}")
targets=("${SUPPORTED_TARGETS[@]}")
generateMakefiles=true
cmakeBuild=true

# check options
IFS=',' # split options on ',' characters
while getopts ":b:s:a:t:g:ch" opt; do
    case $opt in
        h)
            usage
            exit 0
            ;;
        b)
            buildpath=$OPTARG
            ;;
        s)
            sourcepath=$OPTARG
            if [[ ! -d "${sourcepath}" ]]; then
                echo "Source path ${sourcepath} not found."
                exit 2
            fi
            ;;
        p)
            platforms=($OPTARG)
            for platform in ${platforms[@]}; do
                if [[ ! " ${SUPPORTED_PLATFORMS[@]} " =~ " ${platform} " ]]; then
                    echo "invalid platform: ${platform}"
                    echo "Supported platforms are: ${SUPPORTED_PLATFORMS[@]}"
                    exit 2
                fi
            done
            ;;
        a)
            architectures=($OPTARG)
            for arch in ${architectures[@]}; do
                if [[ ! " ${SUPPORTED_ARCHITECTURES[@]} " =~ " ${arch} " ]]; then
                    echo "invalid architecture: ${arch}"
                    echo "Supported architectures are: ${SUPPORTED_ARCHITECTURES[@]}"
                    exit 2
                fi
            done
            ;;
        t)
            targets=($OPTARG)
            for t in ${targets[@]}; do
                if [[ ! " ${SUPPORTED_TARGETS[@]} " =~ " ${t} " ]]; then
                    echo "invalid target: ${t}"
                    echo "Supported targets are: ${SUPPORTED_TARGETS[@]}"
                    exit 2
                fi
            done
            ;;
        g)
            if [[ $OPTARG == true ]]; then
                generateMakefiles=true
            else
                generateMakefiles=false
            fi
            ;;
        c)
            if [[ $OPTARG == true ]]; then
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
echo "build platforms: ${platforms[@]}"
echo "build architectures: ${architectures[@]}"
echo "build targets: ${targets[@]}"
echo "generate Makefiles: ${generateMakefiles}"
echo "CMake Build: ${cmakeBuild}"
sourcepath=$(cd ${sourcepath} && pwd)   #full path
buildpath=$(mkdir -p ${buildpath} && cd ${buildpath} && pwd)    #full path

# generate Makefiles for each architecture and target
frameworkspath="frameworks/tvos"
tvos_toolchain_platform="TVOS"
if ${generateMakefiles}; then
    for platform in ${platforms[@]}; do 
        for arch in ${architectures[@]}; do 
            if [[ "${platform}" == "device" && " ${DEVICE_ARCHITECTURES[@]} " =~ " ${arch} " ]]; then
                toolchain="cmake/toolchains/apple.toolchain.cmake"
            elif [[ "${platform}" == "simulator" && " ${SIMULATOR_ARCHITECTURES[@]} " =~ " ${arch} " ]]; then
                toolchain="cmake/toolchains/apple.toolchain.cmake"
                tvos_toolchain_platform="SIMULATOR_TVOS"
            else
                continue
            fi

            echo "generate Makefiles start"
            mkdir -p ${buildpath}/tvos_build_file/${platform}-${arch} && cd ${buildpath}/tvos_build_file/${platform}-${arch}
            cmake -DCMAKE_TOOLCHAIN_FILE=${sourcepath}/${toolchain} \
                  -DPLATFORM=${tvos_toolchain_platform} \
                  -DCMAKE_ARCHIVE_OUTPUT_DIRECTORY=${buildpath}/${frameworkspath}/${platform}-${arch} \
                ${sourcepath}
            echo "generate Makefiles end"
        done
    done
fi

# build framework for each architecture and target
IFS=$'\n' # split $(ls) on \n characters
if ${cmakeBuild}; then
    for platform in ${platforms[@]}; do 
        for arch in ${architectures[@]}; do 
            if [ -d "${buildpath}/tvos_build_file/${platform}-${arch}" ]; then
            {
                cd ${buildpath}/tvos_build_file/${platform}-${arch}
                echo "build ${platform} ${arch} ${targets[@]} framework start"
                cmake --build . --target ${targets[@]}
                echo "build ${platform} ${arch} ${targets[@]} framework end"
            
            } &
            fi
        done
    done
    subprocess_fail=0
    for job in $(jobs -p); do
        wait $job || let "subprocess_fail+=1"
    done
    if [ "${subprocess_fail}" == "0" ]; then
        echo "frameworks build end"
    else
        echo "frameworks build error, ${subprocess_fail} architecture(s) build failed"
        exit 2
    fi

    # arrange the framework 
    cd ${buildpath}/${frameworkspath}
    for platform in ${platforms[@]}; do 
        for arch in ${architectures[@]}; do
            if [[ ! -d "${platform}-${arch}" ]]; then
                continue
            fi

            # rename firebase_app to firebase
            if [[ ! -d "${platform}-${arch}/firebase.framework" ]]; then
                mv ${platform}-${arch}/firebase_app.framework ${platform}-${arch}/firebase.framework
                mv ${platform}-${arch}/firebase.framework/firebase_app ${platform}-${arch}/firebase.framework/firebase
                rm ${platform}-${arch}/firebase.framework/Info.plist
            fi

            # delete useless Info.plist
            for target in ${targets[@]}; do
                if [[ -f "${platform}-${arch}/${target}.framework/Info.plist" ]]; then
                    rm ${platform}-${arch}/${target}.framework/Info.plist
                fi
            done

            # delete non-framework dir
            for dir in $(ls ${platform}-${arch}); do
                if [[ ! ${dir} =~ ".framework" ]]; then
                    rm -rf ${platform}-${arch}/${dir}
                fi
            done
        done
    done

    # if we built for all architectures (arm64 armv7 x86_64 i386)
    # build universal framework as well
    if [[ ${#architectures[@]} < ${#SUPPORTED_ARCHITECTURES[@]} ]]; then
        exit 0
    fi

    targets+=('firebase')
    for target in ${targets[@]}; do
        mkdir -p universal/${target}.framework
        libsubpath="${target}.framework/${target}"
        lipo -create "device-arm64/${libsubpath}" \
                     "simulator-x86_64/${libsubpath}" \
            -output "universal/${libsubpath}"
    done
    if [[ ! -d "universal/firebase.framework/Headers" ]]; then
        cp -R device-arm64/firebase.framework/Headers universal/firebase.framework
    fi
    echo "universal frameworks build end & ready to use"

    # covert framework into xcframework
    cd ${buildpath}
    xcframeworkspath="xcframeworks"
    mkdir -p ${xcframeworkspath}
    # create library for xcframework
    for platform in ${platforms[@]}; do 
        for target in ${targets[@]}; do
            libsubpath="${target}.framework/${target}" 
            if [[ "${platform}" == "device" ]]; then
                outputdir="${xcframeworkspath}/${target}.xcframework/tvos-arm64/${target}.framework"
                mkdir -p ${outputdir}
                lipo -create "${frameworkspath}/device-arm64/${libsubpath}" \
                    -output "${outputdir}/${target}"

            elif [[ "${platform}" == "simulator" ]]; then
                outputdir="${xcframeworkspath}/${target}.xcframework/tvos-x86_64-simulator/${target}.framework"
                mkdir -p ${outputdir}
                lipo -create "${frameworkspath}/simulator-x86_64/${libsubpath}" \
                    -output "${outputdir}/${target}"
            fi
        done
    done

    # create Info.plist for xcframework
    for target in ${targets[@]}; do                   
        cp ${sourcepath}/build_scripts/tvos/Info.plist ${xcframeworkspath}/${target}.xcframework
        sed -i "" "s/LIBRARY_PATH/${target}.framework/" ${xcframeworkspath}/${target}.xcframework/Info.plist
    done

    # create Headers for xcframework
    if [[ ! -d "${xcframeworkspath}/firebase.xcframework/tvos-arm64/firebase.framework/Headers" ]]; then
        cp -R ${frameworkspath}/device-arm64/firebase.framework/Headers  \
            ${xcframeworkspath}/firebase.xcframework/tvos-arm64/firebase.framework/Headers
        cp -R ${frameworkspath}/device-arm64/firebase.framework/Headers  \
            ${xcframeworkspath}/firebase.xcframework/tvos-x86_64-simulator/firebase.framework/Headers
    fi
    echo "xcframeworks build end & ready to use"
fi
