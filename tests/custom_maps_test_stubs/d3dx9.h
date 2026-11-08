// A stand-in for the DirectX helper library's header, for the custom maps test only. Written 2026-11-07 by Melanikus for the AvP port.
// Credit: Melanikus and Bobblen (the custom single-player maps follow Bobblen's avp-forever, rewritten).
//
// custom_maps.cpp asks for pictures through TextureManager.h, whose graphics header (win32/d3_func.h) includes the real d3dx9.h. That
// comes from a package the tests don't install, and nothing the test runs uses it. The header only needs these names to exist,
// because it declares structures that mention them, so that's all this file provides.

#ifndef custom_maps_test_d3dx9_h
#define custom_maps_test_d3dx9_h

#include <d3d9.h>

struct D3DXMATRIX  { float numbers[16]; };
struct D3DXPLANE   { float a, b, c, d; };
struct D3DXVECTOR3 { float x, y, z; };

struct ID3DXConstantTable;
typedef const char *D3DXHANDLE;

#endif
