// Written 2026-11-07 by Melanikus for the AvP port.
// Modified 2026-11-10 by Melanikus for the AvP port:
//   chunksIn and readModelFile can take a chunk with any name, as the game does, for tools/levels/lift-table.js (#189)
//
// Reading and writing the game's model files (.rif), shared by rif-pick.js and rif-drop-picture.js. It only reads files it's
// given and makes new bytes; the tools that use it decide what to write, and where.
//
// How a model file is laid out (read from the game's own readers in 3dc/win95, such as mishchnk.cpp and hierarchy_chunk.cpp):
//   The file is a row of "chunks". Each chunk starts with 8 letters saying what it is, then a 4-byte size that counts those
//   12 bytes too, then its contents. Some chunks hold a row of other chunks inside them.
//   The whole file is one chunk, REBINFF2, holding everything else. The game's own files are usually packed small first
//   (they start REBCRIF1 instead), and this unpacks those. The tools always write the unpacked kind: the game reads both
//   (File_Chunk in 3dc/win95/mishchnk.cpp).

'use strict';

const fs = require('fs');

// Far bigger than any model file the game has. A file bigger than this isn't one, so it isn't read at all.
const kLargestFile = 64 * 1024 * 1024;

// Every problem goes through here: it says what was wrong, says nothing was written, and stops the program.
let toolName = 'rif';
function stop(why) {
  console.error(toolName + ': ' + why + '. Nothing was written.');
  process.exit(1);
}
function setToolName(name) {
  toolName = name;
}

// ---- Unpacking a packed file ----
// A packed file starts with a heading: the letters REBCRIF1, then 4 bytes we don't need, the unpacked size, how many codes
// there are of each length (11 lengths), and 256 bytes saying which byte value each code stands for. The packed bits follow.
// This is the game's own way of unpacking (3dc/win95/huffman.cpp), with every place it reads or writes checked first.
const kMostCodeLength = 11;
const kHeadingSize = 8 + 4 + 4 + 4 * kMostCodeLength + 256;

function makeCodeTable(lengths, values) {
  const table = new Int32Array(1 << kMostCodeLength);
  let codeBits = 0, copies = 1 << kMostCodeLength, step = 1, codesLeft = 0, lengthBit = 4, valueAt = 255, place = 0, lengthAt = 0;
  for (;;) {
    do {
      codeBits++; lengthBit <<= 1; step <<= 1; copies >>= 1;
      if (lengthAt >= kMostCodeLength) stop('the packed file\'s code lengths run past their table');
      codesLeft = lengths[lengthAt++];
    } while (!codesLeft);
    do {
      let entry = valueAt < 0 ? 0xff : (codeBits | (values[valueAt--] << 8));
      let at = place >> 2;
      let count = copies;
      do {
        if (at >= table.length) stop('the packed file\'s codes run past their table');
        table[at] = entry; at += step;
      } while (--count);
      entry = lengthBit;
      do {
        entry >>= 1;
        if (entry & 3) return table;
        place ^= entry;
      } while (!(place & entry));
    } while (--codesLeft);
  }
}

function unpack(file) {
  if (file.length < kHeadingSize) stop('the packed file is smaller than its own heading');
  const size = file.readInt32LE(12);
  const packedBytes = file.length - kHeadingSize;
  // Each byte of the file can't stand for more than 8 bytes once unpacked
  if (size < 12 || size > packedBytes * 8) stop('the packed file says it unpacks to a size it can\'t');
  const lengths = [];
  for (let i = 0; i < kMostCodeLength; i++) lengths.push(file.readInt32LE(16 + 4 * i));
  const table = makeCodeTable(lengths, file.subarray(16 + 4 * kMostCodeLength, kHeadingSize));
  const words = Math.floor(packedBytes / 4);
  const out = Buffer.alloc(size);
  let made = 0, inHand = 0, spare = 0, width = 0, bits = 0, spareBits = 0, wordAt = 0, value = 0;
  do {
    inHand += width;
    let fill = 31 - inHand;
    bits = (bits << fill) >>> 0;
    if (fill > spare) {
      fill -= spare;
      inHand += spare;
      if (spare) bits = ((bits >>> spare) | (spareBits << (32 - spare))) >>> 0;
      spareBits = wordAt < words ? file.readUInt32LE(kHeadingSize + 4 * wordAt++) : 0;
      spare = 32;
    }
    bits = ((bits >>> fill) | (spareBits << (32 - fill))) >>> 0;
    spareBits = spareBits >>> fill;
    spare -= fill;
    inHand = 31;
    let first = true;
    do {
      if (!first) { bits = bits >>> width; out[made++] = value; }
      first = false;
      const entry = table[(bits >>> 1) & 0x7ff];
      width = entry & 0xff; value = (entry >> 8) & 0xff;
    } while ((inHand -= width) >= 0 && made !== size);
  } while (inHand > -32 && made !== size);
  if (made !== size) stop('the packed file ran out before it was all unpacked');
  return out;
}

