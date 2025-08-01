#!/bin/bash

mkdir -p Build

if [ ! -f "Build/BkBuild" ]; then
	clang++ -std=c++20 -g -Wall -Werror -I "Source" -I "ThirdParty" "Source/Build/Build.cpp" -o "Build/BkBuild"
fi

em++ -std=c++20 -g -gsource-map -Wall -Wextra -Werror -ferror-limit=0 \
	-Wsign-conversion -Wno-unused-parameter -Wno-missing-designated-field-initializers \
	-I"Source" -isystem"ThirdParty" \
	Source/Sandbox/Sandbox.cpp --shell-file Source/Sandbox/Sandbox.html -o Build/index.html \
	--use-port=emdawnwebgpu $@
