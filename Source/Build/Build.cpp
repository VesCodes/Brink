#include "Core/Application.h"
#include "Core/Core.h"
#include "Core/Memory.h"
#include "Core/Platform.h"
#include "Core/Span.h"
#include "Core/String.h"

#ifndef BK_BUILD
#include "Core/Application.cpp"
#include "Core/Core.cpp"
#include "Core/Memory.cpp"
#include "Core/PlatformLinux.cpp"
#include "Core/PlatformMacOS.cpp"
#include "Core/PlatformWindows.cpp"
#include "Core/String.cpp"
#endif

using namespace Bk;

enum class BuildConfig : uint8
{
	Debug,
	Release,
};

struct BuildContext
{
	Platform platform;
	BuildConfig config;

	String cacheDir;

	TSpan<String> includes;
	TSpan<String> systemIncludes;
	TSpan<String> definitions;

	TSpan<String> extraCompilerArguments;
	TSpan<String> extraLinkerArguments;
};

bool ReadTextFile(Arena& arena, String filePath, String& content)
{
	bool result = false;

	FileHandle fileHandle = OpenFile(filePath, FileAccess::Read);
	if (fileHandle)
	{
		ArenaMarker marker = PushMarker(arena);

		TSpan<uint8> buffer = Push(arena, GetFileSize(fileHandle));
		if (ReadFile(fileHandle, buffer) == buffer.length)
		{
			content.data = reinterpret_cast<const char*>(buffer.data);
			content.length = buffer.length;
			result = true;
		}
		else
		{
			PopMarker(arena, marker);
		}

		CloseFile(fileHandle);
	}

	return result;
}

bool WriteTextFile(String filePath, String content)
{
	bool result = false;

	FileHandle fileHandle = OpenFile(filePath, FileAccess::Write);
	if (fileHandle)
	{
		result = WriteFile(fileHandle, AsBytes(content.data, content.length)) == content.length;
		CloseFile(fileHandle);
	}

	return result;
}

String GetCompilerPath(const BuildContext& context)
{
	if (context.platform == Platform::Emscripten)
	{
#if BK_PLATFORM_WINDOWS
		return "em++.bat";
#else
		return "em++";
#endif
	}

	return "clang++";
}

String GetResponseFilePath(Arena& arena, const BuildContext& context, String targetFile)
{
	ArenaScope scratch = GetScratchArena(&arena);

	StringBuilder builder(scratch.arena);
	AppendPath(builder, context.cacheDir);
	AppendPath(builder, GetFileName(targetFile));
	Append(builder, ".rsp");

	return ToString(builder, arena);
}

String GetDependencyFilePath(Arena& arena, const BuildContext& context, String targetFile)
{
	ArenaScope scratch = GetScratchArena(&arena);

	StringBuilder builder(scratch.arena);
	AppendPath(builder, context.cacheDir);
	AppendPath(builder, GetFileName(targetFile));
	Append(builder, ".d");

	return ToString(builder, arena);
}

bool ShouldCompile(const BuildContext& context, String outputFile)
{
	ArenaScope scratch = GetScratchArena();

	if (!FileExists(outputFile))
	{
		return true;
	}

	String dependencyFile = GetDependencyFilePath(scratch.arena, context, outputFile);
	if (!FileExists(dependencyFile))
	{
		return true;
	}

	String dependencyTokenStream;
	if (!ReadTextFile(scratch.arena, dependencyFile, dependencyTokenStream))
	{
		printf("Failed to read dependency file '%.*s'\n", int32(dependencyFile.length), dependencyFile.data);
		return true;
	}

	String targetFile;
	if (!ParseToken(dependencyTokenStream, targetFile))
	{
		printf("Failed to parse dependency file '%.*s'\n", int32(dependencyFile.length), dependencyFile.data);
		return true;
	}

	targetFile = Slice(targetFile, 0, targetFile.length - 1);
	if (targetFile != outputFile)
	{
		printf("[%.*s] Mismatched dependency target '%.*s'\n", int32(outputFile.length), outputFile.data, int32(targetFile.length), targetFile.data);
		return true;
	}

	FileProperties outputFileProps = GetFileProperties(outputFile);
	uint64 outputFileTime = GetPackedTimeFromDateTime(outputFileProps.createdTime);

	bool result = false;

	for (String token; ParseToken(dependencyTokenStream, token);)
	{
		if (token == "\\")
		{
			continue;
		}

		if (!FileExists(token))
		{
			printf("[%.*s] Missing dependency '%.*s'\n", int32(outputFile.length), outputFile.data, int32(token.length), token.data);
			result = true;
		}

		FileProperties dependencyFileProps = GetFileProperties(token);
		uint64 dependencyFileTime = GetPackedTimeFromDateTime(dependencyFileProps.modifiedTime);

		if (dependencyFileTime > outputFileTime)
		{
			printf("[%.*s] Modified dependency '%.*s'\n", int32(outputFile.length), outputFile.data, int32(token.length), token.data);
			result = true;
		}
	}

	return result;
}

