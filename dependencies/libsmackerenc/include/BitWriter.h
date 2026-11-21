/*
 * libsmackerenc - Smacker video encoder
 * Copyright (C) 2011 Barry Duncan
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

/*
 * Modified 2026-11-18 by Melanikus for the AvP port: derived from libsmackerdec's BitReader.h (Copyright (C) 2011 Barry Duncan), turned round
 * into a WRITER: the bits go into each byte from its lowest bit up, in the order BitReader::GetBit takes them out, and PutBits(value, n)
 * puts value's lowest bit first, as BitReader::GetBits(n) builds its number.
 */

#ifndef _SmackerBitWriter_h_
#define _SmackerBitWriter_h_

#include <stddef.h>
#include <stdint.h>
#include <vector>

namespace SmackerCommon {

class BitWriter
{
	public:
		BitWriter();

		void PutBit(uint32_t bit);

		// n is 0 to 32
		void PutBits(uint32_t value, uint32_t n);

		size_t BitCount() const;

		// the bytes so far: the last one is filled up with 0 bits
		const std::vector<uint8_t> &Bytes() const;

	private:
		std::vector<uint8_t> bytes;
		uint32_t bitsInLast; // how many bits of the last byte are used; 0: the next bit starts a new byte
};

} // close namespace SmackerCommon

#endif
