#!/bin/bash

mkdir -p Build

emcc -std=c++20 -g -Wall -Wextra -Werror -Wsign-conversion -Wno-unused-parameter -Wno-missing-designated-field-initializers -ferror-limit=0 \
    Source/Brink.cpp Source/BrinkSandbox.cpp --shell-file Source/Brink.html -o Build/index.html -sUSE_WEBGPU=1 $@