ProcessHandle CompileFile(const BuildContext& context, String inputFile, String outputFile)
{
	ArenaScope scratch = GetScratchArena();

	StringBuilder arguments(scratch.arena);

	AppendLine(arguments, "-c");

	if (context.platform == Platform::MacOS)
	{
		AppendLine(arguments, "-x objective-c++");
	}
	else
	{
		AppendLine(arguments, "-x c++");
	}

	AppendLine(arguments, "-std=c++20");

	switch (context.config)
	{
		case BuildConfig::Debug:
		{
			AppendLine(arguments, "-O0");
			break;
		}

		case BuildConfig::Release:
		{
			AppendLine(arguments, "-O3");
			break;
		}
	}

	// #TODO: Add option to skip generating debug info
	// #TODO: Look into -gcodeview for Windows
	AppendLine(arguments, "-g");

	AppendLine(arguments, "-fdiagnostics-absolute-paths");
	AppendLine(arguments, "-fvisibility=hidden");
	AppendLine(arguments, "-fno-exceptions");
	// AppendLine(arguments, "-ftime-trace");

	AppendLine(arguments, "-Wall");
	AppendLine(arguments, "-Werror");
	AppendLine(arguments, "-Wno-unknown-warning-option");

	AppendLine(arguments, "-MMD");
	String dependencyFile = GetDependencyFilePath(scratch.arena, context, outputFile);
	AppendLinef(arguments, "-MF \"%.*s\"", dependencyFile.length, dependencyFile.data);

	for (String includePath : context.includes)
	{
		AppendLinef(arguments, "-I \"%.*s\"", includePath.length, includePath.data);
	}

	for (String includePath : context.systemIncludes)
	{
		AppendLinef(arguments, "-isystem \"%.*s\"", includePath.length, includePath.data);
	}

	for (String definition : context.definitions)
	{
		AppendLinef(arguments, "-D \"%.*s\"", definition.length, definition.data);
	}

	for (String extraArgument : context.extraCompilerArguments)
	{
		AppendLine(arguments, extraArgument);
	}

	AppendLinef(arguments, "\"%.*s\"", inputFile.length, inputFile.data);
	AppendLinef(arguments, "-o \"%.*s\"", outputFile.length, outputFile.data);

	String responseFile = GetResponseFilePath(scratch.arena, context, outputFile);
	if (!WriteTextFile(responseFile, ToString(arguments, scratch.arena)))
	{
		printf("Failed to write response file '%.*s'\n", int32(responseFile.length), responseFile.data);
		return 0;
	}

	Reset(arguments);
	Append(arguments, '@');
	Append(arguments, responseFile);

	return CreateProcess({
		.executable = GetCompilerPath(context),
		.arguments = ToString(arguments, scratch.arena),
	});
}

