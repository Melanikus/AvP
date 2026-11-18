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
 * Modified 2026-11-18 by Melanikus for the AvP port: derived from libsmackerdec's HuffmanVLC.cpp and the tree readers of its
 * SmackerDecoder.cpp (Copyright (C) 2011 Barry Duncan, based on FFmpeg's smacker.c): see HuffmanTree.h.
 */

#include "HuffmanTree.h"

#include <functional>
#include <queue>
#include <utility>

namespace SmackerCommon {

namespace {

// Each node's depth, by walking down from the top (no recursion: a tree is built here before its depth is known)
uint32_t DeepestLeaf(const HuffTree &tree)
{
	uint32_t deepest = 0;
	std::vector<std::pair<int32_t, uint32_t> > todo;

	todo.push_back(std::make_pair(tree.root, 0u));

	while (!todo.empty())
	{
		std::pair<int32_t, uint32_t> const at = todo.back();
		todo.pop_back();

		const HuffTree::Node &node = tree.nodes[(size_t)at.first];

		if (node.first < 0)
		{
			deepest = at.second > deepest ? at.second : deepest;
		}
		else
		{
			todo.push_back(std::make_pair(node.second, at.second + 1));
			todo.push_back(std::make_pair(node.first, at.second + 1));
		}
	}

	return deepest;
}

void WriteNode(BitWriter &bits, const HuffTree &tree, int32_t at, const HuffTree *lowBytes, const HuffTree *highBytes)
{
	const HuffTree::Node &node = tree.nodes[(size_t)at];

	if (node.first < 0)
	{
		bits.PutBit(0);

		if (lowBytes)
		{
			HuffTree_PutSymbol(bits, *lowBytes, node.symbol & 0xFF);
			HuffTree_PutSymbol(bits, *highBytes, node.symbol >> 8);
		}
		else
		{
			bits.PutBits(node.symbol, 8);
		}

		return;
	}

	bits.PutBit(1);
	WriteNode(bits, tree, node.first, lowBytes, highBytes);
	WriteNode(bits, tree, node.second, lowBytes, highBytes);
}

} // close the unnamed namespace

bool HuffTree_Build(const std::vector<uint64_t> &counts, uint32_t mostLength, HuffTree *tree)
{
	tree->nodes.clear();
	tree->leaves.clear();
	tree->code.assign(counts.size(), 0);
	tree->length.assign(counts.size(), 0);
	tree->root = -1;

	if (mostLength < 1)
		mostLength = 1;
	if (mostLength > 32)
		mostLength = 32;

	std::vector<uint32_t> symbols;
	std::vector<uint64_t> weights;

	for (size_t i = 0; i < counts.size(); i++)
	{
		if (counts[i])
		{
			symbols.push_back((uint32_t)i);
			weights.push_back(counts[i]);
		}
	}

	// no codes of mostLength bits can tell that many symbols apart
	if (symbols.empty() || (uint64_t)symbols.size() > ((uint64_t)1 << mostLength))
	{
		return false;
	}

	// Huffman's way, taking the two lightest each time (ties: the older node first, so the same counts always give the same tree). A tree too
	// deep is made again from the counts halved (none below 1), which flattens it; counts all 1 give a tree as flat as it can be, which fits.
	for (;;)
	{
		typedef std::pair<uint64_t, int32_t> Weighed;
		std::priority_queue<Weighed, std::vector<Weighed>, std::greater<Weighed> > lightest;

		tree->nodes.clear();

		for (size_t i = 0; i < symbols.size(); i++)
		{
			HuffTree::Node leaf = { -1, -1, symbols[i] };

			tree->nodes.push_back(leaf);
			lightest.push(std::make_pair(weights[i], (int32_t)i));
		}

		while (lightest.size() > 1)
		{
			Weighed const a = lightest.top();
			lightest.pop();
			Weighed const b = lightest.top();
			lightest.pop();

			HuffTree::Node node = { a.second, b.second, 0 };

			tree->nodes.push_back(node);
			lightest.push(std::make_pair(a.first + b.first, (int32_t)tree->nodes.size() - 1));
		}

		tree->root = lightest.top().second;

		if (DeepestLeaf(*tree) <= mostLength)
		{
			break;
		}

		for (uint64_t &w : weights)
		{
			w = (w >> 1) | 1;
		}
	}

	// the codes, and the leaves in the order they are written (first child first)
	struct At
	{
		int32_t node;
		uint32_t code;
		uint32_t depth;
	};

	std::vector<At> todo;
	At const top = { tree->root, 0, 0 };

	todo.push_back(top);

	while (!todo.empty())
	{
		At const at = todo.back();
		todo.pop_back();

		const HuffTree::Node &node = tree->nodes[(size_t)at.node];

		if (node.first < 0)
		{
			tree->code[node.symbol] = at.code;
			tree->length[node.symbol] = (uint8_t)at.depth;
			tree->leaves.push_back(node.symbol);
		}
		else
		{
			At const second = { node.second, at.code | (1u << at.depth), at.depth + 1 };
			At const first = { node.first, at.code, at.depth + 1 };

			todo.push_back(second);
			todo.push_back(first);
		}
	}

	return true;
}

void HuffTree_WriteByteTree(BitWriter &bits, const HuffTree &tree)
{
	WriteNode(bits, tree, tree.root, 0, 0);
}

void HuffTree_WriteBigTree(BitWriter &bits, const HuffTree &tree, const HuffTree &lowBytes, const HuffTree &highBytes)
{
	WriteNode(bits, tree, tree.root, &lowBytes, &highBytes);
}

void HuffTree_PutSymbol(BitWriter &bits, const HuffTree &tree, uint32_t symbol)
{
	if (symbol < tree.code.size())
	{
		bits.PutBits(tree.code[symbol], tree.length[symbol]);
	}
}

} // close namespace SmackerCommon
