// Written 2026-11-07 by Melanikus for the AvP port.
//
// Makes a small model file (a .rif) out of a bigger one. It copies only the models it's told to (each model is a
// "hierarchy": a tree of moving parts, such as a gun and the hand holding it), the parts those models use, and the shapes of
// those parts. Everything it copies stays byte for byte as it was. The new file gets its own internal name, so the game
// can tell it apart from the file it came from.
//
// Why: the MelPyth mod changed the game's own marwep.rif in place, to add the pulse rifle's ammo counter and a new right-hand
// pistol. We don't change the player's marwep.rif. Instead, the changed models go into a file of our own
// (avp_huds\MelPyth_MarWep.rif), and the game draws those guns from it when it's there. The player's marwep.rif stays as it is.
//
// How to run it, with node, from the folder that holds the file it reads (see the names' check below). The new file goes in that
// folder's "out" folder, which it makes if it isn't there. The tool itself can be anywhere:
//   node <the repository>/tools/m5-files/rif-pick.js <the bigger file's name> <the new file's name> <the new internal name> <model name> [<model name> ...]
// For example:
//   node <the repository>/tools/m5-files/rif-pick.js marwep MelPyth_MarWep MelPyth_MarWep "Pulse Rifle" Cudgel Pistol "Two pistol"
//   reads marwep.rif and writes out\MelPyth_MarWep.rif
//
// It only reads the bigger file and never changes it. It won't write over a file that's already there. It stops, writing
// nothing, if a model isn't found, if anything in the file isn't laid out as expected, or if the file holds a kind of piece it
// doesn't know. Then nobody has to guess what it did.
//
// How a model file is laid out: see rif-file.js, which this uses to read one. It always writes the unpacked kind.
//   The pieces inside it, and what this does with each:
//     RIFVERIN  the file's version             kept as it is
//     REBENVDT  the file's settings: its internal name (RIFFNAME), its list of pictures, its palette and its lights.
//               Kept as it is, except the internal name. Each polygon names its picture by a number, and the list of
//               pictures says which number each one has, so the list stays whole and every number keeps its picture.
//     OBJCHIER  one model, named by the OBHIERNM inside it         kept if it's one of the models asked for
//     RBOBJECT  one part, named in its OBJHEAD1                    kept if a kept model uses it
//     REBSHAPE  one part's shape, numbered in its SHPHEAD1         kept if a kept part uses it
//     INDSOUND  one sound the models' moves can play                all kept: the moves name them by number
//     OBHALTSH  another look for some parts (the grenade launcher's grenades, in marwep.rif). Each one names the parts it
//               changes. Dropped, but if one changes a part that's kept, this stops, because then the look would be lost.

'use strict';

const fs = require('fs');
const path = require('path');
const crypto = require('crypto');
const { stop, setToolName, chunksIn, textIn, chunkHeading, readModelFile } = require('./rif-file.js');

setToolName('rif-pick');

// Each file is named, not given as a path: letters, digits and _ only, and the tool adds its ending. The file it reads is in the
// folder it's run from, and the new file goes in that folder's "out" folder. That's the same check tests/takeouts/find-moved-row.js
// makes on the name of the list it's given, so no name can reach a file anywhere else. Windows' device names (CON, NUL, COM1 and
// so on) are refused too, because Windows takes CON.rif for the screen, in any folder.
const kDeviceName = /^(con|prn|aux|nul|com[0-9]|lpt[0-9])$/i;
const here = path.resolve(process.cwd());

// ---- What to keep ----
const [fromName, toName, newName, ...wanted] = process.argv.slice(2);
if (!fromName || !toName || !newName || wanted.length === 0) stop('give it the bigger file, the new file, the new internal name and at least one model name');
// Both names are checked before any file is touched
if (!/^[A-Za-z0-9_]+$/.test(fromName) || kDeviceName.test(fromName)) {
  stop(`the bigger file is named with letters, digits and _ only, without .rif, and not as a device: "${fromName}" isn't`);
}
if (!/^[A-Za-z0-9_]+$/.test(toName) || kDeviceName.test(toName)) {
  stop(`the new file is named with letters, digits and _ only, without .rif, and not as a device: "${toName}" isn't`);
}
const from = path.join(here, `${fromName}.rif`);
const outFolder = path.join(here, 'out');
const to = path.join(outFolder, `${toName}.rif`);
if (fs.existsSync(to)) stop(`${to} is already there, and this never writes over a file`);
// The internal name is printed in the game's log and compared as text, so only plain letters, and short
if (!/^[A-Za-z0-9_]{1,31}$/.test(newName)) stop('the new internal name has to be 1 to 31 letters, digits or _');
if (new Set(wanted).size !== wanted.length) stop('a model was asked for twice');