ProcessHandle LinkFiles(const BuildContext& context, TSpan<String> inputFiles, String outputFile)
{
	ArenaScope scratch = GetScratchArena();

	StringBuilder arguments(scratch.arena);

	// #TODO: Add option to skip generating debug info
	AppendLine(arguments, "-g");

	// #TODO: Look into LLD linker
	// AppendLine(arguments, "-fuse-ld=lld");

	AppendLine(arguments, "-fdiagnostics-absolute-paths");

	if (context.platform == Platform::Windows)
	{
		AppendLine(arguments, "-Wl,-incremental:no");
	}

	for (String extraArgument : context.extraLinkerArguments)
	{
		AppendLine(arguments, extraArgument);
	}

	for (String inputFile : inputFiles)
	{
		AppendLinef(arguments, "\"%.*s\"", inputFile.length, inputFile.data);
	}

	AppendLinef(arguments, "-o \"%.*s\"", outputFile.length, outputFile.data);

	String responseFile = GetResponseFilePath(scratch.arena, context, outputFile);
	if (!WriteTextFile(responseFile, ToString(arguments, scratch.arena)))
	{
		printf("Failed to write response file '%.*s'\n", int32(responseFile.length), responseFile.data);
		return 0;
	}

	Reset(arguments);
	Append(arguments, '@');
	Append(arguments, responseFile);

	return CreateProcess({
		.executable = GetCompilerPath(context),
		.arguments = ToString(arguments, scratch.arena),
	});
}

enum class ActionResult : uint8
{
	Failed,
	Succeeded,
	Skipped,
};

ActionResult CompileModule(const BuildContext& context, String moduleName)
{
	ArenaScope scratch = GetScratchArena();

	StringBuilder builder(scratch.arena);
	AppendPath(builder, "Source");
	AppendPath(builder, moduleName);

	String moduleSourceDir = ToString(builder, scratch.arena);
	size_t moduleSourceDirPrefixLength = moduleSourceDir.length - moduleName.length;

	Reset(builder);
	AppendPath(builder, context.cacheDir);
	AppendPath(builder, moduleName);
	Append(builder, ".module.o");

	String moduleObjectFile = ToString(builder, scratch.arena);

	Reset(builder, builder.length - 1);
	Append(builder, "cpp");

	String moduleUnityFile = ToString(builder, scratch.arena);

	if (!ShouldCompile(context, moduleObjectFile))
	{
		return ActionResult::Skipped;
	}

	Reset(builder);
	AppendLine(builder, "// Automatically generated module unity file");

	FileIteratorHandle fileIt = CreateFileIterator(scratch.arena, moduleSourceDir);
	for (FileIteratorEntry file; AdvanceFileIterator(fileIt, file);)
	{
		if (EndsWith(file.path, ".cpp", true))
		{
			String sourceFile = Slice(file.path, moduleSourceDirPrefixLength);
			AppendLinef(builder, "#include \"%.*s\"", sourceFile.length, sourceFile.data);
		}
	}

	DestroyFileIterator(fileIt);

	if (!WriteTextFile(moduleUnityFile, ToString(builder, scratch.arena)))
	{
		printf("Failed to write module unity file '%.*s'\n", int32(moduleUnityFile.length), moduleUnityFile.data);
		return ActionResult::Failed;
	}

	ProcessHandle process = CompileFile(context, moduleUnityFile, moduleObjectFile);

	int32 processExitCode = -1;
	if (!process || !WaitForProcess(process, &processExitCode))
	{
		return ActionResult::Failed;
	}

	return processExitCode == 0 ? ActionResult::Succeeded : ActionResult::Failed;
}

ActionResult CompileModules(const BuildContext& context, TSpan<String> moduleNames)
{
	ActionResult result = ActionResult::Skipped;

	for (String moduleName : moduleNames)
	{
		ActionResult moduleResult = CompileModule(context, moduleName);

		if (moduleResult == ActionResult::Failed)
		{
			result = ActionResult::Failed;
			break;
		}

		if (moduleResult == ActionResult::Succeeded)
		{
			result = ActionResult::Succeeded;
		}
	}

	return result;
}

