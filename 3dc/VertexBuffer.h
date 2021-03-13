// Copyright (C) 2010 Barry Duncan. All Rights Reserved.
// The original author of this code can be contacted at: bduncan22@hotmail.com

// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
//    this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED ''AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
// INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
// FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
// DEVELOPERS AND CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
// OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
// OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
// ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef _VertexBuffer_h_
#define _VertexBuffer_h_

#include "renderer.h"

class VertexBuffer
{
	public:
		r_VertexBuffer	_vertexBuffer;
		uint32_t		_sizeInBytes;
		enum R_USAGE	_usage;
		uint32_t		_stride;
		bool			_isLocked;

		// constructor
		VertexBuffer():
			_capacity(0),
			_sizeInBytes(0),
			_usage(USAGE_DYNAMIC),
			_stride(0),
			_isLocked(false)
		{
		}

		// deconstructor
		~VertexBuffer()
		{
			Release();
		}

		// public functions
		bool Create(uint32_t capacity, uint32_t stride, enum R_USAGE usage);
		bool Release();
		bool Lock(void **data);
		bool Unlock();
		bool Set();
		bool Draw();
		uint32_t GetCapacity() const { return _capacity; }
		uint32_t GetSize()     const { return _nVerts; }
		void SetSize(uint32_t size);

	private:
		uint32_t _capacity;    // number of vertices we can hold
		uint32_t _nVerts;      // number of vertices in the buffer
};

#endif
