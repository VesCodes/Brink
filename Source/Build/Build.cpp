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
	String platform;
	BuildConfig config;

	String cacheDir;

	TSpan<String> includes;
	TSpan<String> systemIncludes;
	TSpan<String> definitions;

	TSpan<String> extraCompilerArguments;
	TSpan<String> extraLinkerArguments;
};

bool WriteTextFile(String outputFile, StringBuilder& builder)
{
	FileHandle fileHandle = OpenFile(outputFile, FileAccess::Write);
	if (!fileHandle)
	{
		return false;
	}

	ArenaScope scratch = GetScratchArena();

	String fileContent = builder.ToString(scratch.arena);
	size_t bytesWritten = WriteFile(fileHandle, TSpan((uint8_t*)fileContent.data, fileContent.length));

	CloseFile(fileHandle);

	return bytesWritten == fileContent.length;
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

	String dependencyFile = GetDependencyFilePath(scratch.arena, context, outputFile);
	if (!FileExists(dependencyFile))
	{
		return true;
	}

	FileHandle fileHandle = OpenFile(dependencyFile, FileAccess::Read);
	if (!fileHandle)
	{
		return true;
	}

	size_t fileSize = GetFileSize(fileHandle);

	TSpan<uint8> fileContent = scratch.arena.Push<uint8>(fileSize);
	if (ReadFile(fileHandle, fileContent))
	{
		// #TODO: Parse dependencies and check if out of date
		// String dependencyText((char*)fileContent.data, fileContent.length);
		// for (String token; ParseToken(dependencyText, token);)
		// {
		// 	printf("'%.*s'\n", int32(token.length), token.data);
		// }
	}

	CloseFile(fileHandle);

	return true;
}

ProcessHandle CompileFile(const BuildContext& context, String inputFile, String outputFile)
{
	ArenaScope scratch = GetScratchArena();

	ShouldCompile(context, outputFile);

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
	if (!WriteTextFile(responseFile, arguments))
	{
		printf("Failed to write response file '%.*s'\n", int32(responseFile.length), responseFile.data);
		return 0;
	}

	arguments.Reset();
	arguments.Append('@');
	arguments.Append(responseFile);

	return CreateProcess({
		.executable = "clang++",
		.arguments = arguments.ToString(scratch.arena),
	});
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

	arguments.AppendLine("-Wl,-incremental:no");

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
	if (!WriteTextFile(responseFile, arguments))
	{
		printf("Failed to write response file '%.*s'\n", int32(responseFile.length), responseFile.data);
		return 0;
	}

	arguments.Reset();
	arguments.Append('@');
	arguments.Append(responseFile);

	return CreateProcess({
		.executable = "clang++",
		.arguments = arguments.ToString(scratch.arena),
	});
}

ProcessHandle CompileModule(const BuildContext& context, String moduleName)
{
	ArenaScope scratch = GetScratchArena();

	StringBuilder builder(scratch.arena);
	builder.AppendPath("Source");
	builder.AppendPath(moduleName);

	String moduleSourceDir = builder.ToString(scratch.arena);

	builder.Reset();
	builder.AppendPath(context.cacheDir);
	builder.AppendPath(moduleName);
	builder.Append(".module.cpp");

	String moduleUnityFile = builder.ToString(scratch.arena);

	builder.Reset(builder.length - 3);
	builder.Append("o");

	String moduleObjectFile = builder.ToString(scratch.arena);

	builder.Reset();
	builder.AppendLine("// Automatically generated module unity file");

	FileIteratorHandle fileIterator = CreateFileIterator(scratch.arena, moduleSourceDir);
	for (FileIteratorEntry entry; AdvanceFileIterator(fileIterator, entry);)
	{
		if (entry.path.EndsWith(".cpp", true))
		{
			String sourceFile = entry.path.Slice(7);
			builder.AppendLinef("#include \"%.*s\"", sourceFile.length, sourceFile.data);
		}
	}

	DestroyFileIterator(fileIterator);

	if (!WriteTextFile(moduleUnityFile, builder))
	{
		printf("Failed to write module unity file '%.*s'\n", int32(moduleUnityFile.length), moduleUnityFile.data);
		return 0;
	}

	return CompileFile(context, moduleUnityFile, moduleObjectFile);
}

ProcessHandle LinkModules(const BuildContext& context, TSpan<String> moduleNames, String executableName)
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

	return LinkFiles(context, moduleObjectFiles, executableName);
}

int32 main(int32 argc, char** argv)
{
	Arena arena = {};

	BuildContext context = {
		.config = BuildConfig::Debug,
		.includes = { "Source", "ThirdParty" },
		.definitions = { "BK_BUILD" },
	};

	for (int32 i = 1; i < argc; ++i)
	{
		String argName = argv[i];
		String argValue = {};

		if (size_t equalsIdx = argName.Find('='); equalsIdx != SIZE_MAX)
		{
			argValue = argName.Slice(equalsIdx + 1);
			argName = argName.Slice(0, equalsIdx);
		}

		if (argName.Equals("-Platform", true))
		{
			context.platform = argValue;
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
	}

	StringBuilder builder(arena);
	builder.AppendPath("Build/Cache");
	// builder.AppendPath(context.platform);
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

	ProcessHandle process = CompileModule(context, "Core");
	int32 processExitCode = -1;

	if (!WaitForProcess(process, &processExitCode) || processExitCode != 0)
	{
		printf("Failed to compile Core module (%d)\n", processExitCode);
		return 1;
	}

	process = CompileModule(context, "Build");

	if (!WaitForProcess(process, &processExitCode) || processExitCode != 0)
	{
		printf("Failed to compile Build module (%d)\n", processExitCode);
		return 1;
	}

	DeleteFile("Build/Cache/BkBuild.exe");
	MoveFile("Build/BkBuild.exe", "Build/Cache/BkBuild.exe");

	process = LinkModules(context, { "Core", "Build" }, "Build/BkBuild.exe");
	if (!WaitForProcess(process, &processExitCode) || processExitCode != 0)
	{
		MoveFile("Build/Cache/BkBuild.exe", "Build/BkBuild.exe");

		printf("Failed to link BkBuild executable (%d)\n", processExitCode);
		return 1;
	}

	return 0;
}
