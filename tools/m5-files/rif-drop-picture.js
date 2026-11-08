// Written 2026-11-07 by Melanikus for the AvP port.
//
// Takes one picture out of a model file (a .rif): the picture's name comes off the file's list of pictures, and every polygon
// painted with it is taken out of every shape. Everything else stays byte for byte as it was, apart from the numbers that
// count polygons or point at one by its place, which are counted again so they still point at the same polygons.
//
// Why: the MelPyth mod's dropped-gun model (avp_huds\ShotgunPickup.rif) paints 6 polygons with common objects\girl.rim, which
// nobody has. The user's ruling, 2026-11-07: it was a piece of a picture used by accident, so remove it from the file. Without
// it, the game would paint those polygons bright pink, its colour for a missing picture. The story is in the project's notes.
//
// How to run it, with node, from the folder that holds the file it reads (see the names' check below). The new file goes in that
// folder's "out" folder, which it makes if it isn't there. The tool itself can be anywhere:
//   node <the repository>/tools/m5-files/rif-drop-picture.js <the model file's name> <the new file's name> <picture name>
// For example:
//   node <the repository>/tools/m5-files/rif-drop-picture.js ShotgunPickup ShotgunPickup "common objects\girl.rim"
//   reads ShotgunPickup.rif and writes out\ShotgunPickup.rif
//
// It only reads the model file and never changes it. It won't write over a file that's already there. It stops, writing
// nothing, if anything isn't as it expects: then nobody has to guess what it did.
//
// What it changes, in each shape (REBSHAPE) and in each of a shape's pieces (SUBSHAPE, the bits it breaks into):
//   the heading (SHPHEAD1, or SUBSHPHD for a piece)  its count of polygons
//   SHPPOLYS  the polygons, 36 bytes each       the picture's are taken out. Each says which normal is its own, by place,
//                                               and that place is counted again
//   SHPPNORM  one normal for each polygon       the taken polygons' normals go with them
//   SHPMRGDT  for each polygon, the place of the one it joins with to make a four-sided one, or -1. The places are counted
//             again. If a taken polygon is joined with one that stays, this stops, because that one would change too
//   SHPPCINF  a list of changes, each naming a polygon by its place      counted again; one naming a taken polygon stops this
// And in the picture lists (BMPLSTST, BMPNAMES): the picture's entry comes out. Each entry carries its own number, which the
// polygons use, so the others keep theirs (Chunk_With_BMPs in 3dc/win95/bitmap_names.cpp).
// A shape holding any other kind of chunk stops this, in case that one counts polygons too.

'use strict';

const fs = require('fs');
const path = require('path');
const crypto = require('crypto');
const { stop, setToolName, chunksIn, textIn, chunkHeading, readModelFile } = require('./rif-file.js');

setToolName('rif-drop-picture');

// Each file is named, not given as a path: letters, digits and _ only, and the tool adds its ending. The file it reads is in the
// folder it's run from, and the new file goes in that folder's "out" folder. That's the same check tests/takeouts/find-moved-row.js
// makes on the name of the list it's given, so no name can reach a file anywhere else. Windows' device names (CON, NUL, COM1 and
// so on) are refused too, because Windows takes CON.rif for the screen, in any folder.
const kDeviceName = /^(con|prn|aux|nul|com[0-9]|lpt[0-9])$/i;
const here = path.resolve(process.cwd());

const [fromName, toName, pictureName] = process.argv.slice(2);
if (!fromName || !toName || !pictureName) stop('give it the model file, the new file and the picture\'s name');
// Both names are checked before any file is touched
if (!/^[A-Za-z0-9_]+$/.test(fromName) || kDeviceName.test(fromName)) {
  stop(`the model file is named with letters, digits and _ only, without .rif, and not as a device: "${fromName}" isn't`);
}
if (!/^[A-Za-z0-9_]+$/.test(toName) || kDeviceName.test(toName)) {
  stop(`the new file is named with letters, digits and _ only, without .rif, and not as a device: "${toName}" isn't`);
}
const from = path.join(here, `${fromName}.rif`);
const outFolder = path.join(here, 'out');
const to = path.join(outFolder, `${toName}.rif`);
if (fs.existsSync(to)) stop(`${to} is already there, and this never writes over a file`);
const { data, top } = readModelFile(from);

// The chunks that hold other chunks, which this opens. Every other chunk is kept as its bytes, unless it's one this changes.
const kHolders = new Set(['REBENVDT', 'REBSHAPE', 'SHPEXTFL', 'SHPFRAGS', 'SUBSHAPE', 'FRAGTYPE']);
function tree(list, where) {
  return list.map((c) => ({ name: c.name, c, kids: kHolders.has(c.name) ? tree(chunksIn(data, c.body, c.end, `${where} > ${c.name}`), `${where} > ${c.name}`) : null }));
}
const roots = tree(top, 'the file');

