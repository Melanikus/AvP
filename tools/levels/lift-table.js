// Written 2026-11-10 by Melanikus for the AvP port.
//
// Prints every platform lift in some of the game's level files, one line each, in the form of the table of stock lifts in
// tests/net_checks_test.cpp (kStockLifts). That test checks the rule for calling a lift in a network game against every real
// lift (#189), and this is how anyone can make the table again from their own copy of the game. It only reads the files it's
// given and prints. It writes nothing.
//
// How to run it, with node, from the game's avp_rifs folder (the files are named, not given as paths: see the names' check below).
// The tool itself can be anywhere:
//   node <the repository>/tools/levels/lift-table.js <level> [<level> ...]
// For example:
//   node <the repository>/tools/levels/lift-table.js derelict derelict_a
//   reads derelict.rif and derelict_a.rif, and prints one line for each of their lifts
// The table in the test holds the 23 single-player levels that have lifts, in alphabetical order.
//
// Each line is { "level", "name", top, bottom, minX, maxX, minZ, maxZ, radius }, in the game's mm: the file's numbers times the
// level's scale, cut to whole numbers as the game cuts them. A bigger y is lower down, so the top is the smaller number.
//   top, bottom: the two ends of the car's travel. The car starts where the level puts it, which is the top when the track goes
//     down and the bottom when it goes up (InitialisePlatformLift, 3dc/avp/bh_platform_lift.cpp).
//   minX, maxX, minZ, maxZ: how far the car's shape reaches from its middle, seen from above.
//   radius: the size of the car's whole shape, the rule the network game used to call a lift by until #189.
//
// Where each number is in a level file (the game's own readers, in 3dc/win95 and 3dc/avp/win95; rif-file.js explains chunks):
//   RBOBJECT   one thing in the level. Inside it:
//     OBJHEAD1   its header (Object_Header_Chunk, chunk_object.cpp): flags (4 bytes), a lock name (16), its place as three
//                4-byte whole numbers x, y, z at +20, a turn (16), its number, a version, its shape's number at +56, and its
//                name from +60, ending with a zero
//     OBJTRAK2   its track (Object_Track_Chunk2, chunk_object.cpp): how many sections (4 bytes), then each section
//                (ChunkTrackSection, chunk_object.hpp): two turns of 16 bytes, then where it starts at +32 and where it ends at
//                +44, each x, y, z. A lift has one section, and its travel is the end's y take away the start's y
//                (add_platlift, object_setup.cpp)
//     OBJPRJDT   its game data. Inside it, AVPSTRAT is its strategy: the kind of thing it is, at +8. A platform lift is 54
//                (PlatLiftStrategy, AVP_Strategy_Chunk)
//   REBSHAPE   one shape. Inside it, SHPHEAD1 (Shape_Header_Chunk, chunk_shape.cpp): flags (4), a lock name (16), its number
//              at +20, its corners and polygons, its radius as a 4-byte decimal number at +32, then four-byte whole numbers:
//              the furthest x at +36, the nearest x at +40, y at +44 and +48, z at +52 and +56
//   REBENVDT   the level's settings. Inside it, ENVSDSCL is the level's scale, an 8-byte decimal number. 1 when it's not there.

'use strict';

const fs = require('fs');
const path = require('path');
const { stop, setToolName, chunksIn, textIn, readModelFile } = require('../m5-files/rif-file.js');

setToolName('lift-table');

// Each file is named, not given as a path: letters, digits and _ only, and the tool adds its ending. It's read from the folder the
// tool is run from, the same check rif-pick.js makes, so no name can reach a file anywhere else. Windows' device names (CON, NUL,
// COM1 and so on) are refused too, because Windows takes CON.rif for the screen, in any folder.
const kDeviceName = /^(con|prn|aux|nul|com[0-9]|lpt[0-9])$/i;
const here = path.resolve(process.cwd());
const levels = process.argv.slice(2);

if (levels.length === 0) stop('give it at least one level, by name, without .rif');
for (const level of levels) {
  if (!/^[A-Za-z0-9_]+$/.test(level) || kDeviceName.test(level)) {
    stop(`a level is named with letters, digits and _ only, without .rif, and not as a device: "${level}" isn't`);
  }
}

