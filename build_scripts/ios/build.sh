#!/bin/bash -e
#
# Script to build iOS Frameworks
# If built for all architectures (arm64 armv7 x86_64 i386),
# it will build universal framework as well
#

usage(){
    echo "Usage: $0 [options]
 options:
   -b, build path              default: ios_build
   -s, source path             default: .
   -a, framework architecture  default: SUPPORTED_ARCHITECTURES
   -t, CMake target            default: SUPPORTED_TARGETS
   -g, generate Makefiles      default: true
   -c, CMake build             default: true
 example: 
   build_scripts/ios/build.sh -b ios_build -s . -a arm64,x86_64 -t firebase_admob,firebase_auth -c false"
}

set -e

readonly SUPPORTED_ARCHITECTURES=(arm64 armv7 x86_64 i386)
readonly SUPPORTED_TARGETS=(firebase_admob firebase_analytics firebase_auth firebase_database firebase_dynamic_links firebase_firestore firebase_functions firebase_instance_id firebase_messaging firebase_remote_config firebase_storage)

# build default value
buildpath="ios_build"
sourcepath="."
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
echo "build architectures: ${architectures[@]}"
echo "build targets: ${targets[@]}"
echo "generate Makefiles: ${generateMakefiles}"
echo "CMake Build: ${cmakeBuild}"
sourcepath=$(cd ${sourcepath} && pwd)   #full path
buildpath=$(mkdir -p ${buildpath} && cd ${buildpath} && pwd)    #full path

# generate Makefiles for each architecture and target
frameworkspath="frameworks/ios"
if ${generateMakefiles}; then
    for arch in ${architectures[@]}; do 
        echo "generate Makefiles start"
        mkdir -p ${buildpath}/ios_build_file/${arch} && cd ${buildpath}/ios_build_file/${arch}
        if [[ "${arch}" == "arm64" || "${arch}" == "armv7" ]]; then
            toolchain="cmake/toolchains/ios.cmake"
        elif [[ "${arch}" == "x86_64" || "${arch}" == "i386" ]]; then
            toolchain="cmake/toolchains/ios_simulator.cmake"
        fi
        cmake -DCMAKE_TOOLCHAIN_FILE=${sourcepath}/${toolchain} \
            -DCMAKE_OSX_ARCHITECTURES=${arch} \
            -DCMAKE_ARCHIVE_OUTPUT_DIRECTORY=${buildpath}/${frameworkspath}/${arch} \
            ${sourcepath}
        echo "generate Makefiles end"
    done
fi

# build framework for each architecture and target
IFS=$'\n' # split $(ls) on \n characters
if ${cmakeBuild}; then
    for arch in ${architectures[@]}; do 
    {
        cd ${buildpath}/ios_build_file/${arch}
        echo "build ${arch} ${targets[@]} framework start"
        cmake --build . --target ${targets[@]}
        echo "build ${arch} ${targets[@]} framework end"
    
    } &
    done
    subprocess_fail=0
    for job in $(jobs -p); do
        wait $job || let "subprocess_fail+=1"
    done
    if [ "${subprocess_fail}" == "0" ]; then
        echo "${architectures[@]} frameworks build end"
    else
        echo "frameworks build error, ${subprocess_fail} architecture(s) build failed"
        exit 2
    fi

    # arrange the framework 
    cd ${buildpath}/${frameworkspath}
    for arch in ${architectures[@]}; do
        # rename firebase_app to firebase
        if [[ ! -d "${arch}/firebase.framework" ]]; then
            mv ${arch}/firebase_app.framework ${arch}/firebase.framework
            mv ${arch}/firebase.framework/firebase_app ${arch}/firebase.framework/firebase
            rm ${arch}/firebase.framework/Info.plist
        fi

        # delete useless Info.plist
        for target in ${targets[@]}; do
            if [[ -f "${arch}/${target}.framework/Info.plist" ]]; then
                rm ${arch}/${target}.framework/Info.plist
            fi
        done

        # delete non-framework dir
        for dir in $(ls ${arch}); do
            if [[ ! ${dir} =~ ".framework" ]]; then
                rm -rf ${arch}/${dir}
            fi
        done
    done
    echo "${architectures[@]} frameworks ready to use"

    # if we built for all architectures (arm64 armv7 x86_64 i386)
    # build universal framework as well
    if [[ ${#architectures[@]} < ${#SUPPORTED_ARCHITECTURES[@]} ]]; then
        exit 0
    fi

    targets+=('firebase')
    for target in ${targets[@]}; do
        mkdir -p universal/${target}.framework
        libsubpath="${target}.framework/${target}"
        if [[ -f "${SUPPORTED_ARCHITECTURES[0]}/${libsubpath}" ]]; then
            lipo -create "${SUPPORTED_ARCHITECTURES[0]}/${libsubpath}" \
                        "${SUPPORTED_ARCHITECTURES[1]}/${libsubpath}" \
                        "${SUPPORTED_ARCHITECTURES[2]}/${libsubpath}" \
                        "${SUPPORTED_ARCHITECTURES[3]}/${libsubpath}" \
                -output "universal/${libsubpath}"
        fi
    done
    if [[ ! -d "universal/firebase.framework/Headers" ]]; then
        cp -R ${SUPPORTED_ARCHITECTURES[0]}/firebase.framework/Headers universal/firebase.framework
    fi
    echo "universal frameworks build end & ready to use"
fi
