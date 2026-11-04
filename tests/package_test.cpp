// Tries a finished package's shader compiler the way a tester's machine would use it. Written 2026-11-04 by Melanikus for the AvP port.
//
// The game compiles its shaders from text files every time it starts. It uses Microsoft's D3DX library, and D3DX loads a second library,
// D3DCompiler_43.dll, by name while it's running. Nothing links to that second one, so a build can leave it behind with no error. On most
// developers' machines nobody would notice, because the old DirectX runtime put a copy in Windows' own folder. On a clean Windows, every
// shader would fail and the game wouldn't start.
//
// So it isn't enough to ask whether a shader compiles. A machine's own copy could make that pass. This also asks Windows WHICH FILE it
// gets for that name, and fails unless it's the one in the package. That means it can catch a missing file on any machine, even one that
// has its own copy. This relies on D3DX asking for it by name like anything else, which comes from reading the DLL.
//
// HOW TO USE IT: copy this program into the package's folder and run it from there with --here. It finds the libraries the same way the
// game does, with Windows looking beside the program first. game-build.yml does this.
//
// With no arguments, which is how tests.yml runs every test, it only checks its own rule on made-up paths.
//
// WHAT IT CAN'T SHOW: that the game itself starts on a clean Windows. It compiles one small shader, and needs no graphics card, so it says
// nothing about the rest of the game. tests/shader_test.cpp is the one that compiles every real shader.

#include <windows.h>
#include <unknwn.h>
#include <stdio.h>
#include <string.h>

static char const *const D3DXLibrary = "d3dx9_43.dll";
static char const *const CompilerLibrary = "D3DCompiler_43.dll";

// Says whether a file sits directly in a folder, and not in one inside it, or in a folder whose name only starts the same way.
// Windows doesn't mind about capital letters in a path, so neither does this. folder has no slash on its end.
static bool FileIsDirectlyIn(char const *path, char const *folder)
{
	size_t const folderLength = strlen(folder);

	if (folderLength == 0 || _strnicmp(path, folder, folderLength) != 0)
	{
		return false;
	}

	char const *rest = path + folderLength;

	if (*rest != '\\' && *rest != '/')
	{
		return false;
	}

	rest++;

	return *rest != 0 && !strchr(rest, '\\') && !strchr(rest, '/');
}

static int checksFailed = 0;

static void Check(bool good, char const *what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what);

	if (!good)
	{
		checksFailed++;
	}
}

// With no arguments: the rule, on paths made up for it
static int CheckTheRule()
{
	Check(FileIsDirectlyIn("C:\\package\\D3DCompiler_43.dll", "C:\\package"), "a file in the folder");
	Check(FileIsDirectlyIn("c:\\PACKAGE\\d3dcompiler_43.dll", "C:\\package"), "capital letters don't matter");
	Check(!FileIsDirectlyIn("C:\\Windows\\SysWOW64\\D3DCompiler_43.dll", "C:\\package"), "Windows' own copy doesn't count");
	Check(!FileIsDirectlyIn("C:\\package2\\D3DCompiler_43.dll", "C:\\package"), "a folder whose name only starts the same doesn't count");
	Check(!FileIsDirectlyIn("C:\\package\\inner\\D3DCompiler_43.dll", "C:\\package"), "a folder inside it doesn't count");
	Check(!FileIsDirectlyIn("C:\\package\\", "C:\\package"), "the folder itself isn't a file in it");
	Check(!FileIsDirectlyIn("", "C:\\package"), "no path at all");
	Check(!FileIsDirectlyIn("C:\\package\\x.dll", ""), "no folder at all");

	if (checksFailed)
	{
		printf("FAILED: %d checks\n", checksFailed);
		return 1;
	}

	puts("all package test checks passed");
	return 0;
}

// D3DX hands back its answers in buffers. This is the part of its ID3DXBuffer that's needed here, in the same order as Microsoft's.
struct CompilerBuffer : public IUnknown
{
	virtual void *STDMETHODCALLTYPE GetBufferPointer() = 0;
	virtual DWORD STDMETHODCALLTYPE GetBufferSize() = 0;
};