// ---- The picture lists: find the picture's number, and take its entry out ----
// A list is a count (the low 16 bits; the high 16 are a version, kept), then for each picture five numbers (flags, its own
// number, then three more), and its name with zeros after it up to a whole number of 4 bytes, at least one zero.
let pictureNumber = null;
let entriesTaken = 0;
function listWithout(node) {
  const c = node.c;
  if (c.end - c.body < 4) stop(`a picture list too small for its count`);
  const head = data.readInt32LE(c.body);
  const count = head & 0xffff;
  let at = c.body + 4;
  const kept = [];
  for (let i = 0; i < count; i++) {
    if (c.end - at < 21) stop('a picture list ends inside an entry');
    const number = data.readInt32LE(at + 4);
    const name = textIn(data, at + 20, c.end, 'a picture\'s name');
    const size = 20 + name.length + (4 - (name.length % 4));
    if (size > c.end - at) stop(`the picture ${name} runs past its list`);
    if (name.toLowerCase() === pictureName.toLowerCase()) {
      if (pictureNumber !== null && pictureNumber !== number) stop(`two lists give ${name} different numbers`);
      pictureNumber = number;
      entriesTaken++;
    } else {
      kept.push(data.subarray(at, at + size));
    }
    at += size;
  }
  // Anything after the entries is kept too, as the game's reader ignores it
  const after = data.subarray(at, c.end);
  const newHead = Buffer.alloc(4);
  newHead.writeInt32LE((head & ~0xffff) | kept.length);
  node.body = Buffer.concat([newHead, ...kept, after]);
}
(function findLists(nodes) {
  for (const n of nodes) {
    if (n.name === 'BMPLSTST' || n.name === 'BMPNAMES') listWithout(n);
    if (n.kids) findLists(n.kids);
  }
})(roots);
if (pictureNumber === null) stop(`no picture list names ${pictureName}`);

// ---- The shapes and their pieces ----
const kShapeKinds = new Set(['SHPHEAD1', 'SUBSHPHD', 'SHPCENTR', 'SHPEXTFL', 'SHPRAWVT', 'SHPVNORM', 'SHPPNORM', 'SHPPOLYS', 'SHPUVCRD',
  'SHPMRGDT', 'SHPPCINF', 'SHPFRAGS', 'SHPFNAME', 'FRAGDATA']);
