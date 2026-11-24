// A PICTURE WRITTEN PLAINLY: a Direct3D surface or texture saved as a picture file without ever writing through a link. Written 2026-11-23
// by Melanikus (Drake) for the AvP port (card 6, Frost's review of #294).
//
// WHY: D3DXSaveSurfaceToFile and D3DXSaveTextureToFile open their file by its name and follow whatever is there, as a plain CreateFile
// does (plain_write.h says why that matters). So the picture is made in memory first (D3DXSave...ToFileInMemory), as the editor's marks
// already do (mark_key.cpp), and its bytes are written through PlainWrite_WriteWholeFile. It is its own header so that plain_write.h needs
// no Direct3D. tests/plain_write_test.cpp tries the write part (PlainWrite_WriteWholeFile) and finds, by reading, that no D3DX picture
// is saved straight to a file anywhere in the game.
#ifndef PLAIN_WRITE_PICTURE_H
#define PLAIN_WRITE_PICTURE_H

#include <d3dx9.h>
#include <string>

#include "plain_write.h"

// A surface saved as a picture file (format: D3DXIFF_PNG, D3DXIFF_JPG, ...). False, with why set, when D3DX can't make it or it's refused.
inline bool PlainWrite_SurfaceToFile(const std::string &path, D3DXIMAGE_FILEFORMAT format, IDirect3DSurface9 *surface, std::string *why)
{
	std::string ignored;
	std::string &whyNot = why ? *why : ignored;
	ID3DXBuffer *picture = 0;

	if (!surface || FAILED(D3DXSaveSurfaceToFileInMemory(&picture, format, surface, NULL, NULL)) || !picture)
	{
		whyNot = "Direct3D couldn't make the picture";
		return false;
	}

	bool const written = PlainWrite_WriteWholeFile(path, picture->GetBufferPointer(), picture->GetBufferSize(), &whyNot);
	picture->Release();
	return written;
}

// A texture saved as a picture file, the same way
inline bool PlainWrite_TextureToFile(const std::string &path, D3DXIMAGE_FILEFORMAT format, IDirect3DBaseTexture9 *texture, std::string *why)
{
	std::string ignored;
	std::string &whyNot = why ? *why : ignored;
	ID3DXBuffer *picture = 0;

	if (!texture || FAILED(D3DXSaveTextureToFileInMemory(&picture, format, texture, NULL)) || !picture)
	{
		whyNot = "Direct3D couldn't make the picture";
		return false;
	}

	bool const written = PlainWrite_WriteWholeFile(path, picture->GetBufferPointer(), picture->GetBufferSize(), &whyNot);
	picture->Release();
	return written;
}

#endif