bool LinkModules(const BuildContext& context, TSpan<String> moduleNames, String executableName)
{
	ArenaScope scratch = GetScratchArena();

	StringBuilder builder(scratch.arena);
	AppendPath(builder, context.cacheDir);

	size_t cacheDirLength = builder.length;

	TSpan<String> moduleObjectFiles = Push<String>(scratch.arena, moduleNames.length);
	for (size_t moduleIdx = 0; moduleIdx < moduleNames.length; ++moduleIdx)
	{
		AppendPath(builder, moduleNames[moduleIdx]);
		Append(builder, ".module.o");

		moduleObjectFiles[moduleIdx] = ToString(builder, scratch.arena);
		Reset(builder, cacheDirLength);
	}

	ProcessHandle process = LinkFiles(context, moduleObjectFiles, executableName);

	int32 processExitCode = -1;
	if (!process || !WaitForProcess(process, &processExitCode))
	{
		return false;
	}

	return processExitCode == 0;
}

bool GenerateCompileCommands(const BuildContext& context, TSpan<String> moduleNames)
{
	ArenaScope scratch = GetScratchArena();

	String directory = GetCurrentDirectory(scratch.arena);

	StringBuilder builder(scratch.arena);
	StringBuilder pathBuilder(scratch.arena);

	AppendLine(builder, "[");
	for (String moduleName : moduleNames)
	{
		Reset(pathBuilder);
		AppendPath(pathBuilder, "Source");
		AppendPath(pathBuilder, moduleName);

		String moduleSourceDir = ToString(pathBuilder, scratch.arena);

		Reset(pathBuilder);
		AppendPath(pathBuilder, context.cacheDir);
		AppendPath(pathBuilder, moduleName);
		Append(pathBuilder, ".module.o");

		String moduleObjectFile = ToString(pathBuilder, scratch.arena);
		String responseFile = GetResponseFilePath(scratch.arena, context, moduleObjectFile);

		FileIteratorHandle fileIt = CreateFileIterator(scratch.arena, moduleSourceDir);
		for (FileIteratorEntry file; AdvanceFileIterator(fileIt, file);)
		{
			if (!EndsWith(file.path, ".cpp", true))
			{
				continue;
			}

			AppendLine(builder, "\t{");
			AppendLinef(builder, "\t\t\"directory\": \"%.*s\",", directory.length, directory.data);
			AppendLinef(builder, "\t\t\"file\": \"%.*s\",", file.path.length, file.path.data);
			AppendLinef(builder, "\t\t\"arguments\": [ \"clang++\", \"@%.*s\" ]", responseFile.length, responseFile.data);
			AppendLine(builder, "\t},");
		}

		DestroyFileIterator(fileIt);
	}

	if (builder.length > 1)
	{
		// Strip trailing comma
		Reset(builder, builder.length - 2);
		Append(builder, '\n');
	}

	AppendLine(builder, "]");

	return WriteTextFile("compile_commands.json", ToString(builder, scratch.arena));
}

String GenerateGuid(Arena& arena)
{
	// #TODO: Super dirty, revisit
	uint8 bytes[16];

	uint64 mix = GetCpuTicks();
	for (size_t idx = 0; idx < 16; ++idx)
	{
		bytes[idx] ^= (mix >> (idx % sizeof(mix) * 8)) & 0xFF;
		mix ^= bytes[idx];
	}

	ArenaScope scratch = GetScratchArena(&arena);

	StringBuilder builder(scratch.arena);
	Appendf(builder, "%02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X%02X%02X%02X%02X",
	        bytes[0], bytes[1], bytes[2], bytes[3], bytes[4], bytes[5], bytes[6], bytes[7],
	        bytes[8], bytes[9], bytes[10], bytes[11], bytes[12], bytes[13], bytes[14], bytes[15]);

	return ToString(builder, arena);
}

