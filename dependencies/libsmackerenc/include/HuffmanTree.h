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
 * Modified 2026-11-18 by Melanikus for the AvP port: derived from libsmackerdec's HuffmanVLC.h and the tree readers of its SmackerDecoder.cpp
 * (Copyright (C) 2011 Barry Duncan, based on FFmpeg's smacker.c): the trees MADE and WRITTEN, for his readers to read back.
 *
 * A tree's codes are his: the branch at depth d is bit d of the code (0 the first child, 1 the second), the order his DecodeTree gives
 * them (prefix | 1 << (length - 1) for the second child) and his GetCode walks them (a 1 bit jumps over the first child). A tree of one
 * leaf has a code 0 bits long: his VLC table for it is empty, and GetCode reads nothing for it.
 */

#ifndef _SmackerHuffmanTree_h_
#define _SmackerHuffmanTree_h_

#include <stdint.h>
#include <vector>

#include "BitWriter.h"

namespace SmackerCommon {

struct HuffTree
{
	struct Node
	{
		int32_t first;     // the child a 0 bit takes; -1 for a leaf
		int32_t second;    // the child a 1 bit takes; -1 for a leaf
		uint32_t symbol;   // a leaf's symbol
	};

	std::vector<Node> nodes;        // nodes[root] is the top
	int32_t root;
	std::vector<uint32_t> code;     // by symbol: its code (bit d is the branch at depth d)
	std::vector<uint8_t> length;    // by symbol: its code's length; 0 for a symbol not in the tree, and for the leaf of a one-leaf tree
	std::vector<uint32_t> leaves;   // the symbols of the leaves, in the order the tree is written (first child first)
};

// The tree for these counts, one per symbol (counts.size() symbols: 256 for a byte, 65536 for 16 bits); a symbol of count 0 is left out.
// No code is longer than mostLength (1 to 32). false when no symbol has a count.
bool HuffTree_Build(const std::vector<uint64_t> &counts, uint32_t mostLength, HuffTree *tree);

// A byte tree as his DecodeTree reads it: a node is a 1 bit and its two children, a leaf a 0 bit and its 8-bit symbol
void HuffTree_WriteByteTree(BitWriter &bits, const HuffTree &tree);

// A 16-bit tree as his DecodeBigTree reads it: a node is a 1 bit and its two children, a leaf a 0 bit, then the code of the symbol's low
// byte in lowBytes, then the code of its high byte in highBytes
void HuffTree_WriteBigTree(BitWriter &bits, const HuffTree &tree, const HuffTree &lowBytes, const HuffTree &highBytes);

// A symbol's code
void HuffTree_PutSymbol(BitWriter &bits, const HuffTree &tree, uint32_t symbol);

} // close namespace SmackerCommon

#endif
