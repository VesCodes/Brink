#include "Core/Core.h"
#include "Core/Memory.h"
#include "Core/Platform.h"
#include "Core/Span.h"
#include "Core/String.h"

#ifndef BK_BUILD
#include "Core/Core.cpp"
#include "Core/Memory.cpp"
#include "Core/Platform.cpp"
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
		result = WriteFile(fileHandle, TSpan((uint8_t*)content.data, content.length)) == content.length;
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

bool CompileModule(const BuildContext& context, String moduleName)
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
	builder.Append(".module.cpp");

	String moduleUnityFile = builder.ToString(scratch.arena);

	builder.Reset(builder.length - 3);
	builder.Append("o");

	String moduleObjectFile = builder.ToString(scratch.arena);

	if (!ShouldCompile(context, moduleObjectFile))
	{
		return true;
	}

	builder.Reset();
	builder.AppendLine("// Automatically generated module unity file");

	FileIteratorHandle fileIterator = CreateFileIterator(scratch.arena, moduleSourceDir);
	for (FileIteratorEntry entry; AdvanceFileIterator(fileIterator, entry);)
	{
		if (entry.path.EndsWith(".cpp", true))
		{
			String sourceFile = entry.path.Slice(moduleSourceDirPrefixLength);
			builder.AppendLinef("#include \"%.*s\"", sourceFile.length, sourceFile.data);
		}
	}

	DestroyFileIterator(fileIterator);

	if (!WriteTextFile(moduleUnityFile, builder.ToString(scratch.arena)))
	{
		printf("Failed to write module unity file '%.*s'\n", int32(moduleUnityFile.length), moduleUnityFile.data);
		return false;
	}

	ProcessHandle process = CompileFile(context, moduleUnityFile, moduleObjectFile);

	int32 processExitCode = -1;
	if (!process || !WaitForProcess(process, &processExitCode))
	{
		return false;
	}

	return processExitCode == 0;
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

	if (fileBuilder.length > 0)
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

	if (fileBuilder.length > 0)
	{
		builder.Append(fileBuilder.ToString(scratch.arena));
	}

	builder.AppendLine("</ItemGroup>");
}

void GenerateProject()
{
	ArenaScope scratch = GetScratchArena();

	StringBuilder builder(scratch.arena);

	TSpan<String> platforms = { "x64" };
	TSpan<String> configs = { "Debug", "Release" };

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

int32 main(int32 argc, char** argv)
{
	Arena arena = {};

	BuildContext context = {
		.platform = GetPlatform(),
		.config = BuildConfig::Debug,
		.includes = { "Source", "ThirdParty" },
		.definitions = { "BK_BUILD" },
	};

	for (int32 i = 1; i < argc; ++i)
	{
		String argument = argv[i];
		if (argument == "-GenerateProject")
		{
			GenerateProject();
		}
	}

	// Build
	{
		double startTime = GetTimeSec();

		PrepareBuildContext(arena, context);

		if (!CompileModule(context, "Core"))
		{
			printf("Failed to compile Core module\n");
			return 1;
		}

		if (!CompileModule(context, "Build"))
		{
			printf("Failed to compile Build module\n");
			return 1;
		}

		double compileTime = GetTimeSec();
		printf("Compiled modules for Build in %0.4fs\n", compileTime - startTime);

		StringBuilder builder(arena);
		builder.AppendPath(argv[0]);
		builder.NormalizePath();
		builder.Append(".bak");

		String backupFile = builder.ToString(arena);
		String targetFile = backupFile.Slice(0, backupFile.length - 4);

		DeleteFile(backupFile);
		MoveFile(targetFile, backupFile);

		if (!LinkModules(context, { "Core", "Build" }, targetFile))
		{
			MoveFile(backupFile, targetFile);

			printf("Failed to link Build target\n");
			return 1;
		}

		double linkTime = GetTimeSec();
		printf("Linked modules for Build in %0.4fs\n", linkTime - compileTime);
	}

	// Sandbox
	{
		double startTime = GetTimeSec();

		context.platform = Platform::Emscripten;

		context.extraCompilerArguments = {
			"--use-port=emdawnwebgpu",
		};

		context.extraLinkerArguments = {
			"--use-port=emdawnwebgpu",
			"--shell-file=Source/Sandbox/Sandbox.html",
		};

		PrepareBuildContext(arena, context);

		if (!CompileModule(context, "Core"))
		{
			printf("Failed to compile Core module\n");
			return 1;
		}

		if (!CompileModule(context, "Sandbox"))
		{
			printf("Failed to compile Sandbox module\n");
			return 1;
		}

		double compileTime = GetTimeSec();
		printf("Compiled modules for Sandbox in %0.4fs\n", compileTime - startTime);

		if (!LinkModules(context, { "Core", "Sandbox" }, "Build/index.html"))
		{
			printf("Failed to link Sandbox target\n");
			return 1;
		}

		double linkTime = GetTimeSec();
		printf("Linked modules for Sandbox in %0.4fs\n", linkTime - compileTime);
	}

	return 0;
}
