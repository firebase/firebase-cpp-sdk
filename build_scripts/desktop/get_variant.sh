#!/bin/bash
set -e

# Given a filename on the command-line, figure out what variant it should be packaged into.
# For example, firebase-cpp-sdk-windows-x64-Release-static-build.tgz would be:
#   OS: Windows
#   Compiler: VS2015 (default for Windows)
#   CPU: x64
#   Debug mode: Release
#   CRT linkage: MT ("static")
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
linkage=MD
vs=VS2015
stl=c++

for c in $(echo "${filename}" | tr "_.-" "\n\n\n"); do
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
	    linkage=MD
	;;
	dynamic)
	    linkage=MD
	;;
	mt)
	    linkage=MT
	;;
	static)
	    linkage=MT
	    ;;
	# Debug/Release compilation mode
	release)
	    debugmode=Release
	;;
	Release)
	    debugmode=Release
	;;
	debug)
	    debugmode=Debug
	;;
	Debug)
	    debugmode=Debug
	;;
	# Android STL variant
	c++)
	    stl=c++
        ;;
	gnustl)
	    stl=gnustl
        ;;
	stlport)
	    stl=stlport
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
	echo -n "${arch}"
    ;;
    windows)
	if [[ -z "${arch_win}" ]]; then
	    echo "Invalid architecture for Windows: ${arch}" 1>&2
	    exit 1
	fi
	echo -n "${vs}/${linkage}/${arch_win}/${debugmode}"
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
	    echo -n "${stl}"
	else
	    echo -n "${arch}/${stl}"
	fi
    ;;
    *)
	echo "Invalid OS target ${os}" 1>&2
	exit 1
    ;;
    
esac
