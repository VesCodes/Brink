#!/bin/bash

mkdir -p Build

emcc -std=c++20 -g -gsource-map -Wall -Wextra -Werror -ferror-limit=0 \
    -Wsign-conversion -Wno-unused-parameter -Wno-missing-designated-field-initializers \
    Source/BkSandbox.cpp --shell-file Source/BkSandbox.html -o Build/index.html \
    --use-port=emdawnwebgpu $@
