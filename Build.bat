@echo off

if not exist Build mkdir Build

emcc -std=c++20 -g -Wall -Wextra -Werror -Wsign-conversion -Wno-unused-parameter -Wno-missing-designated-field-initializers -ferror-limit=0 ^
    Source/BrinkSandbox.cpp --shell-file Source/Brink.html -o Build/index.html --use-port=ThirdParty/emdawnwebgpu_pkg/emdawnwebgpu.port.py:cpp_bindings=false %*