const { data, top } = readModelFile(from);

const kKnown = new Set(['RIFVERIN', 'REBENVDT', 'OBJCHIER', 'RBOBJECT', 'REBSHAPE', 'INDSOUND', 'OBHALTSH']);
for (const c of top) if (!kKnown.has(c.name)) stop(`the file holds a ${c.name}, which this doesn't know what to do with`);
if (top.filter((c) => c.name === 'REBENVDT').length !== 1) stop('the file has to have exactly one REBENVDT');

// The models asked for, and every part name inside each one (an OBJHIERD names its part after a count and that many numbers)
const modelName = (c) => {
  const n = chunksIn(data, c.body, c.end, 'a model').filter((k) => k.name === 'OBHIERNM');
  if (n.length !== 1) stop('a model without exactly one name');
  return textIn(data, n[0].body, n[0].end, 'a model\'s name');
};
const partNames = new Set();
function partsOf(c, where) {
  for (const k of chunksIn(data, c.body, c.end, where)) {
    if (k.name === 'OBJHIERD') {
      if (k.end - k.body < 4) stop(`${where}: a part with no room for its name`);
      const count = data.readInt32LE(k.body);
      if (count < 0 || count > (k.end - k.body - 4) / 4) stop(`${where}: a part with an impossible count`);
      const name = textIn(data, k.body + 4 + 4 * count, k.end, `${where}: a part's name`);
      if (name) partNames.add(name);
    } else if (k.name === 'OBJCHIER') {
      partsOf(k, where);
    }
  }
}
const models = new Map();
for (const c of top.filter((c) => c.name === 'OBJCHIER')) {
  const name = modelName(c);
  if (models.has(name)) stop(`two models are named ${name}`);
  models.set(name, c);
}
for (const name of wanted) {
  if (!models.has(name)) stop(`${from} has no model named ${name}. It has: ${[...models.keys()].join(', ')}`);
  partsOf(models.get(name), name);
}

// The parts: an OBJHEAD1 holds the shape's number at 56 bytes in, and the part's name after it (the game's Object_Header_Chunk)
const parts = new Map();
for (const c of top.filter((c) => c.name === 'RBOBJECT')) {
  const head = chunksIn(data, c.body, c.end, 'a part').filter((k) => k.name === 'OBJHEAD1');
  if (head.length !== 1 || head[0].end - head[0].body < 61) stop('a part without its heading');
  const name = textIn(data, head[0].body + 60, head[0].end, 'a part\'s name');
  if (parts.has(name)) stop(`two parts are named ${name}`);
  parts.set(name, { chunk: c, shape: data.readInt32LE(head[0].body + 56) });
}
const keptParts = [...partNames].filter((n) => parts.has(n));
// A model can name a part that has no shape at all, such as "Root": the game makes it an empty joint, so it's fine
const emptyJoints = [...partNames].filter((n) => !parts.has(n));

// The shapes: a SHPHEAD1 holds the shape's number at 20 bytes in
const shapes = new Map();
for (const c of top.filter((c) => c.name === 'REBSHAPE')) {
  const head = chunksIn(data, c.body, c.end, 'a shape').filter((k) => k.name === 'SHPHEAD1');
  if (head.length !== 1 || head[0].end - head[0].body < 24) stop('a shape without its heading');
  const number = data.readInt32LE(head[0].body + 20);
  if (shapes.has(number)) stop(`two shapes are numbered ${number}`);
  shapes.set(number, c);
}
const keptShapes = new Set();
for (const n of keptParts) {
  const s = parts.get(n).shape;
  if (!shapes.has(s)) stop(`the part ${n} uses shape ${s}, and the file has no such shape`);
  keptShapes.add(s);
}

