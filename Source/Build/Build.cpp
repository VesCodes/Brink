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

	if (context.platform == "Emscripten")
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

	if (context.platform == "Windows")
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

void PrepareBuildContext(Arena& arena, BuildContext& context)
{
	StringBuilder builder(arena);
	builder.AppendPath("Build/Cache");
	builder.AppendPath(context.platform);
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
		.config = BuildConfig::Debug,
		.includes = { "Source", "ThirdParty" },
		.definitions = { "BK_BUILD" },
	};

	// Build
	{
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
	}

	// Sandbox
	{
		context.platform = "Emscripten";

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

		if (!LinkModules(context, { "Core", "Sandbox" }, "Build/index.html"))
		{
			printf("Failed to link Sandbox target\n");
			return 1;
		}
	}

	return 0;
}
