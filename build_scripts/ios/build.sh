#!/bin/bash
#
# Script to build iOS Frameworks
# If built for all architectures (arm64 armv7 x86_64 i386),
# it will build universal framework as well
#
# Usage: ./build.sh [options]
# options:
#   -b, build path              default: ios_build
#   -s, source path             default: .
#   -a, framework architecture  default: SUPPORTED_ARCHITECTURES
#   -t, CMake target            default: SUPPORTED_TARGETS
# example: 
#   build_scripts/ios/build.sh -b ios_build -s . -a arm64,x86_64 -t firebase_admob,firebase_auth

set -e

readonly SUPPORTED_ARCHITECTURES=(arm64 armv7 x86_64 i386)
readonly SUPPORTED_TARGETS=(firebase_admob firebase_analytics firebase_auth firebase_database firebase_dynamic_links firebase_firestore firebase_functions firebase_instance_id firebase_messaging firebase_remote_config firebase_storage)

# build default value
buildpath="ios_build"
sourcepath="."
architectures=("${SUPPORTED_ARCHITECTURES[@]}")
targets=("${SUPPORTED_TARGETS[@]}")

# check options
IFS=',' # split options on ',' characters
while getopts ":b:s:a:t:" opt; do
    case $opt in
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
        *)
            echo "unknow parameter"
            exit 2
            ;;
    esac
done
echo "build path: ${buildpath}"
echo "source path: ${sourcepath}"
echo "build architectures: ${architectures[@]}"
echo "build targets: ${targets[@]}"
sourcepath=$(cd ${sourcepath} && pwd)   #full path
buildpath=$(mkdir -p ${buildpath} && cd ${buildpath} && pwd)    #full path

# build framework for each architecture and target
frameworkspath="frameworks/ios"
for arch in ${architectures[@]}; do 
{
    cd ${buildpath}
    echo "build ${arch} framework start"
    mkdir -p build_file/${arch} && cd build_file/${arch}
    if [[ "${arch}" == "arm64" || "${arch}" == "armv7" ]]; then
        toolchain="cmake/toolchains/ios.cmake"
    elif [[ "${arch}" == "x86_64" || "${arch}" == "i386" ]]; then
        toolchain="cmake/toolchains/ios_simulator.cmake"
    fi
    cmake -DCMAKE_TOOLCHAIN_FILE=${sourcepath}/${toolchain} \
        -DCMAKE_OSX_ARCHITECTURES=${arch} \
        -DCMAKE_ARCHIVE_OUTPUT_DIRECTORY=${buildpath}/${frameworkspath}/${arch} \
        ${sourcepath}
    cmake --build . --target ${targets[@]}

    echo "build ${arch} framework end"
} &
done
wait
echo "${architectures[@]} frameworks build end"

# arrange the framework 
IFS=$'\n' # split $(ls) on \n characters
cd ${buildpath}/${frameworkspath}
for arch in ${architectures[@]}; do
    mv ${arch}/firebase_app.framework ${arch}/firebase.framework
    mv ${arch}/firebase.framework/firebase_app ${arch}/firebase.framework/firebase

    files=$(ls ${arch})
    for f in ${files}; do
        if [[ ${f} =~ ".framework" ]]; then
            rm ${arch}/${f}/Info.plist
        else 
            rm -rf ${arch}/${f}
        fi
    done
done
echo "${architectures[@]} frameworks ready to use"

# if we built for all architectures (arm64 armv7 x86_64 i386)
# build universal framework as well
if [[ ${#architectures[@]} < ${#SUPPORTED_ARCHITECTURES[@]} ]]; then
    exit 0
fi

frameworks=$(ls arm64)
for framework in ${frameworks}; do
    mkdir -p universal/${framework}
    libbasename="$(basename "${framework}" .framework)"
    libsubpath="${framework}/${libbasename}"
    if [[ -f "arm64/${libsubpath}" ]]; then
        lipo -create "arm64/${libsubpath}" "armv7/${libsubpath}" "x86_64/${libsubpath}" "i386/${libsubpath}" \
            -output "universal/${libsubpath}"
    fi
done
cp -R arm64/firebase.framework/Headers universal/firebase.framework
echo "universal frameworks build end & ready to use"
