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
 * Modified 2026-11-18 by Melanikus for the AvP port: derived from libsmackerdec's BitReader.cpp (Copyright (C) 2011 Barry Duncan), turned
 * round into a writer (see BitWriter.h).
 */

#include "BitWriter.h"

namespace SmackerCommon {

BitWriter::BitWriter()
{
	this->bitsInLast = 0;
}

void BitWriter::PutBit(uint32_t bit)
{
	if (bitsInLast == 0)
	{
		bytes.push_back(0);
	}

	bytes.back() |= (uint8_t)((bit & 1) << bitsInLast);
	bitsInLast = (bitsInLast + 1) & 7;
}

void BitWriter::PutBits(uint32_t value, uint32_t n)
{
	for (uint32_t i = 0; i < n && i < 32; i++)
	{
		PutBit((value >> i) & 1);
	}
}

size_t BitWriter::BitCount() const
{
	return bytes.size() * 8 - (bitsInLast ? 8 - bitsInLast : 0);
}

const std::vector<uint8_t> &BitWriter::Bytes() const
{
	return bytes;
}

} // close namespace SmackerCommon
