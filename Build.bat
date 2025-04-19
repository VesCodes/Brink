@echo off

emcc Source/Brink.cpp Source/BrinkSandbox.cpp --shell-file Source/Brink.html -o Build/index.html -s USE_WEBGPU=1 %*