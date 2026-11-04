// Compiles every shader exactly the way the game does. Written 2026-11-04 by Melanikus for the AvP port.
//
// The game compiles its shaders from text files each time it starts, with Microsoft's D3DX library. tests/compile-shaders.ps1 checks
// them with Microsoft's newest compiler, but D3DX uses a much older one, and the two don't always agree, so a shader could pass
// that check and still fail in the game. This test uses the very same library, the same call and the same settings as
// R_CreateVertexShader and R_CreatePixelShader in win32/d3_func.cpp. Any message from the compiler counts as a failure, warnings
// included. Nothing here needs a graphics card: this only turns text into shader code, and throws the code away.
//
// The library isn't linked in, because vcpkg fetches it, and the tests can be built without vcpkg: CodeQL does. So this loads it once
// it's running. Building the game fetches it, and so does tests.yml, and this has to run from the top of the repository.
//
// A .fxh file isn't a shader. It's included by others, so it's checked as part of each one that includes it.

#include <windows.h>
#include <unknwn.h>
#include <stdio.h>

#include <filesystem>
#include <string>

#include "shader_model.h"

// Where the build puts Microsoft's libraries, for the 32-bit game. D3DX loads the second one itself, by name, while it's running.
static char const *const LibraryFolder = "vcpkg_installed\\x86-windows\\bin";
static char const *const CompilerLibrary = "D3DCompiler_43.dll";
static char const *const D3DXLibrary = "D3DX9_43.dll";

// D3DX hands back its answers in buffers. This is the part of its ID3DXBuffer that's needed here, in the same order as Microsoft's.
struct CompilerBuffer : public IUnknown
{
	virtual void *STDMETHODCALLTYPE GetBufferPointer() = 0;
	virtual DWORD STDMETHODCALLTYPE GetBufferSize() = 0;
};

// The game's call, D3DXCompileShaderFromFile, written out by hand because its header only exists after the game has been built.
typedef HRESULT (WINAPI *CompileFromFile)(char const *file, void const *macros, void *includes, char const *function, char const *profile,
	DWORD flags, CompilerBuffer **code, CompilerBuffer **messages, IUnknown **constants);

static HMODULE LoadFromBuild(char const *name)
{
	std::string const path = std::filesystem::absolute(std::filesystem::path(LibraryFolder) / name).string();

	return LoadLibraryExA(path.c_str(), NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
}

// Compiles one file, and says whether it went through with nothing to report.
static bool CompilesCleanly(CompileFromFile compile, std::filesystem::path const &file)
{
	std::string const name = file.filename().string();
	bool const vertexShader = file.extension() == ".vsh";
	std::string const kind = vertexShader ? "vs" : "ps";
	std::string const function = kind + "_main";
	std::string const profile = kind + (ShaderNeedsModel3(name) ? "_3_0" : "_2_0");

	CompilerBuffer *code = 0;
	CompilerBuffer *messages = 0;
	IUnknown *constants = 0;

	HRESULT const result = compile(file.string().c_str(), NULL, NULL, function.c_str(), profile.c_str(), 0, &code, &messages, &constants);
	bool const clean = SUCCEEDED(result) && code && !messages;

	printf("  %s  %s as %s\n", clean ? "ok" : "FAILED", name.c_str(), profile.c_str());

	if (messages)
	{
		// the compiler's own text, which ends with a zero
		printf("%s\n", (char const *)messages->GetBufferPointer());
		messages->Release();
	}

	if (code)
	{
		code->Release();
	}

	if (constants)
	{
		constants->Release();
	}

	return clean;
}

int main()
{
	puts("Compiling every shader with the game's own compiler");

	// Loaded first, so that when D3DX asks for it by name it gets the one from the build, and this test can't pass or fail
	// on what happens to be installed on this machine.
	HMODULE const compilerLibrary = LoadFromBuild(CompilerLibrary);
	HMODULE const d3dx = LoadFromBuild(D3DXLibrary);

	if (!compilerLibrary || !d3dx)
	{
		printf("Couldn't load %s from %s. Build the game first, or fetch it as tests.yml does, and run this from the top of the repository.\n", D3DXLibrary, LibraryFolder);
		return 1;
	}

	CompileFromFile const compile = (CompileFromFile)GetProcAddress(d3dx, "D3DXCompileShaderFromFileA");

	if (!compile)
	{
		puts("The library has no D3DXCompileShaderFromFileA");
		return 1;
	}

	int shaders = 0;
	int failed = 0;

	for (std::filesystem::directory_entry const &entry : std::filesystem::directory_iterator("shaders"))
	{
		std::filesystem::path const extension = entry.path().extension();

		if (extension != ".vsh" && extension != ".psh")
		{
			continue;
		}

		shaders++;

		if (!CompilesCleanly(compile, entry.path()))
		{
			failed++;
		}
	}

	// With no shaders at all, every shader would have passed, which would prove nothing.
	if (shaders == 0 || failed != 0)
	{
		printf("The shader test FAILED: %d of %d\n", failed, shaders);
		return 1;
	}

	printf("The shader test passed: %d shaders\n", shaders);
	return 0;
}