void GenerateProjectFiles(StringBuilder& builder, String path)
{
	ArenaScope scratch = GetScratchArena(builder.arena);

	StringBuilder fileBuilder(scratch.arena);

	FileIteratorHandle fileIt = CreateFileIterator(scratch.arena, path);
	for (FileIteratorEntry file; AdvanceFileIterator(fileIt, file);)
	{
		if (EnumHasAnyFlags(file.properties.attributes, FileAttributes::Directory))
		{
			GenerateProjectFiles(builder, file.path);
			continue;
		}

		String fileExt = GetExtension(file.path);

		if (Equals(fileExt, "cpp", true))
		{
			AppendLinef(fileBuilder, "\t<ClCompile Include=\"%.*s\"/>", file.path.length, file.path.data);
		}
		else if (Equals(fileExt, "h", true))
		{
			AppendLinef(fileBuilder, "\t<ClInclude Include=\"%.*s\"/>", file.path.length, file.path.data);
		}
		else
		{
			AppendLinef(fileBuilder, "\t<None Include=\"%.*s\"/>", file.path.length, file.path.data);
		}
	}

	DestroyFileIterator(fileIt);

	if (fileBuilder.length != 0)
	{
		AppendLine(builder, "<ItemGroup>");
		Append(builder, ToString(fileBuilder, scratch.arena));
		AppendLine(builder, "</ItemGroup>");
	}
}

void GenerateProjectFilters(StringBuilder& builder, String path)
{
	ArenaScope scratch = GetScratchArena(builder.arena);

	StringBuilder fileBuilder(scratch.arena);

	Append(fileBuilder, path);
	Replace(fileBuilder, '/', '\\');

	String filterPath = ToString(fileBuilder, scratch.arena);
	Reset(fileBuilder);

	FileIteratorHandle fileIt = CreateFileIterator(scratch.arena, path);
	for (FileIteratorEntry file; AdvanceFileIterator(fileIt, file);)
	{
		if (EnumHasAnyFlags(file.properties.attributes, FileAttributes::Directory))
		{
			GenerateProjectFilters(builder, file.path);
			continue;
		}

		String fileExt = GetExtension(file.path);

		if (Equals(fileExt, "cpp", true))
		{
			AppendLinef(fileBuilder, "\t<ClCompile Include=\"%.*s\">", file.path.length, file.path.data);
			AppendLinef(fileBuilder, "\t\t<Filter>%.*s</Filter>", filterPath.length, filterPath.data);
			AppendLinef(fileBuilder, "\t</ClCompile>", file.path.length, file.path.data);
		}
		else if (Equals(fileExt, "h", true))
		{
			AppendLinef(fileBuilder, "\t<ClInclude Include=\"%.*s\">", file.path.length, file.path.data);
			AppendLinef(fileBuilder, "\t\t<Filter>%.*s</Filter>", filterPath.length, filterPath.data);
			AppendLinef(fileBuilder, "\t</ClInclude>", file.path.length, file.path.data);
		}
		else
		{
			AppendLinef(fileBuilder, "\t<None Include=\"%.*s\">", file.path.length, file.path.data);
			AppendLinef(fileBuilder, "\t\t<Filter>%.*s</Filter>", filterPath.length, filterPath.data);
			AppendLinef(fileBuilder, "\t</None>", file.path.length, file.path.data);
		}
	}

	DestroyFileIterator(fileIt);

	AppendLine(builder, "<ItemGroup>");

	String filterGuid = GenerateGuid(scratch.arena);

	AppendLinef(builder, "\t<Filter Include=\"%.*s\">", filterPath.length, filterPath.data);
	AppendLinef(builder, "\t\t<UniqueIdentifier>{%.*s}</UniqueIdentifier>", filterGuid.length, filterGuid.data);
	AppendLine(builder, "\t</Filter>");

	if (fileBuilder.length != 0)
	{
		Append(builder, ToString(fileBuilder, scratch.arena));
	}

	AppendLine(builder, "</ItemGroup>");
}

