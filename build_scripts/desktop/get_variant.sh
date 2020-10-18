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

for c in $(echo "${filename}" | tr "_.-" "\n\n\n"); do
    case $c in
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
	arm64)
	    arch=arm64
	    arch_win=arm64
	;;
	armv7)
	    arch=armv7
	    arch_win=arm32
	;;
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
    esac
done

if [[ -z "${os}" ]]; then
    echo "Couldn't determine OS" 1>&2
    exit 1
fi
if [[ -z "${arch}" ]]; then
    echo "Couldn't determine architecture" 1>&2
    exit 1
fi

case ${os} in
    darwin)
	echo -n "${arch}"
    ;;
    linux)
	echo -n "${arch}"
    ;;
    windows)
	echo -n "${vs}/${linkage}/${arch_win}/${debugmode}"
    ;;
    *)
	echo "Invalid OS target ${os}" 1>&2
	exit 1
    ;;
    
esac