// D3DXCompileShader, written out by hand because its header only exists after the game has been built
typedef HRESULT (WINAPI *CompileText)(char const *text, UINT length, void const *macros, void *includes, char const *function, char const *profile,
	DWORD flags, CompilerBuffer **code, CompilerBuffer **messages, IUnknown **constants);

// Which file a loaded library came from. Says false if it isn't loaded, or its path doesn't fit.
static bool WhereLoadedFrom(char const *name, char *path, DWORD size)
{
	HMODULE const library = GetModuleHandleA(name);

	if (!library)
	{
		return false;
	}

	DWORD const length = GetModuleFileNameA(library, path, size);

	return length != 0 && length < size;
}

// With --here: the real thing, in the folder this program is in
static int CheckThePackage()
{
	char folder[MAX_PATH];
	DWORD const length = GetModuleFileNameA(NULL, folder, MAX_PATH);

	if (length == 0 || length >= MAX_PATH)
	{
		puts("FAILED: this program can't tell which folder it's in");
		return 1;
	}

	char *const lastSlash = strrchr(folder, '\\');

	if (!lastSlash)
	{
		puts("FAILED: this program can't tell which folder it's in");
		return 1;
	}

	*lastSlash = 0;
	printf("Trying the package in %s\n", folder);

	// by name alone, as the game does, so Windows looks where it would look for the game
	HMODULE const d3dx = LoadLibraryA(D3DXLibrary);
	char path[MAX_PATH];

	if (!d3dx || !WhereLoadedFrom(D3DXLibrary, path, MAX_PATH))
	{
		printf("FAILED: %s couldn't be loaded from anywhere\n", D3DXLibrary);
		return 1;
	}

	printf("  %s came from %s\n", D3DXLibrary, path);

	if (!FileIsDirectlyIn(path, folder))
	{
		printf("FAILED: %s isn't in the package. That copy belongs to this machine, and a clean Windows wouldn't have it\n", D3DXLibrary);
		return 1;
	}

	// D3DX loads the compiler only while it's compiling, and lets go of it before it returns, so asking Windows afterwards finds nothing:
	// a first version of this test did that, and saw it. So this asks for it first, by name, the same way D3DX does, which gets the file
	// D3DX would get. It stays loaded while the shader compiles, so that shader goes through this very file.
	if (!LoadLibraryA(CompilerLibrary) || !WhereLoadedFrom(CompilerLibrary, path, MAX_PATH))
	{
		printf("FAILED: %s couldn't be loaded from anywhere, so no shader can compile on this machine\n", CompilerLibrary);
		return 1;
	}

	printf("  %s came from %s\n", CompilerLibrary, path);

	if (!FileIsDirectlyIn(path, folder))
	{
		printf("FAILED: %s isn't in the package. That copy belongs to this machine, and on a clean Windows every shader would fail\n", CompilerLibrary);
		return 1;
	}

	CompileText const compile = (CompileText)GetProcAddress(d3dx, "D3DXCompileShader");

	if (!compile)
	{
		printf("FAILED: %s has no D3DXCompileShader\n", D3DXLibrary);
		return 1;
	}

	static char const shader[] = "float4 vs_main(float4 place : POSITION) : POSITION { return place; }";
	CompilerBuffer *code = 0;
	CompilerBuffer *messages = 0;
	IUnknown *constants = 0;

	HRESULT const result = compile(shader, (UINT)strlen(shader), NULL, NULL, "vs_main", "vs_2_0", 0, &code, &messages, &constants);
	bool const compiled = SUCCEEDED(result) && code != 0;

	if (messages)
	{
		printf("  the compiler said: %s\n", (char const *)messages->GetBufferPointer());
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

	if (!compiled)
	{
		printf("FAILED: the package's compiler didn't compile a small shader (its answer was 0x%08lX)\n", (unsigned long)result);
		return 1;
	}

	puts("The package's own compiler compiled a shader");
	return 0;
}

int main(int argc, char **argv)
{
	if (argc == 2 && strcmp(argv[1], "--here") == 0)
	{
		return CheckThePackage();
	}

	if (argc != 1)
	{
		puts("FAILED: the only thing this takes is --here");
		return 1;
	}

	return CheckTheRule();
}