void GenerateProject()
{
	ArenaScope scratch = GetScratchArena();

	StringBuilder builder(scratch.arena);

	String platforms[] = { "x64" };
	String configs[] = { "Debug", "Release" };

	// https://learn.microsoft.com/en-us/cpp/build/reference/vcxproj-file-structure?view=msvc-170

	AppendLine(builder, "<?xml version=\"1.0\" encoding=\"utf-8\"?>");
	AppendLine(builder, "<Project DefaultTargets=\"Build\" xmlns=\"http://schemas.microsoft.com/developer/msbuild/2003\">");

	{
		AppendLine(builder, "<ItemGroup Label=\"ProjectConfigurations\">");

		for (String config : configs)
		{
			for (String platform : platforms)
			{
				AppendLinef(builder, "\t<ProjectConfiguration Include=\"%.*s|%.*s\">", config.length, config.data, platform.length, platform.data);
				AppendLinef(builder, "\t\t<Configuration>%.*s</Configuration>", config.length, config.data);
				AppendLinef(builder, "\t\t<Platform>%.*s</Platform>", platform.length, platform.data);
				AppendLine(builder, "\t</ProjectConfiguration>");
			}
		}

		AppendLine(builder, "</ItemGroup>");
	}

	{
		String projectGuid = GenerateGuid(scratch.arena);

		AppendLine(builder, "<PropertyGroup Label=\"Globals\">");
		AppendLinef(builder, "\t<ProjectGuid>{%.*s}</ProjectGuid>", projectGuid.length, projectGuid.data);
		AppendLine(builder, "\t<VCProjectVersion>17.0</VCProjectVersion>");
		AppendLine(builder, "\t<Keyword>MakeFileProj</Keyword>");
		AppendLine(builder, "</PropertyGroup>");
	}

	AppendLine(builder, "<Import Project=\"$(VCTargetsPath)\\Microsoft.Cpp.default.props\" Condition=\"Exists('$(VCTargetsPath)\\Microsoft.Cpp.default.props')\"/>");

	{
		AppendLine(builder, "<PropertyGroup Label=\"Configuration\">");
		AppendLine(builder, "\t<ConfigurationType>Makefile</ConfigurationType>");
		AppendLine(builder, "\t<PlatformToolset>v143</PlatformToolset>");
		AppendLine(builder, "</PropertyGroup>");
	}

	AppendLine(builder, "<Import Project=\"$(VCTargetsPath)\\Microsoft.Cpp.props\" Condition=\"Exists('$(VCTargetsPath)\\Microsoft.Cpp.props')\"/>");

	AppendLine(builder, "<ImportGroup Label=\"ExtensionSettings\"/>");
	AppendLine(builder, "<ImportGroup Label=\"PropertySheets\"/>");
	AppendLine(builder, "<PropertyGroup Label=\"UserMacros\"/>");

	{
		AppendLine(builder, "<PropertyGroup>");
		AppendLine(builder, "\t<BuildCommand Condition=\"'$(OS)' == 'Windows_NT'\">Build.bat</BuildCommand>");
		AppendLine(builder, "\t<BuildCommand Condition=\"'$(OS)' != 'Windows_NT'\">bash Build.sh</BuildCommand>");
		AppendLine(builder, "\t<NMakePreprocessorDefinitions>$(NMakePreprocessorDefinitions);BK_BUILD</NMakePreprocessorDefinitions>");
		AppendLine(builder, "\t<IncludePath>$(IncludePath);Source;ThirdParty</IncludePath>");
		AppendLine(builder, "\t<AdditionalOptions>/std:c++20</AdditionalOptions>");
		AppendLine(builder, "\t<OutDir>Build\\Cache</OutDir>");
		AppendLine(builder, "\t<IntDir>Build\\Cache</IntDir>");
		AppendLine(builder, "</PropertyGroup>");

		for (String config : configs)
		{
			for (String platform : platforms)
			{
				AppendLinef(builder, "<PropertyGroup Condition=\"'$(Configuration)|$(Platform)' == '%.*s|%.*s'\">",
				            config.length, config.data, platform.length, platform.data);

				AppendLine(builder, "\t<NMakeBuildCommandLine>$(BuildCommand)</NMakeBuildCommandLine>");
				AppendLine(builder, "\t<NMakeReBuildCommandLine>$(BuildCommand)</NMakeReBuildCommandLine>");
				AppendLine(builder, "\t<NMakeCleanCommandLine>$(BuildCommand)</NMakeCleanCommandLine>");
				AppendLine(builder, "\t<NMakeOutput>Build\\BkBuild.exe</NMakeOutput>");

				AppendLinef(builder, "</PropertyGroup>");
			}
		}
	}

	GenerateProjectFiles(builder, "Source");

	AppendLine(builder, "<Import Project=\"$(VCTargetsPath)\\Microsoft.Cpp.targets\" Condition=\"Exists('$(VCTargetsPath)\\Microsoft.Cpp.targets')\"/>");
	AppendLine(builder, "<ImportGroup Label=\"ExtensionTargets\"/>");

	AppendLine(builder, "</Project>");

	WriteTextFile("Brink.vcxproj", ToString(builder, scratch.arena));

	Reset(builder);

	AppendLine(builder, "<?xml version=\"1.0\" encoding=\"utf-8\"?>");
	AppendLine(builder, "<Project xmlns=\"http://schemas.microsoft.com/developer/msbuild/2003\">");

	GenerateProjectFilters(builder, "Source");

	AppendLine(builder, "</Project>");

	WriteTextFile("Brink.vcxproj.filters", ToString(builder, scratch.arena));
}

