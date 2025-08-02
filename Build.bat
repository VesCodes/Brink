@echo off

if not exist Build mkdir Build

if not exist "Build/BkBuild.exe" (
	clang++ -std=c++20 -g -Wall -Werror -I "Source" -I "ThirdParty" "Source/Build/Build.cpp" -o "Build/BkBuild.exe"
)

"Build/BkBuild.exe" %*
