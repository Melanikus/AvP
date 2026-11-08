// Written 2026-11-07 by Melanikus for the AvP port.
//
// Makes the new guns' small sound file out of a bigger one. It copies only the sounds it's told to, each one byte for byte
// as it was, then writes the end mark. Nothing else from the bigger file comes along.
//
// Why: the MelPyth mod's sound file (MelPyth\fastfile\common.ffl) holds 15 sounds, but the game only takes 4 of them (the
// list is kNewWeaponSounds in 3dc/avp/psndproj.cpp). The other 11 are the mod's own versions of sounds the game already
// has, and the user ruled that the game keeps its own. So the file we ship holds just the 4.
//
// How to run it, with node, from the folder that holds the files. Both file names have to be inside that folder (see
// pathInside below), and the tool itself can be anywhere:
//   node <the repository>/tools/m5-files/sound-bank-pick.js <the bigger file> <the new file> <sound name> [<sound name> ...]
// For example:
//   node <the repository>/tools/m5-files/sound-bank-pick.js M5\common.ffl out\common.ffl Revolver_Fire.wav headshot.wav AutoShotgun4.wav SniperRifle_Fire.wav
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

// Every file name on the command line comes through here first, before any file is looked at. A name has to be a plain path
// inside the folder the tool is run from: not one that starts from a drive or a share, not one that climbs out with "..", and
// not one that names one of Windows' devices (CON, NUL, COM1 and so on, with or without an ending). Anything else stops the
// tool before it touches a file, so a mistyped or hostile name can't make it read or write somewhere else. It's the same rule
// as tests/takeouts/find-moved-row.js. Each tool keeps its own copy, because the security scanner is only known to follow a
// check made in the same file.
const kDeviceName = /^(con|prn|aux|nul|com[0-9]|lpt[0-9]|conin\$|conout\$)(\..*)?$/i;
function pathInside(name, what) {
  if (typeof name !== 'string' || name === '' || name.includes('\0')) stop(`${what} has to be a file name`);
  // A drive (C:), a share (\\server), a device path (\\.\ or \\?\) or a path from the top of a drive (\name) starts somewhere else
  if (path.isAbsolute(name) || name.includes(':') || name.startsWith('\\') || name.startsWith('/')) {
    stop(`${what} "${name}" has to be a path inside the folder this is run from, not one that starts somewhere else`);
  }
  const parts = name.split(/[\\/]/);
  if (parts.includes('..') || parts.some((part) => kDeviceName.test(part.trim()))) stop(`${what} "${name}" climbs out of this folder or names a device`);
  const here = path.resolve(process.cwd());
  const whole = path.resolve(here, name);
  if (!whole.startsWith(here + path.sep)) stop(`${what} "${name}" isn't inside the folder this is run from`);
  return whole;
}

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
const from = pathInside(fromName, 'the bigger file');
const to = pathInside(toName, 'the new file');
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
fs.writeFileSync(to, out, { flag: 'wx' });

const sha = (bytes) => crypto.createHash('sha256').update(bytes).digest('hex');
console.log(`${from}: ${entries.length} sounds, all as the game expects. ${to}: ${picked.length} of them, ${out.length} bytes, sha256 ${sha(out)}`);
for (const e of picked) {
  console.log(`  number ${e.number}, pitch ${e.pitch}, ${e.name}, ${e.end - e.start} bytes, sha256 ${sha(file.subarray(e.start, e.end))}`);
}
