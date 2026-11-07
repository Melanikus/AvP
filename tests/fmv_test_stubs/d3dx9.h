// A stand-in for the DirectX helper library's header, for the video test only. Written 2026-11-04 by Melanikus for the AvP port.
//
// The game's graphics header (win32/d3_func.h) includes the real d3dx9.h, which comes from a package the tests don't install. The
// video code never uses anything from it. It only needs these names to exist, because that header declares structures that mention them.

#ifndef fmv_test_d3dx9_h
#define fmv_test_d3dx9_h

#include <d3d9.h>

struct D3DXMATRIX  { float numbers[16]; };
struct D3DXPLANE   { float a, b, c, d; };
struct D3DXVECTOR3 { float x, y, z; };

struct ID3DXConstantTable;
typedef const char *D3DXHANDLE;

#endif
