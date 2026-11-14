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
//
// Modified 2026-11-14 by Melanikus (Dietrich) for the AvP port (R-LAMP-1): each shader's size is printed against what its shader model
// allows, read from the same library's own listing of the code, so a shader near its limit (shoulderLamp.psh) can be measured before and
// after a change. One over the limit fails here, as it would fail to compile in the game.

#include <windows.h>
#include <unknwn.h>
#include <stdio.h>
#include <string.h>

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

// D3DXDisassembleShader, which writes compiled shader code out as text. Its last line says how big the shader is.
typedef HRESULT (WINAPI *Disassemble)(DWORD const *code, BOOL colours, char const *comments, CompilerBuffer **text);

// How big a shader is, from the line the listing ends with: "// approximately 63 instruction slots used (4 texture, 59 arithmetic)". A shader
// model 3 listing may give the total alone, and a shader of one sum says "1 instruction slot used", with no parts. With no parts given, every
// slot counts as a sum, which is the most it could be. Gives back false when there's no such line.
static bool ShaderSize(char const *listing, int *slots, int *texture, int *arithmetic)
{
	char const *const line = strstr(listing, "// approximately ");

	if (!line || sscanf_s(line, "// approximately %d instruction slot", slots) != 1)
	{
		return false;
	}

	char const *const lineEnd = strchr(line, '\n');
	char const *const parts = strchr(line, '(');

	if (!parts || (lineEnd && parts > lineEnd) || sscanf_s(parts, "(%d texture, %d arithmetic)", texture, arithmetic) != 2)
	{
		*texture = 0;
		*arithmetic = *slots;
	}

	return true;
}

static HMODULE LoadFromBuild(char const *name)
{
	std::string const path = std::filesystem::absolute(std::filesystem::path(LibraryFolder) / name).string();

	return LoadLibraryExA(path.c_str(), NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
}

// Prints how big a compiled shader is against what its shader model allows, and says whether it fits. Shader model 2 counts its sums and its
// texture reads apart: 64 and 32 for a pixel shader, 256 in all for a vertex shader. Shader model 3 allows at least 512 of either.
static bool PrintsItsSize(Disassemble disassemble, CompilerBuffer *code, std::string const &profile)
{
	CompilerBuffer *listing = 0;

	if (!disassemble || FAILED(disassemble((DWORD const *)code->GetBufferPointer(), FALSE, NULL, &listing)) || !listing)
	{
		printf("      its size couldn't be listed\n");
		return true;
	}

	// The listing is the library's own text. It's copied by its size, so a missing end can't be read past.
	std::string const text((char const *)listing->GetBufferPointer(), listing->GetBufferSize());
	listing->Release();

	int slots = 0;
	int texture = 0;
	int arithmetic = 0;

	if (!ShaderSize(text.c_str(), &slots, &texture, &arithmetic))
	{
		printf("      its size isn't in its listing\n");
		return true;
	}

	bool const modelTwoPixels = profile == "ps_2_0";
	bool const fits = modelTwoPixels ? (arithmetic >= 0 && arithmetic <= 64 && texture <= 32) : slots <= (profile == "vs_2_0" ? 256 : 512);

	if (modelTwoPixels)
	{
		printf("      %s %d sums of 64 and %d texture reads of 32 (%d slots in all)\n", fits ? "size:" : "TOO BIG:", arithmetic, texture, slots);
	}
	else
	{
		printf("      %s %d slots of %d\n", fits ? "size:" : "TOO BIG:", slots, profile == "vs_2_0" ? 256 : 512);
	}

	return fits;
}

// Compiles one file, and says whether it went through with nothing to report.
static bool CompilesCleanly(CompileFromFile compile, Disassemble disassemble, std::filesystem::path const &file)
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
	bool clean = SUCCEEDED(result) && code && !messages;

	printf("  %s  %s as %s\n", clean ? "ok" : "FAILED", name.c_str(), profile.c_str());

	if (clean)
	{
		clean = PrintsItsSize(disassemble, code, profile);
	}

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

	// The same library's listing of compiled code, for each shader's size. Without it the sizes aren't printed, and the compile alone decides.
	Disassemble const disassemble = (Disassemble)GetProcAddress(d3dx, "D3DXDisassembleShader");

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

		if (!CompilesCleanly(compile, disassemble, entry.path()))
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