void PrepareBuildContext(Arena& arena, BuildContext& context)
{
	StringBuilder builder(arena);
	AppendPath(builder, "Build/Cache");
	AppendPath(builder, GetPlatformName(context.platform));
	AppendPath(builder, context.config == BuildConfig::Debug ? "Debug" : "Release");

	context.cacheDir = ToString(builder, arena);

	CreateDirectory(context.cacheDir);
}

void SelfUpdate(int32 argc, char** argv)
{
	ArenaScope scratch = GetScratchArena();

	BuildContext context = {
		.platform = GetPlatform(),
		.config = BuildConfig::Debug,
		.includes = Copy<String>(scratch.arena, { "Source", "ThirdParty" }),
		.definitions = Copy<String>(scratch.arena, { "BK_BUILD" }),
	};

	PrepareBuildContext(scratch.arena, context);

	ActionResult result = CompileModules(context, { "Core", "Build" });

	if (result == ActionResult::Skipped)
	{
		return;
	}

	if (result == ActionResult::Failed)
	{
		printf("Failed to compile during self-update\n");
		ExitApp(1);
	}

	StringBuilder builder(scratch.arena);
	AppendPath(builder, argv[0]);
	NormalizePath(builder);
	Append(builder, ".bak");

	String backupFile = ToString(builder, scratch.arena);
	String targetFile = Slice(backupFile, 0, backupFile.length - 4);

	DeleteFile(backupFile);
	MoveFile(targetFile, backupFile);

	if (!LinkModules(context, { "Core", "Build" }, targetFile))
	{
		MoveFile(backupFile, targetFile);

		printf("Failed to link during self-update\n");
		ExitApp(1);
	}

	Reset(builder);
	for (int32 i = 1; i < argc; ++i)
	{
		String argValue = argv[i];

		if (Contains(argValue, ' '))
		{
			Append(builder, '\"');
			Append(builder, argValue);
			Append(builder, "\"");
		}
		else
		{
			Append(builder, argValue);
		}

		Append(builder, ' ');
	}

	ProcessHandle process = CreateProcess({
		.executable = argv[0],
		.arguments = ToString(builder, scratch.arena),
	});

	printf("-----\n");

	int32 exitCode = 1;
	WaitForProcess(process, &exitCode);

	ExitApp(exitCode);
}

