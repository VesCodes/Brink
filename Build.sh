#!/bin/bash

mkdir -p Build

if [ ! -f "Build/BkBuild" ]; then
	clang++ -x objective-c++ -std=c++20 -g -Wall -Werror -I "Source" -I "ThirdParty" "Source/Build/Build.cpp" -o "Build/BkBuild"
fi

Build/BkBuild $@