// The other looks: dropped, unless one changes a part that's kept
const kept = new Set(keptParts);
let looksDropped = 0;
for (const c of top.filter((c) => c.name === 'OBHALTSH')) {
  // a number, the look's name, then how many parts it changes, and each one's old and new part names, each padded to 4 bytes
  let at = c.body + 4;
  const lookName = textIn(data, at, c.end, 'a look\'s name');
  at += (lookName.length + 1 + 3) & ~3;
  if (c.end - at < 4) stop(`the look ${lookName} has no count`);
  const count = data.readInt32LE(at); at += 4;
  if (count < 0 || count > (c.end - at) / 8) stop(`the look ${lookName} has an impossible count`);
  for (let i = 0; i < 2 * count; i++) {
    const n = textIn(data, at, c.end, `a part name in the look ${lookName}`);
    at += (n.length + 1 + 3) & ~3;
    if (kept.has(n)) stop(`the look ${lookName} changes the part ${n}, which is kept, so dropping the look would lose it`);
  }
  looksDropped++;
}

// ---- Writing ----
// The settings chunk again, with only its internal name changed. The name is padded with zeros to a whole number of 4 bytes.
function settingsWithNewName(c) {
  const inside = chunksIn(data, c.body, c.end, 'the settings');
  if (inside.filter((k) => k.name === 'RIFFNAME').length !== 1) stop('the settings have to hold exactly one internal name');
  const pieces = [];
  for (const k of inside) {
    if (k.name === 'RIFFNAME') {
      const body = Buffer.alloc((newName.length + 1 + 3) & ~3);
      body.write(newName, 0, 'latin1');
      pieces.push(chunkHeading('RIFFNAME', body.length), body);
    } else {
      pieces.push(data.subarray(k.at, k.end));
    }
  }
  const body = Buffer.concat(pieces);
  return Buffer.concat([chunkHeading('REBENVDT', body.length), body]);
}

const out = [];
const count = { models: 0, parts: 0, shapes: 0, sounds: 0 };
for (const c of top) {
  if (c.name === 'RIFVERIN') out.push(data.subarray(c.at, c.end));
  else if (c.name === 'REBENVDT') out.push(settingsWithNewName(c));
  else if (c.name === 'OBJCHIER' && wanted.includes(modelName(c))) { out.push(data.subarray(c.at, c.end)); count.models++; }
  else if (c.name === 'RBOBJECT' && [...kept].some((n) => parts.get(n).chunk === c)) { out.push(data.subarray(c.at, c.end)); count.parts++; }
  else if (c.name === 'REBSHAPE' && [...keptShapes].some((s) => shapes.get(s) === c)) { out.push(data.subarray(c.at, c.end)); count.shapes++; }
  else if (c.name === 'INDSOUND') { out.push(data.subarray(c.at, c.end)); count.sounds++; }
}
const body = Buffer.concat(out);
const file = Buffer.concat([chunkHeading('REBINFF2', body.length), body]);

// Read back what's about to be written, the same way, so a mistake here can't reach the disk
const back = chunksIn(file, 0, file.length, 'the new file');
if (back.length !== 1) stop('the new file didn\'t read back as one chunk');
chunksIn(file, back[0].body, back[0].end, 'the new file');

// 'wx' makes the write itself refuse a file that appeared since the check above
if (!fs.existsSync(outFolder)) fs.mkdirSync(outFolder);
fs.writeFileSync(to, file, { flag: 'wx' });

const sha = crypto.createHash('sha256').update(file).digest('hex');
const droppedParts = [...parts.keys()].filter((n) => !kept.has(n));
console.log(`${from}: ${models.size} models, ${parts.size} parts, ${shapes.size} shapes.`);
console.log(`${to}: internal name ${newName}, ${count.models} models (${wanted.join(', ')}), ${count.parts} parts, ${count.shapes} shapes, ${count.sounds} sounds, ${file.length} bytes, sha256 ${sha}`);
console.log(`  joints with no shape, as in the bigger file: ${emptyJoints.join(', ') || 'none'}`);
console.log(`  other looks dropped: ${looksDropped}. Parts left out: ${droppedParts.length}: ${droppedParts.join(', ')}`);
