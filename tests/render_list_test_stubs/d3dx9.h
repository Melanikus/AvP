// A stand-in for the DirectX helper library's header, for the render list test only. Written 2026-11-04 by Melanikus for
// the AvP port.
//
// The game's graphics header (win32/d3_func.h) includes the real d3dx9.h, which comes from a package the tests don't
// install. The render lists never use anything from it. They only need these names to exist, because the same header
// declares structures that mention them, so that's all this file provides.

#ifndef render_list_test_d3dx9_h
#define render_list_test_d3dx9_h

#include <d3d9.h>

struct D3DXMATRIX  { float numbers[16]; };
struct D3DXPLANE   { float a, b, c, d; };
struct D3DXVECTOR3 { float x, y, z; };

struct ID3DXConstantTable;
typedef const char *D3DXHANDLE;

#endif
