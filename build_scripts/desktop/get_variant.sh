#!/bin/bash

# Copyright 2020 Google LLC
set -e

# Given a filename on the command-line, figure out what variant it should be packaged into.
# For example, firebase-cpp-sdk-windows-x64-Release-static-build.tgz would be:
#   OS: Windows
#   Compiler: VS2015 (default for Windows)
#   CPU: x64
#   Debug mode: Release
#   MSVC CRT linkage: MT ("static")
# So the build variant would be: VS2015/MT/x64/Release

path=$1

if [[ -z "${path}" ]]; then
    echo "Please specify package file." 1>&2
    exit 2
fi

filename=$(basename "${path}")

# Ensure the filename starts with firebase-cpp-sdk-
if [[ "${filename}" != "firebase-cpp-sdk-"* ]]; then
    echo "Can only guess variant for firebase-cpp-sdk files. ${filename}" 1>&2
    exit 2
fi

os=
arch=
arch_win=
debugmode=Release
msvc_runtime_library=MD
vs=VS2019
stl=c++
linux_abi=legacy

for c in $(echo "${filename}" | tr "[:upper:]" "[:lower:]" | tr "_.-" "\n\n\n"); do
    case $c in
	# Operating systems
	ios)
	    os=ios
	;;
	android)
	    os=android
	;;
	linux)
	    os=linux
	;;
	windows)
	    os=windows
	;;
	darwin)
	    os=darwin
	;;
	# Desktop and mobile CPU types
	x86)
	    arch=i386
	    arch_win=x86
	;;
	i386)
	    arch=i386
	    arch_win=x86
	;;
	x64)
	    arch=x86_64
	    arch_win=x64
	;;
	x86_64)
	    arch=x86_64
	    arch_win=x64
	;;
	# Additional iOS and Android CPU types
	arm64)
	    arch=arm64
	;;
	armv7)
	    arch=armv7
	;;
	universal)
	    arch=universal
        ;;
	# Additional Android CPU types
	arm64-v8a)
	    arch=arm64-v8a
	;;
	armeabi)
	    arch=armeabi
	;;
	armeabi-v7a)
	    arch=armeabi-v7a
	;;
	armeabi-v7a-hard)
	    arch=armeabi-v7a-hard
	;;
	mips)
	    arch=mips
	;;
	mips64)
	    arch=mips64
	;;
	# Windows OS framework linkages
	md)
	    msvc_runtime_library=MD
	;;
	dynamic)
	    msvc_runtime_library=MD
	;;
	mt)
	    msvc_runtime_library=MT
	;;
	static)
	    msvc_runtime_library=MT
	    ;;
	# Debug/Release compilation mode
	release)
	    debugmode=Release
	;;
	debug)
	    debugmode=Debug
	;;
	# Android STL variant
	c++)
	    stl=c++
        ;;
	cxx11)
	    linux_abi=cxx11
        ;;
	c++11)
	    linux_abi=cxx11
        ;;
	legacy)
	    linux_abi=legacy
        ;;
	vs2019)
	    vs=VS2019
        ;;
	vs2017)
	    vs=VS2017
        ;;
	vs2015)
	    vs=VS2015
        ;;
	vs2013)
	    vs=VS2013
        ;;
    esac
done

if [[ -z "${os}" ]]; then
    echo "Couldn't determine OS" 1>&2
    exit 1
fi

case ${os} in
    darwin)
	if [[ -z "${arch}" ]]; then
	    echo "Couldn't determine architecture" 1>&2
	    exit 1
	fi
	echo -n "${arch}"
    ;;
    linux)
	if [[ -z "${arch}" ]]; then
	    echo "Couldn't determine architecture" 1>&2
	    exit 1
	fi
	echo -n "${arch}/${linux_abi}"
    ;;
    windows)
	if [[ -z "${arch_win}" ]]; then
	    echo "Invalid architecture for Windows: ${arch}" 1>&2
	    exit 1
	fi
	echo -n "${vs}/${msvc_runtime_library}/${arch_win}/${debugmode}"
    ;;
    ios)
	if [[ -z "${arch}" ]]; then
	    echo -n "."
	else
	    echo -n "${arch}"
	fi
    ;;
    android)
	if [[ -z "${arch}" ]]; then
	    echo -n "."
	else
	    echo -n "${arch}"
	fi
    ;;
    *)
	echo "Invalid OS target ${os}" 1>&2
	exit 1
    ;;
    
esac
