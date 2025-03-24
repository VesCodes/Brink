#!/bin/bash

emcc Source/Brink.cpp --shell-file Source/Brink.html -o Build/index.html -s USE_WEBGPU=1 $@