// A level's file, taken from this folder's own list of files, never made from the name it was given: the name is only compared with
// what's here. So what's read is always a plain file of this folder (not a link, a folder or anything elsewhere), whatever the name
// says. The game's own files are named in any case (Derelict.RIF), so the comparison ignores case, and two files that differ only in
// case are refused rather than guessed between.
function levelFile(level) {
  if (!/^[A-Za-z0-9_]+$/.test(level) || kDeviceName.test(level)) {
    stop(`a level is named with letters, digits and _ only, without .rif, and not as a device: "${level}" isn't`);
  }
  const wanted = `${level}.rif`.toLowerCase();
  const found = fs.readdirSync(here, { withFileTypes: true }).filter((entry) => entry.isFile() && entry.name.toLowerCase() === wanted);
  if (found.length !== 1) stop(found.length ? `more than one file here is named ${level}.rif` : `there's no plain file ${level}.rif in ${here}`);
  return path.join(here, path.basename(found[0].name));
}

// A 4-byte whole number inside a chunk, which has to be wholly inside it
function wholeAt(data, chunk, offset, what) {
  if (offset + 4 > chunk.end - chunk.body) stop(`${what}: ${chunk.name} is too short to hold it`);
  return data.readInt32LE(chunk.body + offset);
}

for (const level of levels) {
  const from = levelFile(level);
  // true: a chunk may have any name, as the game allows (see chunksIn in rif-file.js)
  const { data, top } = readModelFile(from, true);
  const shapes = new Map();
  let scale = 1;

  for (const chunk of top) {
    if (chunk.name === 'REBSHAPE') {
      for (const part of chunksIn(data, chunk.body, chunk.end, `${level} shape`, true)) {
        if (part.name !== 'SHPHEAD1') continue;
        if (part.end - part.body < 60) stop(`${level}: a shape's header is too short`);
        shapes.set(data.readInt32LE(part.body + 20), {
          radius: data.readFloatLE(part.body + 32),
          maxX: data.readInt32LE(part.body + 36),
          minX: data.readInt32LE(part.body + 40),
          maxZ: data.readInt32LE(part.body + 52),
          minZ: data.readInt32LE(part.body + 56),
        });
      }
    }
    if (chunk.name === 'REBENVDT') {
      for (const part of chunksIn(data, chunk.body, chunk.end, `${level} settings`, true)) {
        if (part.name !== 'ENVSDSCL') continue;
        if (part.end - part.body < 8) stop(`${level}: the scale is too short`);
        scale = data.readDoubleLE(part.body);
      }
    }
  }
  if (!(scale > 0 && scale < 1000)) stop(`${level}: a scale of ${scale} isn't a level's`);

  // The game cuts each number to a whole one after scaling it ((int) in add_platlift and in chunk_load.cpp)
  const inMm = (value) => Math.trunc(value * scale);

  for (const chunk of top) {
    if (chunk.name !== 'RBOBJECT') continue;
    let name = null, y = 0, shapeNumber = -1, kind = -1, sections = 0, travel = 0;
    for (const part of chunksIn(data, chunk.body, chunk.end, `${level} thing`, true)) {
      if (part.name === 'OBJHEAD1') {
        y = wholeAt(data, part, 24, `${level} thing`);
        shapeNumber = wholeAt(data, part, 56, `${level} thing`);
        name = textIn(data, part.body + 60, part.end, `${level}: a thing's name`);
      }
      if (part.name === 'OBJTRAK2') {
        sections = wholeAt(data, part, 0, `${level} track`);
        if (sections === 1) travel = wholeAt(data, part, 4 + 44 + 4, `${level} track`) - wholeAt(data, part, 4 + 32 + 4, `${level} track`);
      }
      if (part.name === 'OBJPRJDT') {
        for (const inner of chunksIn(data, part.body, part.end, `${level} game data`, true)) {
          if (inner.name === 'AVPSTRAT') kind = wholeAt(data, inner, 8, `${level} strategy`);
        }
      }
    }
    if (kind !== 54) continue;
    if (name === null) stop(`${level}: a platform lift has no header`);

    // Where the car starts, and its two ends, as InitialisePlatformLift works them out
    const home = inMm(y);
    const move = sections === 1 ? inMm(travel) : 0;
    const topY = move < 0 ? home + move : home;
    const bottomY = move < 0 ? home : home + move;
    const shape = shapes.get(shapeNumber);
    if (!shape) stop(`${level}: the lift "${name}" has no shape`);

    // Only plain letters in the printed name, so the line is always a good C++ string
    const printable = name.replace(/[^ -~]|["\\]/g, '?');
    const note = move > 0 ? '' : move < 0 ? ' // starts at the bottom' : ' // has no travel';
    console.log(`\t{ "${level.toLowerCase()}", "${printable}", ${topY}, ${bottomY}, ${inMm(shape.minX)}, ${inMm(shape.maxX)}, ` +
      `${inMm(shape.minZ)}, ${inMm(shape.maxZ)}, ${inMm(shape.radius)} },${note}`);
  }
}
