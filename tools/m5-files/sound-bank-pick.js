// Written 2026-11-07 by Melanikus for the AvP port.
//
// Makes the new guns' small sound file out of a bigger one. It copies only the sounds it's told to, each one byte for byte
// as it was, then writes the end mark. Nothing else from the bigger file comes along.
//
// Why: the MelPyth mod's sound file (MelPyth\fastfile\common.ffl) holds 15 sounds, but the game only takes 4 of them (the
// list is kNewWeaponSounds in 3dc/avp/psndproj.cpp). The other 11 are the mod's own versions of sounds the game already
// has, and the user ruled that the game keeps its own. So the file we ship holds just the 4.
//
// How to run it, with node, from the folder that holds the file it reads (see the names' check below). The new file goes in that
// folder's "out" folder, which it makes if it isn't there. The tool itself can be anywhere:
//   node <the repository>/tools/m5-files/sound-bank-pick.js <the bigger file's name> <the new file's name> <sound name> [<sound name> ...]
// For example:
//   node <the repository>/tools/m5-files/sound-bank-pick.js common common Revolver_Fire.wav headshot.wav AutoShotgun4.wav SniperRifle_Fire.wav
//   reads common.ffl and writes out\common.ffl
//
// It only reads the bigger file and never changes it. It won't write over a file that's already there, so it can't destroy
// anything by mistake. It stops, writing nothing, if a name isn't found, or is found twice, or if anything in the bigger file
// isn't the way the game's reader expects.
//
// The layout of a sound file, the same as 3dc/avp/weapon_sound_bank.h describes:
//   a row of entries, then two bytes of 255 that mark the end. Each entry is:
//   one byte, the sound's number
//   one byte, how far to shift its pitch
//   its name, with a zero after it
//   the sound itself, a whole .wav file: the letters RIFF, a size, the letters WAVE, then named blocks
// The checks below are the game's own checks from that file, in the same order, so a file this accepts is one the game
// accepts. The real proof is still the game's reader: run it over the new file afterwards.

'use strict';

const fs = require('fs');
const path = require('path');
const crypto = require('crypto');

// The game won't read a sound file bigger than this (kLargestNewWeaponSoundFile in psndproj.cpp), so neither will this
const kLargestFile = 16 * 1024 * 1024;
// The longest name the game accepts (kSoundBankLongestName in weapon_sound_bank.h)
const kLongestName = 128;

function stop(why) {
  console.error('sound-bank-pick: ' + why + '. Nothing was written.');
  process.exit(1);
}

// Each file is named, not given as a path: letters, digits and _ only, and the tool adds its ending. The file it reads is in the
// folder it's run from, and the new file goes in that folder's "out" folder. That's the same check tests/takeouts/find-moved-row.js
// makes on the name of the list it's given, so no name can reach a file anywhere else. Windows' device names (CON, NUL, COM1 and
// so on) are refused too, because Windows takes CON.rif for the screen, in any folder.
const kDeviceName = /^(con|prn|aux|nul|com[0-9]|lpt[0-9])$/i;
const here = path.resolve(process.cwd());

function lettersAre(bytes, at, letters) {
  return bytes.toString('latin1', at, at + 4) === letters;
}

