@echo off

if not exist Build mkdir Build

emcc -std=c++20 -g -gsource-map -Wall -Wextra -Werror -ferror-limit=0 ^
    -Wsign-conversion -Wno-unused-parameter -Wno-missing-designated-field-initializers ^
    Source/BrinkSandbox.cpp --shell-file Source/Brink.html -o Build/index.html ^
    --use-port=ThirdParty/emdawnwebgpu_pkg/emdawnwebgpu.port.py:cpp_bindings=false %*