// ---- Reading chunks ----
// A chunk's 8 letters are capitals, digits, _ or a space. A name shorter than 8 is filled out with zeros (OBINTDT is), so a
// zero is allowed too, but only after the name's own letters.
function isChunkName(bytes, at) {
  let ended = false;
  for (let i = 0; i < 8; i++) {
    const c = bytes[at + i];
    if (c === 0 && i > 0) { ended = true; continue; }
    const ok = (c >= 48 && c <= 57) || (c >= 65 && c <= 90) || (c >= 97 && c <= 122) || c === 95 || c === 32;
    if (!ok || ended) return false;
  }
  return true;
}

// The chunks in a row, from start to end. The row has to fill that space exactly, with no bytes over.
// anyName lets a chunk have any 8 bytes for its name, as the game itself does: it keeps a chunk with a name it doesn't know as
// plain bytes (Miscellaneous_Chunk, Chunk_With_Children::DynCreate in 3dc/win95/chunk.cpp). The game's own Sulaco.RIF has one
// named "(" and seven zeros, between two of its things. Only its size is checked then. Left out, the name has to look like one.
function chunksIn(bytes, start, end, where, anyName = false) {
  const list = [];
  let at = start;
  while (at < end) {
    if (end - at < 12) stop(`${where}: ${end - at} bytes are left over at the end`);
    if (!anyName && !isChunkName(bytes, at)) stop(`${where}: something that isn't a chunk, at byte ${at}`);
    const size = bytes.readInt32LE(at + 8);
    if (size < 12 || size > end - at) stop(`${where}: ${bytes.toString('latin1', at, at + 8)} at byte ${at} has a size that doesn't fit`);
    list.push({ name: bytes.toString('latin1', at, at + 8), at, size, body: at + 12, end: at + size });
    at += size;
  }
  return list;
}

// Text that ends with a zero, inside a chunk. It has to end before the chunk does.
function textIn(bytes, at, end, what) {
  let e = at;
  while (e < end && bytes[e] !== 0) e++;
  if (e >= end) stop(`${what} never ends`);
  return bytes.toString('latin1', at, e);
}

// The 12 bytes that start a chunk: its letters, and its size counting these 12
function chunkHeading(name, bodySize) {
  const h = Buffer.alloc(12);
  h.write(name, 0, 8, 'latin1');
  h.writeInt32LE(12 + bodySize, 8);
  return h;
}

// A whole model file, unpacked if it was packed. Gives back the bytes and the chunks inside its one REBINFF2 chunk. anyName is
// chunksIn's, for the chunks inside it: a tool that only reads, such as tools/levels/lift-table.js, takes what the game takes.
function readModelFile(from, anyName = false) {
  if (fs.statSync(from).size > kLargestFile) stop(`${from} is far bigger than any model file`);
  const raw = fs.readFileSync(from);
  let data;
  if (raw.length >= 8 && raw.toString('latin1', 0, 8) === 'REBCRIF1') data = unpack(raw);
  else if (raw.length >= 8 && raw.toString('latin1', 0, 8) === 'REBINFF2') data = raw;
  else stop(`${from} isn't a model file`);

  const whole = chunksIn(data, 0, data.length, from);
  if (whole.length !== 1 || whole[0].name !== 'REBINFF2') stop(`${from} isn't one REBINFF2 chunk holding the rest`);
  return { data, top: chunksIn(data, whole[0].body, whole[0].end, 'the file', anyName) };
}

module.exports = { stop, setToolName, chunksIn, textIn, chunkHeading, readModelFile };
