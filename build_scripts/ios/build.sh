#!/bin/bash

set -e

mkdir ios_framework_build && cd ios_framework_build

frameworks_path="frameworks/ios"
architectures=(arm64 armv7 x86_64 i386)
for arch in ${architectures[@]}
do
{   
    echo "build ${arch} start"
    toolchain="cmake/toolchains/ios.cmake"
    if [[ "${arch}" == "x86_64" || "${arch}" == "i386" ]]; then
        toolchain="cmake/toolchains/ios_simulator.cmake"
    fi

    mkdir ${arch}_build && cd ${arch}_build
    cmake -DCMAKE_TOOLCHAIN_FILE=../../${toolchain} \
        -DCMAKE_OSX_ARCHITECTURES=${arch} \
        -DCMAKE_ARCHIVE_OUTPUT_DIRECTORY=../../${frameworks_path}/${arch} \
        ../.. 
    cmake --build .
    echo "build ${arch} end"
} &
done
wait
echo "all build end"

cd frameworks/ios
for arch in ${architectures[@]}
do
    mv ${arch}/firebase_app.framework ${arch}/firebase.framework
    mv ${arch}/firebase.framework/firebase_app ${arch}/firebase.framework/firebase

    frameworks=$(ls ${arch})
    for framework in ${frameworks}
    do
        rm ${arch}/${framework}/Info.plist
    done
done
echo "arm64 armv7 x86_64 i386 frameworks ready"

mkdir universal
frameworks=$(ls arm64)
for framework in ${frameworks}
do
    mkdir universal/${framework}
    lib_basename="$(basename "${framework}" .framework)"
    lib_sub_path="${framework}/${lib_basename}"
    lipo -create "arm64/${lib_sub_path}" "armv7/${lib_sub_path}" "x86_64/${lib_sub_path}" "i386/${lib_sub_path}" \
        -output "universal/${lib_sub_path}"
done
cp -R arm64/firebase.framework/Headers universal/firebase.framework
echo "universal frameworks ready"