const report = [];
function dropFromShape(node, label) {
  const kid = (name) => {
    const found = node.kids.filter((k) => k.name === name);
    if (found.length > 1) stop(`${label} has two ${name} chunks`);
    return found[0] || null;
  };
  for (const k of node.kids) if (!kShapeKinds.has(k.name)) stop(`${label} holds a ${k.name}, which this doesn't know, so it can't tell whether that counts polygons`);

  // The polygon count: 28 bytes into a shape's heading, 12 into a piece's (Shape_Header_Chunk, Shape_Sub_Shape_Header_Chunk)
  const heading = kid(node.name === 'REBSHAPE' ? 'SHPHEAD1' : 'SUBSHPHD');
  const countAt = node.name === 'REBSHAPE' ? 28 : 12;
  if (!heading || heading.c.end - heading.c.body < countAt + 4) stop(`${label} has no heading`);
  const n = data.readInt32LE(heading.c.body + countAt);
  const polys = kid('SHPPOLYS'), normals = kid('SHPPNORM'), joins = kid('SHPMRGDT'), changes = kid('SHPPCINF');
  if (!polys || polys.c.end - polys.c.body !== 36 * n) stop(`${label}: its polygons don't match its count of ${n}`);
  if (normals && normals.c.end - normals.c.body !== 12 * n) stop(`${label}: its normals don't match its count of polygons`);
  if (joins && joins.c.end - joins.c.body !== 4 * n) stop(`${label}: its joins don't match its count of polygons`);

  // Which polygons go: the low 12 bits of the fourth number are the picture (the loader's read_polygon)
  const goes = [];
  for (let i = 0; i < n; i++) goes.push((data.readUInt32LE(polys.c.body + 36 * i + 12) & 0xfff) === pictureNumber);
  const place = []; let next = 0;
  for (let i = 0; i < n; i++) place.push(goes[i] ? -1 : next++);
  const taken = n - next;
  if (taken === 0) { report.push(`${label}: none of its ${n} polygons use it`); return; }

  // The polygons, each pointing at its own normal by place. In the game's files that place is the polygon's own; if it
  // isn't, this stops rather than guess what another number means.
  const keptPolys = [];
  for (let i = 0; i < n; i++) {
    const p = data.subarray(polys.c.body + 36 * i, polys.c.body + 36 * i + 36);
    if (p.readInt32LE(4) !== i) stop(`${label}: polygon ${i} names normal ${p.readInt32LE(4)}, not its own`);
    if (goes[i]) continue;
    const copy = Buffer.from(p);
    copy.writeInt32LE(place[i], 4);
    keptPolys.push(copy);
  }
  polys.body = Buffer.concat(keptPolys);
  if (normals) {
    const kept = [];
    for (let i = 0; i < n; i++) if (!goes[i]) kept.push(data.subarray(normals.c.body + 12 * i, normals.c.body + 12 * i + 12));
    normals.body = Buffer.concat(kept);
  }
  if (joins) {
    const out = [];
    for (let i = 0; i < n; i++) {
      const partner = data.readInt32LE(joins.c.body + 4 * i);
      if (partner !== -1 && (partner < 0 || partner >= n)) stop(`${label}: polygon ${i} joins one that isn't there`);
      if (partner !== -1 && goes[i] !== goes[partner]) stop(`${label}: polygon ${i} joins polygon ${partner}, and only one of them uses the picture`);
      if (goes[i]) continue;
      const b = Buffer.alloc(4); b.writeInt32LE(partner === -1 ? -1 : place[partner]); out.push(b);
    }
    joins.body = Buffer.concat(out);
  }
  if (changes) {
    // A number kept as it is (the points the shape had first), then a count, then for each change 3 numbers: the polygon's place
    // and two more (Shape_Poly_Change_Info_Chunk)
    const c = changes.c;
    if (c.end - c.body < 8) stop(`${label}: its list of changes is too small`);
    const entries = data.readInt32LE(c.body + 4);
    if (entries < 0 || c.end - c.body !== 8 + 12 * entries) stop(`${label}: its list of changes doesn't match its count`);
    const out = [];
    for (let e = 0; e < entries; e++) {
      const at = c.body + 8 + 12 * e;
      const poly = data.readInt32LE(at);
      if (poly < 0 || poly >= n) stop(`${label}: a change names polygon ${poly}, which isn't there`);
      if (goes[poly]) stop(`${label}: a change names polygon ${poly}, which uses the picture`);
      const copy = Buffer.from(data.subarray(at, at + 12)); copy.writeInt32LE(place[poly], 0); out.push(copy);
    }
    const first = Buffer.from(data.subarray(c.body, c.body + 8)); first.writeInt32LE(out.length, 4);
    changes.body = Buffer.concat([first, ...out]);
  }
  const head = Buffer.from(data.subarray(heading.c.body, heading.c.end));
  head.writeInt32LE(next, countAt);
  heading.body = head;
  report.push(`${label}: ${taken} of its ${n} polygons used it and were taken out`);
}
let shapeNumber = 0;
(function findShapes(nodes, parentLabel) {
  for (const node of nodes) {
    if (node.name === 'REBSHAPE' || node.name === 'SUBSHAPE') {
      const name = node.kids.find((k) => k.name === 'SHPFNAME');
      const label = node.name === 'REBSHAPE' ? `shape ${shapeNumber++}` : `${parentLabel}, piece ${name ? textIn(data, name.c.body, name.c.end, 'a piece\'s name') : '?'}`;
      dropFromShape(node, label);
      findShapes(node.kids, label);
    } else if (node.kids) {
      findShapes(node.kids, parentLabel);
    }
  }
})(roots, '');

// ---- Writing ----
function bytesOf(node) {
  if (node.kids) {
    const body = Buffer.concat(node.kids.map(bytesOf));
    return Buffer.concat([chunkHeading(node.name, body.length), body]);
  }
  if (node.body) return Buffer.concat([chunkHeading(node.name, node.body.length), node.body]);
  return data.subarray(node.c.at, node.c.end);
}
const body = Buffer.concat(roots.map(bytesOf));
const file = Buffer.concat([chunkHeading('REBINFF2', body.length), body]);

// Read back what's about to be written, so a mistake here can't reach the disk: it has to be chunks all the way down, and
// no polygon or list anywhere may still use the picture
const back = chunksIn(file, 0, file.length, 'the new file');
if (back.length !== 1) stop('the new file didn\'t read back as one chunk');
if (file.toString('latin1').toLowerCase().includes(pictureName.toLowerCase())) stop('the new file still names the picture');
(function check(list, depth) {
  for (const c of list) {
    if (c.name === 'SHPPOLYS') for (let i = c.body; i + 36 <= c.end; i += 36) if ((file.readUInt32LE(i + 12) & 0xfff) === pictureNumber) stop('a polygon in the new file still uses the picture');
    if (kHolders.has(c.name) || c.name === 'REBINFF2') check(chunksIn(file, c.body, c.end, 'the new file'), depth + 1);
  }
})(back, 0);

// 'wx' makes the write itself refuse a file that appeared since the check above
if (!fs.existsSync(outFolder)) fs.mkdirSync(outFolder);
fs.writeFileSync(to, file, { flag: 'wx' });

const sha = crypto.createHash('sha256').update(file).digest('hex');
console.log(`${pictureName} was picture number ${pictureNumber}, in ${entriesTaken} list${entriesTaken === 1 ? '' : 's'}.`);
for (const line of report) console.log('  ' + line);
console.log(`${to}: ${file.length} bytes, sha256 ${sha}`);
