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

bool WriteTextFile(String outputFile, String content)
{
	bool result = false;

	FileHandle fileHandle = OpenFile(outputFile, FileAccess::Write);
	if (fileHandle)
	{
		result = WriteFile(fileHandle, AsBytes(content.data, content.length)) == content.length;
		CloseFile(fileHandle);
	}

	return result;
}

String GetResponseFilePath(Arena& arena, const BuildContext& context, String targetFile)
{
	ArenaScope scratch = GetScratchArena(&arena);

	StringBuilder builder(scratch.arena);
	builder.AppendPath(context.cacheDir);
	builder.AppendPath(GetFileName(targetFile));
	builder.Append(".rsp");

	return builder.ToString(arena);
}

String GetDependencyFilePath(Arena& arena, const BuildContext& context, String targetFile)
{
	ArenaScope scratch = GetScratchArena(&arena);

	StringBuilder builder(scratch.arena);
	builder.AppendPath(context.cacheDir);
	builder.AppendPath(GetFileName(targetFile));
	builder.Append(".d");

	return builder.ToString(arena);
}

bool ShouldCompile(const BuildContext& context, String outputFile)
{
	ArenaScope scratch = GetScratchArena();

	if (!FileExists(outputFile))
	{
		return true;
	}

	FileProperties outputFileProps = GetFileProperties(outputFile);
	uint64 outputFileTime = GetPackedTimeFromDateTime(outputFileProps.createdTime);

	String dependencyFile = GetDependencyFilePath(scratch.arena, context, outputFile);
	if (!FileExists(dependencyFile))
	{
		return true;
	}

	FileHandle fileHandle = OpenFile(dependencyFile, FileAccess::Read);
	if (!fileHandle)
	{
		printf("Failed to open dependency file '%.*s'\n", int32(dependencyFile.length), dependencyFile.data);
		return true;
	}

	TSpan<uint8> fileContent = scratch.arena.Push<uint8>(GetFileSize(fileHandle));
	fileContent.length = ReadFile(fileHandle, fileContent);
	CloseFile(fileHandle);

	String tokenStream((char*)fileContent.data, fileContent.length);

	String targetFile;
	if (!ParseToken(tokenStream, targetFile))
	{
		printf("Failed to parse dependency file '%.*s'\n", int32(dependencyFile.length), dependencyFile.data);
		return true;
	}

	targetFile = targetFile.Slice(0, targetFile.length - 1);
	if (targetFile != outputFile)
	{
		printf("[%.*s] Mismatched dependency target '%.*s'\n", int32(outputFile.length), outputFile.data, int32(targetFile.length), targetFile.data);
		return true;
	}

	bool result = false;

	for (String token; ParseToken(tokenStream, token);)
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

ProcessHandle RunCompiler(const BuildContext& context, String arguments)
{
	ArenaScope scratch = GetScratchArena();

	String executable = "clang++";

	if (context.platform == Platform::Emscripten)
	{
#if BK_PLATFORM_WINDOWS
		executable = "cmd.exe";

		StringBuilder builder(scratch.arena);
		builder.Append("/c em++ ");
		builder.Append(arguments);

		arguments = builder.ToString(scratch.arena);
#else
		executable = "em++";
#endif
	}

	return CreateProcess({
		.executable = executable,
		.arguments = arguments,
	});
}

ProcessHandle CompileFile(const BuildContext& context, String inputFile, String outputFile)
{
	ArenaScope scratch = GetScratchArena();

	StringBuilder arguments(scratch.arena);

	arguments.AppendLine("-c");
	arguments.AppendLine("-x c++");
	arguments.AppendLine("-std=c++20");

	switch (context.config)
	{
		case BuildConfig::Debug:
		{
			arguments.AppendLine("-O0");
			break;
		}

		case BuildConfig::Release:
		{
			arguments.AppendLine("-O3");
			break;
		}
	}

	// #TODO: Add option to skip generating debug info
	// #TODO: Look into -gcodeview for Windows
	arguments.AppendLine("-g");

	arguments.AppendLine("-fdiagnostics-absolute-paths");
	arguments.AppendLine("-fvisibility=hidden");
	arguments.AppendLine("-fno-exceptions");
	// arguments.AppendLine("-ftime-trace");

	arguments.AppendLine("-Wall");
	arguments.AppendLine("-Werror");
	arguments.AppendLine("-Wno-unknown-warning-option");

	arguments.AppendLine("-MMD");
	String dependencyFile = GetDependencyFilePath(scratch.arena, context, outputFile);
	arguments.AppendLinef("-MF \"%.*s\"", dependencyFile.length, dependencyFile.data);

	for (String includePath : context.includes)
	{
		arguments.AppendLinef("-I \"%.*s\"", includePath.length, includePath.data);
	}

	for (String includePath : context.systemIncludes)
	{
		arguments.AppendLinef("-isystem \"%.*s\"", includePath.length, includePath.data);
	}

	for (String definition : context.definitions)
	{
		arguments.AppendLinef("-D \"%.*s\"", definition.length, definition.data);
	}

	for (String extraArgument : context.extraCompilerArguments)
	{
		arguments.AppendLine(extraArgument);
	}

	arguments.AppendLinef("\"%.*s\"", inputFile.length, inputFile.data);
	arguments.AppendLinef("-o \"%.*s\"", outputFile.length, outputFile.data);

	String responseFile = GetResponseFilePath(scratch.arena, context, outputFile);
	if (!WriteTextFile(responseFile, arguments.ToString(scratch.arena)))
	{
		printf("Failed to write response file '%.*s'\n", int32(responseFile.length), responseFile.data);
		return 0;
	}

	arguments.Reset();
	arguments.Append('@');
	arguments.Append(responseFile);

	return RunCompiler(context, arguments.ToString(scratch.arena));
}

ProcessHandle LinkFiles(const BuildContext& context, TSpan<String> inputFiles, String outputFile)
{
	ArenaScope scratch = GetScratchArena();

	StringBuilder arguments(scratch.arena);

	// #TODO: Add option to skip generating debug info
	arguments.AppendLine("-g");

	// #TODO: Look into LLD linker
	// arguments.AppendLine("-fuse-ld=lld");

	arguments.AppendLine("-fdiagnostics-absolute-paths");

	if (context.platform == Platform::Windows)
	{
		arguments.AppendLine("-Wl,-incremental:no");
	}

	for (String extraArgument : context.extraLinkerArguments)
	{
		arguments.AppendLine(extraArgument);
	}

	for (String inputFile : inputFiles)
	{
		arguments.AppendLinef("\"%.*s\"", inputFile.length, inputFile.data);
	}

	arguments.AppendLinef("-o \"%.*s\"", outputFile.length, outputFile.data);

	String responseFile = GetResponseFilePath(scratch.arena, context, outputFile);
	if (!WriteTextFile(responseFile, arguments.ToString(scratch.arena)))
	{
		printf("Failed to write response file '%.*s'\n", int32(responseFile.length), responseFile.data);
		return 0;
	}

	arguments.Reset();
	arguments.Append('@');
	arguments.Append(responseFile);

	return RunCompiler(context, arguments.ToString(scratch.arena));
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
	builder.AppendPath("Source");
	builder.AppendPath(moduleName);

	String moduleSourceDir = builder.ToString(scratch.arena);
	size_t moduleSourceDirPrefixLength = moduleSourceDir.length - moduleName.length;

	builder.Reset();
	builder.AppendPath(context.cacheDir);
	builder.AppendPath(moduleName);
	builder.Append(".module.o");

	String moduleObjectFile = builder.ToString(scratch.arena);

	builder.Reset(builder.length - 1);
	builder.Append("cpp");

	String moduleUnityFile = builder.ToString(scratch.arena);

	if (!ShouldCompile(context, moduleObjectFile))
	{
		return ActionResult::Skipped;
	}

	builder.Reset();
	builder.AppendLine("// Automatically generated module unity file");

	FileIteratorHandle fileIt = CreateFileIterator(scratch.arena, moduleSourceDir);
	for (FileIteratorEntry file; AdvanceFileIterator(fileIt, file);)
	{
		if (file.path.EndsWith(".cpp", true))
		{
			String sourceFile = file.path.Slice(moduleSourceDirPrefixLength);
			builder.AppendLinef("#include \"%.*s\"", sourceFile.length, sourceFile.data);
		}
	}

	DestroyFileIterator(fileIt);

	if (!WriteTextFile(moduleUnityFile, builder.ToString(scratch.arena)))
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
	builder.AppendPath(context.cacheDir);

	size_t cacheDirLength = builder.length;

	TSpan<String> moduleObjectFiles = scratch.arena.Push<String>(moduleNames.length);
	for (size_t moduleIdx = 0; moduleIdx < moduleNames.length; ++moduleIdx)
	{
		builder.AppendPath(moduleNames[moduleIdx]);
		builder.Append(".module.o");

		moduleObjectFiles[moduleIdx] = builder.ToString(scratch.arena);
		builder.Reset(cacheDirLength);
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

	builder.AppendLine("[");
	for (String moduleName : moduleNames)
	{
		pathBuilder.Reset();
		pathBuilder.AppendPath("Source");
		pathBuilder.AppendPath(moduleName);

		String moduleSourceDir = pathBuilder.ToString(scratch.arena);

		pathBuilder.Reset();
		pathBuilder.AppendPath(context.cacheDir);
		pathBuilder.AppendPath(moduleName);
		pathBuilder.Append(".module.o");

		String moduleObjectFile = pathBuilder.ToString(scratch.arena);
		String responseFile = GetResponseFilePath(scratch.arena, context, moduleObjectFile);

		FileIteratorHandle fileIt = CreateFileIterator(scratch.arena, moduleSourceDir);
		for (FileIteratorEntry file; AdvanceFileIterator(fileIt, file);)
		{
			if (!file.path.EndsWith(".cpp", true))
			{
				continue;
			}

			builder.AppendLine("\t{");
			builder.AppendLinef("\t\t\"directory\": \"%.*s\",", directory.length, directory.data);
			builder.AppendLinef("\t\t\"file\": \"%.*s\",", file.path.length, file.path.data);
			builder.AppendLinef("\t\t\"arguments\": [ \"clang++\", \"@%.*s\" ]", responseFile.length, responseFile.data);
			builder.AppendLine("\t},");
		}

		DestroyFileIterator(fileIt);
	}

	if (builder.length > 1)
	{
		// Strip trailing comma
		builder.Reset(builder.length - 2);
		builder.Append('\n');
	}

	builder.AppendLine("]");

	return WriteTextFile("compile_commands.json", builder.ToString(scratch.arena));
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
	builder.Appendf("%02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X%02X%02X%02X%02X",
	                bytes[0], bytes[1], bytes[2], bytes[3], bytes[4], bytes[5], bytes[6], bytes[7],
	                bytes[8], bytes[9], bytes[10], bytes[11], bytes[12], bytes[13], bytes[14], bytes[15]);

	return builder.ToString(arena);
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

		if (fileExt.Equals("cpp", true))
		{
			fileBuilder.AppendLinef("\t<ClCompile Include=\"%.*s\"/>", file.path.length, file.path.data);
		}
		else if (fileExt.Equals("h", true))
		{
			fileBuilder.AppendLinef("\t<ClInclude Include=\"%.*s\"/>", file.path.length, file.path.data);
		}
		else
		{
			fileBuilder.AppendLinef("\t<None Include=\"%.*s\"/>", file.path.length, file.path.data);
		}
	}

	DestroyFileIterator(fileIt);

	if (fileBuilder.length != 0)
	{
		builder.AppendLine("<ItemGroup>");
		builder.Append(fileBuilder.ToString(scratch.arena));
		builder.AppendLine("</ItemGroup>");
	}
}

void GenerateProjectFilters(StringBuilder& builder, String path)
{
	ArenaScope scratch = GetScratchArena(builder.arena);

	StringBuilder fileBuilder(scratch.arena);

	fileBuilder.Append(path);
	fileBuilder.Replace('/', '\\');

	String filterPath = fileBuilder.ToString(scratch.arena);
	fileBuilder.Reset();

	FileIteratorHandle fileIt = CreateFileIterator(scratch.arena, path);
	for (FileIteratorEntry file; AdvanceFileIterator(fileIt, file);)
	{
		if (EnumHasAnyFlags(file.properties.attributes, FileAttributes::Directory))
		{
			GenerateProjectFilters(builder, file.path);
			continue;
		}

		String fileExt = GetExtension(file.path);

		if (fileExt.Equals("cpp", true))
		{
			fileBuilder.AppendLinef("\t<ClCompile Include=\"%.*s\">", file.path.length, file.path.data);
			fileBuilder.AppendLinef("\t\t<Filter>%.*s</Filter>", filterPath.length, filterPath.data);
			fileBuilder.AppendLinef("\t</ClCompile>", file.path.length, file.path.data);
		}
		else if (fileExt.Equals("h", true))
		{
			fileBuilder.AppendLinef("\t<ClInclude Include=\"%.*s\">", file.path.length, file.path.data);
			fileBuilder.AppendLinef("\t\t<Filter>%.*s</Filter>", filterPath.length, filterPath.data);
			fileBuilder.AppendLinef("\t</ClInclude>", file.path.length, file.path.data);
		}
		else
		{
			fileBuilder.AppendLinef("\t<None Include=\"%.*s\">", file.path.length, file.path.data);
			fileBuilder.AppendLinef("\t\t<Filter>%.*s</Filter>", filterPath.length, filterPath.data);
			fileBuilder.AppendLinef("\t</None>", file.path.length, file.path.data);
		}
	}

	DestroyFileIterator(fileIt);

	builder.AppendLine("<ItemGroup>");

	String filterGuid = GenerateGuid(scratch.arena);

	builder.AppendLinef("\t<Filter Include=\"%.*s\">", filterPath.length, filterPath.data);
	builder.AppendLinef("\t\t<UniqueIdentifier>{%.*s}</UniqueIdentifier>", filterGuid.length, filterGuid.data);
	builder.AppendLine("\t</Filter>");

	if (fileBuilder.length != 0)
	{
		builder.Append(fileBuilder.ToString(scratch.arena));
	}

	builder.AppendLine("</ItemGroup>");
}

void GenerateProject()
{
	ArenaScope scratch = GetScratchArena();

	StringBuilder builder(scratch.arena);

	String platforms[] = { "x64" };
	String configs[] = { "Debug", "Release" };

	// https://learn.microsoft.com/en-us/cpp/build/reference/vcxproj-file-structure?view=msvc-170

	builder.AppendLine("<?xml version=\"1.0\" encoding=\"utf-8\"?>");
	builder.AppendLine("<Project DefaultTargets=\"Build\" xmlns=\"http://schemas.microsoft.com/developer/msbuild/2003\">");

	{
		builder.AppendLine("<ItemGroup Label=\"ProjectConfigurations\">");

		for (String config : configs)
		{
			for (String platform : platforms)
			{
				builder.AppendLinef("\t<ProjectConfiguration Include=\"%.*s|%.*s\">", config.length, config.data, platform.length, platform.data);
				builder.AppendLinef("\t\t<Configuration>%.*s</Configuration>", config.length, config.data);
				builder.AppendLinef("\t\t<Platform>%.*s</Platform>", platform.length, platform.data);
				builder.AppendLine("\t</ProjectConfiguration>");
			}
		}

		builder.AppendLine("</ItemGroup>");
	}

	{
		String projectGuid = GenerateGuid(scratch.arena);

		builder.AppendLine("<PropertyGroup Label=\"Globals\">");
		builder.AppendLinef("\t<ProjectGuid>{%.*s}</ProjectGuid>", projectGuid.length, projectGuid.data);
		builder.AppendLine("\t<VCProjectVersion>17.0</VCProjectVersion>");
		builder.AppendLine("\t<Keyword>MakeFileProj</Keyword>");
		builder.AppendLine("</PropertyGroup>");
	}

	builder.AppendLine("<Import Project=\"$(VCTargetsPath)\\Microsoft.Cpp.default.props\" Condition=\"Exists('$(VCTargetsPath)\\Microsoft.Cpp.default.props')\"/>");

	{
		builder.AppendLine("<PropertyGroup Label=\"Configuration\">");
		builder.AppendLine("\t<ConfigurationType>Makefile</ConfigurationType>");
		builder.AppendLine("\t<PlatformToolset>v143</PlatformToolset>");
		builder.AppendLine("</PropertyGroup>");
	}

	builder.AppendLine("<Import Project=\"$(VCTargetsPath)\\Microsoft.Cpp.props\" Condition=\"Exists('$(VCTargetsPath)\\Microsoft.Cpp.props')\"/>");

	builder.AppendLine("<ImportGroup Label=\"ExtensionSettings\"/>");
	builder.AppendLine("<ImportGroup Label=\"PropertySheets\"/>");
	builder.AppendLine("<PropertyGroup Label=\"UserMacros\"/>");

	{
		builder.AppendLine("<PropertyGroup>");
		builder.AppendLine("\t<BuildCommand Condition=\"'$(OS)' == 'Windows_NT'\">Build.bat</BuildCommand>");
		builder.AppendLine("\t<BuildCommand Condition=\"'$(OS)' != 'Windows_NT'\">bash Build.sh</BuildCommand>");
		builder.AppendLine("\t<NMakePreprocessorDefinitions>$(NMakePreprocessorDefinitions);BK_BUILD</NMakePreprocessorDefinitions>");
		builder.AppendLine("\t<IncludePath>$(IncludePath);Source;ThirdParty</IncludePath>");
		builder.AppendLine("\t<AdditionalOptions>/std:c++20</AdditionalOptions>");
		builder.AppendLine("\t<OutDir>Build\\Cache</OutDir>");
		builder.AppendLine("\t<IntDir>Build\\Cache</IntDir>");
		builder.AppendLine("</PropertyGroup>");

		for (String config : configs)
		{
			for (String platform : platforms)
			{
				builder.AppendLinef("<PropertyGroup Condition=\"'$(Configuration)|$(Platform)' == '%.*s|%.*s'\">",
				                    config.length, config.data, platform.length, platform.data);

				builder.AppendLine("\t<NMakeBuildCommandLine>$(BuildCommand)</NMakeBuildCommandLine>");
				builder.AppendLine("\t<NMakeReBuildCommandLine>$(BuildCommand)</NMakeReBuildCommandLine>");
				builder.AppendLine("\t<NMakeCleanCommandLine>$(BuildCommand)</NMakeCleanCommandLine>");
				builder.AppendLine("\t<NMakeOutput>Build\\BkBuild.exe</NMakeOutput>");

				builder.AppendLinef("</PropertyGroup>");
			}
		}
	}

	GenerateProjectFiles(builder, "Source");

	builder.AppendLine("<Import Project=\"$(VCTargetsPath)\\Microsoft.Cpp.targets\" Condition=\"Exists('$(VCTargetsPath)\\Microsoft.Cpp.targets')\"/>");
	builder.AppendLine("<ImportGroup Label=\"ExtensionTargets\"/>");

	builder.AppendLine("</Project>");

	WriteTextFile("Brink.vcxproj", builder.ToString(scratch.arena));

	builder.Reset();

	builder.AppendLine("<?xml version=\"1.0\" encoding=\"utf-8\"?>");
	builder.AppendLine("<Project xmlns=\"http://schemas.microsoft.com/developer/msbuild/2003\">");

	GenerateProjectFilters(builder, "Source");

	builder.AppendLine("</Project>");

	WriteTextFile("Brink.vcxproj.filters", builder.ToString(scratch.arena));
}

void PrepareBuildContext(Arena& arena, BuildContext& context)
{
	StringBuilder builder(arena);
	builder.AppendPath("Build/Cache");
	builder.AppendPath(GetPlatformName(context.platform));
	builder.AppendPath(context.config == BuildConfig::Debug ? "Debug" : "Release");

	context.cacheDir = builder.ToString(arena);

	for (size_t cachePathIdx = 0; cachePathIdx < context.cacheDir.length;)
	{
		size_t slashIdx = context.cacheDir.Slice(cachePathIdx).Find('/');
		if (slashIdx != SIZE_MAX)
		{
			slashIdx += cachePathIdx;
			cachePathIdx = slashIdx + 1;
		}
		else
		{
			cachePathIdx = SIZE_MAX;
		}

		String cachePath = context.cacheDir.Slice(0, slashIdx);
		CreateDirectory(cachePath);
	}
}

void SelfUpdate(int32 argc, char** argv)
{
	ArenaScope scratch = GetScratchArena();

	BuildContext context = {
		.platform = GetPlatform(),
		.config = BuildConfig::Debug,
		.includes = scratch.arena.Copy<String>({ "Source", "ThirdParty" }),
		.definitions = scratch.arena.Copy<String>({ "BK_BUILD" }),
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
	builder.AppendPath(argv[0]);
	builder.NormalizePath();
	builder.Append(".bak");

	String backupFile = builder.ToString(scratch.arena);
	String targetFile = backupFile.Slice(0, backupFile.length - 4);

	DeleteFile(backupFile);
	MoveFile(targetFile, backupFile);

	if (!LinkModules(context, { "Core", "Build" }, targetFile))
	{
		MoveFile(backupFile, targetFile);

		printf("Failed to link during self-update\n");
		ExitApp(1);
	}

	builder.Reset();
	for (int32 i = 1; i < argc; ++i)
	{
		String argValue = argv[i];

		if (argValue.Contains(' '))
		{
			builder.Append('\"');
			builder.Append(argValue);
			builder.Append("\"");
		}
		else
		{
			builder.Append(argValue);
		}

		builder.Append(' ');
	}

	ProcessHandle process = CreateProcess({
		.executable = argv[0],
		.arguments = builder.ToString(scratch.arena),
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
		.includes = arena.Copy<String>({ "Source", "ThirdParty" }),
		.definitions = arena.Copy<String>({ "BK_BUILD" }),
	};

	String singleFile = String::Empty;
	bool generateProject = false;

	for (int32 i = 1; i < argc; ++i)
	{
		String argName = argv[i];
		String argValue = String::Empty;

		size_t equalsIdx = argName.Find('=');
		if (equalsIdx != SIZE_MAX)
		{
			argValue = argName.Slice(equalsIdx + 1);
			argName = argName.Slice(0, equalsIdx);
		}

		if (argName.Equals("-GenerateProject", true))
		{
			generateProject = true;
		}
		else if (argName.Equals("-Platform", true))
		{
			if (argValue.Equals("Windows", true))
			{
				context.platform = Platform::Windows;
			}
			else if (argValue.Equals("MacOS", true))
			{
				context.platform = Platform::MacOS;
			}
			else if (argValue.Equals("Emscripten", true))
			{
				context.platform = Platform::Emscripten;
			}
		}
		else if (argName.Equals("-Config", true))
		{
			if (argValue.Equals("Debug", true))
			{
				context.config = BuildConfig::Debug;
			}
			else if (argValue.Equals("Release", true))
			{
				context.config = BuildConfig::Release;
			}
		}
		else if (argName.Equals("-SingleFile", true))
		{
			singleFile = argValue;
		}
	}

	PrepareBuildContext(arena, context);

	if (generateProject)
	{
		// GenerateProject();
		GenerateCompileCommands(context, { "Build", "Core", "Engine", "Renderer", "Sandbox" });
	}

	DateTime buildTime = GetLocalTime();
	double buildStartTime = GetTimeSec();

	printf("Build started at %02d:%02d:%02d\n", buildTime.hour, buildTime.minute, buildTime.second);

	if (singleFile.length != 0)
	{
		StringBuilder builder(arena);
		builder.AppendPath(context.cacheDir);
		builder.AppendPath(GetFileNameWithoutExtension(singleFile));
		builder.Append(".o");

		String objectFile = builder.ToString(arena);
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
		CreateDirectory("Build/Sandbox");
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

		String modules[] = { "Core", "Engine", "Renderer", "Sandbox" };

		ActionResult compileResult = CompileModules(context, modules);

		if (compileResult == ActionResult::Succeeded)
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