// Reads the entry that starts at "at". Gives back where its parts are, or says why it's wrong. Every size is compared with
// what's left of the file, never added to a position first, so a huge size can't wrap round and slip through.
function readEntry(file, at) {
  const size = file.length;
  if (at > size || size - at < 2) return { wrong: 'the file stops before its end mark' };
  if (file[at] === 0xff && file[at + 1] === 0xff) return { atEndMark: true };

  const number = file[at];
  const pitch = file.readInt8(at + 1);

  // The name, up to its zero, in plain letters only
  const nameAt = at + 2;
  let nameLength = 0;
  for (;;) {
    if (nameAt + nameLength >= size || nameLength > kLongestName) return { wrong: 'a name never ends' };
    const letter = file[nameAt + nameLength];
    if (letter === 0) break;
    if (letter < 0x20 || letter > 0x7e) return { wrong: 'a name has letters that aren\'t plain' };
    nameLength++;
  }
  if (nameLength === 0) return { wrong: 'a name is empty' };
  const name = file.toString('latin1', nameAt, nameAt + nameLength);

  // The .wav: RIFF, then its size, which counts everything after the size itself
  const riffAt = nameAt + nameLength + 1;
  if (size - riffAt < 8 || !lettersAre(file, riffAt, 'RIFF')) return { wrong: `${name} isn't a RIFF` };
  const riffSize = file.readUInt32LE(riffAt + 4);
  const insideAt = riffAt + 8;
  if (riffSize > size - insideAt) return { wrong: `${name} runs past the end of the file` };
  const insideEnd = insideAt + riffSize;

  // WAVE, and straight after it the block that describes the sound
  if (insideEnd - insideAt < 4 || !lettersAre(file, insideAt, 'WAVE')) return { wrong: `${name} isn't a WAVE` };
  let place = insideAt + 4;
  if (insideEnd - place < 8 || !lettersAre(file, place, 'fmt ')) return { wrong: `${name} doesn't start with its format block` };
  const formatSize = file.readUInt32LE(place + 4);
  place += 8;
  if ((formatSize !== 16 && formatSize !== 18) || insideEnd - place < formatSize) return { wrong: `${name}'s format block is the wrong size` };
  const kind = file.readUInt16LE(place);
  const channels = file.readUInt16LE(place + 2);
  const samples = file.readUInt32LE(place + 4);
  const bytesPerSecond = file.readUInt32LE(place + 8);
  const bits = file.readUInt16LE(place + 14);
  if (kind !== 1 || (formatSize === 18 && file.readUInt16LE(place + 16) !== 0)) return { wrong: `${name} isn't plain audio` };
  if ((channels !== 1 && channels !== 2) || (bits !== 8 && bits !== 16)) return { wrong: `${name} has channels or bits the game can't play` };
  if (samples === 0 || bytesPerSecond === 0) return { wrong: `${name} doesn't say how fast it plays` };
  place += formatSize;

  // The other blocks, until the audio itself
  for (;;) {
    if (insideEnd - place < 8) return { wrong: `${name} has no audio block` };
    const blockSize = file.readUInt32LE(place + 4);
    const isAudio = lettersAre(file, place, 'data');
    place += 8;
    if (blockSize > insideEnd - place) return { wrong: `${name} has a block that runs past its end` };
    if (isAudio) break;
    place += blockSize;
  }

  return { number, pitch, name, start: at, end: insideEnd };
}

const [fromName, toName, ...wanted] = process.argv.slice(2);
if (!fromName || !toName || wanted.length === 0) stop('give it the bigger file, the new file, and at least one sound name');
// Both names are checked before any file is touched
if (!/^[A-Za-z0-9_]+$/.test(fromName) || kDeviceName.test(fromName)) {
  stop(`the bigger file is named with letters, digits and _ only, without .ffl, and not as a device: "${fromName}" isn't`);
}
if (!/^[A-Za-z0-9_]+$/.test(toName) || kDeviceName.test(toName)) {
  stop(`the new file is named with letters, digits and _ only, without .ffl, and not as a device: "${toName}" isn't`);
}
const from = path.join(here, `${fromName}.ffl`);
const outFolder = path.join(here, 'out');
const to = path.join(outFolder, `${toName}.ffl`);
if (fs.existsSync(to)) stop(`${to} is already there, and this never writes over a file`);

const stat = fs.statSync(from);
if (stat.size > kLargestFile) stop(`${from} is bigger than the game would ever read`);
const file = fs.readFileSync(from);

// Every entry is read and checked, the ones not wanted too: a file with one bad part can't be trusted about the rest
const entries = [];
for (let at = 0; ;) {
  const entry = readEntry(file, at);
  if (entry.wrong) stop(`${from}: ${entry.wrong}, at byte ${at}`);
  if (entry.atEndMark) break;
  entries.push(entry);
  at = entry.end;
}

// The game finds a sound by its name, in any case (_stricmp in LoadNewWeaponSounds), so names are matched the same way
const picked = [];
for (const name of wanted) {
  const found = entries.filter((e) => e.name.toLowerCase() === name.toLowerCase());
  if (found.length === 0) stop(`${from} has no sound named ${name}`);
  if (found.length > 1) stop(`${from} has ${found.length} sounds named ${name}, so which one is meant isn't clear`);
  if (picked.includes(found[0])) stop(`${name} was asked for twice`);
  picked.push(found[0]);
}

// The new file keeps the bigger file's order, so its sounds come in the same order the game met them before
picked.sort((a, b) => a.start - b.start);
const parts = picked.map((e) => file.subarray(e.start, e.end));
parts.push(Buffer.from([0xff, 0xff]));
const out = Buffer.concat(parts);

// 'wx' makes the write itself refuse a file that appeared since the check above
if (!fs.existsSync(outFolder)) fs.mkdirSync(outFolder);
fs.writeFileSync(to, out, { flag: 'wx' });

const sha = (bytes) => crypto.createHash('sha256').update(bytes).digest('hex');
console.log(`${from}: ${entries.length} sounds, all as the game expects. ${to}: ${picked.length} of them, ${out.length} bytes, sha256 ${sha(out)}`);
for (const e of picked) {
  console.log(`  number ${e.number}, pitch ${e.pitch}, ${e.name}, ${e.end - e.start} bytes, sha256 ${sha(file.subarray(e.start, e.end))}`);
}