int32 AppMain(int32 argc, char** argv)
{
	int32 result = 0;

	SelfUpdate(argc, argv);

	Arena arena = {};

	BuildContext context = {
		.platform = GetPlatform(),
		.config = BuildConfig::Debug,
		.includes = Copy<String>(arena, { "Source", "ThirdParty" }),
		.definitions = Copy<String>(arena, { "BK_BUILD" }),
	};

	String singleFile = String::Empty;
	bool generateProject = false;

	for (int32 i = 1; i < argc; ++i)
	{
		String argName = argv[i];
		String argValue = String::Empty;

		size_t equalsIdx = Find(argName, '=');
		if (equalsIdx != SIZE_MAX)
		{
			argValue = Slice(argName, equalsIdx + 1);
			argName = Slice(argName, 0, equalsIdx);
		}

		if (Equals(argName, "-GenerateProject", true))
		{
			generateProject = true;
		}
		else if (Equals(argName, "-Platform", true))
		{
			if (Equals(argValue, "Windows", true))
			{
				context.platform = Platform::Windows;
			}
			else if (Equals(argValue, "Linux", true))
			{
				context.platform = Platform::Linux;
			}
			else if (Equals(argValue, "MacOS", true))
			{
				context.platform = Platform::MacOS;
			}
			else if (Equals(argValue, "Emscripten", true))
			{
				context.platform = Platform::Emscripten;
			}
		}
		else if (Equals(argName, "-Config", true))
		{
			if (Equals(argValue, "Debug", true))
			{
				context.config = BuildConfig::Debug;
			}
			else if (Equals(argValue, "Release", true))
			{
				context.config = BuildConfig::Release;
			}
		}
		else if (Equals(argName, "-SingleFile", true))
		{
			singleFile = argValue;
		}
	}

	PrepareBuildContext(arena, context);

	if (generateProject)
	{
		// GenerateProject();
		GenerateCompileCommands(context, { "Build", "Core", "Engine", "Sandbox" });
	}

	DateTime buildTime = GetLocalTime();
	double buildStartTime = GetTimeSec();

	printf("Build started at %02d:%02d:%02d\n", buildTime.hour, buildTime.minute, buildTime.second);

	if (singleFile.length != 0)
	{
		StringBuilder builder(arena);
		AppendPath(builder, context.cacheDir);
		AppendPath(builder, GetFileNameWithoutExtension(singleFile));
		Append(builder, ".o");

		String objectFile = ToString(builder, arena);
		CreateDirectory(GetDirectoryName(objectFile));

		ProcessHandle process = CompileFile(context, singleFile, objectFile);

		if (!WaitForProcess(process, &result))
		{
			printf("Failed to compile single file '%.*s'\n", int32(singleFile.length), singleFile.data);
			result = 1;
		}
	}
	else
	{
		CreateDirectory("Build/Sandbox/Assets");
		CopyDirectory("Source/Sandbox/Assets", "Build/Sandbox/Assets");

		String outputFile = String::Empty;
		if (context.platform == Platform::Windows)
		{
			outputFile = "Build/Sandbox/Sandbox.exe";

			context.extraLinkerArguments = {
				"-Wl,/subsystem:windows,/entry:mainCRTStartup",
				"ThirdParty/dawn/webgpu_dawn.lib",
				"-ldxguid",
			};
		}
		else if (context.platform == Platform::MacOS)
		{
			outputFile = "Build/Sandbox/Sandbox";

			context.extraLinkerArguments = {
				"ThirdParty/dawn/libwebgpu_dawn.a",
				"-framework IOKit",
				"-framework IOSurface",
			};
		}
		else if (context.platform == Platform::Emscripten)
		{
			outputFile = "Build/Sandbox/index.html";

			context.extraCompilerArguments = {
				"--use-port=emdawnwebgpu",
			};

			context.extraLinkerArguments = {
				"-sALLOW_MEMORY_GROWTH=1",
				"--use-port=emdawnwebgpu",
				"--shell-file=Source/Sandbox/Sandbox.html",
				"--preload-file=Build/Sandbox/Assets@Assets",
			};
		}

		String modules[] = { "Core", "Engine", "Sandbox" };

		ActionResult compileResult = CompileModules(context, modules);

		if (compileResult == ActionResult::Succeeded || compileResult == ActionResult::Skipped)
		{
			double compileTime = GetTimeSec();
			printf("Compiled modules for Sandbox in %0.4fs\n", compileTime - buildStartTime);

			if (LinkModules(context, modules, outputFile))
			{
				double linkTime = GetTimeSec();
				printf("Linked modules for Sandbox in %0.4fs\n", linkTime - compileTime);
			}
			else
			{
				printf("Failed to link Sandbox modules\n");
				result = 1;
			}
		}
		else if (compileResult == ActionResult::Failed)
		{
			printf("Failed to compile Sandbox modules\n");
			result = 1;
		}
	}

	printf("Build completed in %0.4fs\n", GetTimeSec() - buildStartTime);

	return result;
}
