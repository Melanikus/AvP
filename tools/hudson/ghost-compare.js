// Lines up the two logs of a pair run with ghostlog=yes, and says where a joiner's ghost of an NPC differs from the real
// one on the host. One line per difference, for as long as it lasted, plainly enough to read without knowing the code.
// It only reads. Run it with node:
//   node ghost-compare.js <the host's run folder> <the joiner's run folder> [most mm apart before it counts, 500 if not said]
// The message takes a moment to arrive, and each log only says a line a second unless something changes, so two lines
// are compared if they're within a second of each other, and a difference has to last a second or be a death to be listed.
'use strict';
const fs = require('fs');
const path = require('path');

const [hostFolder, joinFolder, apart] = process.argv.slice(2);
const mostApart = Number(apart) || 500;
const kKinds = { 4: 'alien', 5: 'queen', 6: 'facehugger', 7: 'predator', 8: 'xenoborg', 9: 'marine', 19: 'sentry gun' };
const kMostAwayInTime = 1000;
const kShortestThatCounts = 1000;

if (!hostFolder || !joinFolder) {
  console.log('Give it two folders: the host\'s run and the joiner\'s run.');
  process.exit(2);
}

function readLines(folder) {
  const lines = [];

  for (const text of fs.readFileSync(path.join(folder, 'avp_log.txt'), 'latin1').split(/\r?\n/)) {
    if (text.startsWith('ghostlog: ')) {
      const line = {};

      for (const pair of text.slice(10).split(' ')) {
        const [name, value] = pair.split('=');
        line[name] = Number(value);
      }

      lines.push(line);
    }
  }

  return lines;
}

const host = readLines(hostFolder);
const join = readLines(joinFolder);

if (host.length === 0 || join.length === 0) {
  console.log(`No ghostlog lines to compare: ${host.length} on the host, ${join.length} on the joiner. Was the run asked for ghostlog=yes?`);
  process.exit(1);
}

const startedAt = Math.min(host[0].t, join[0].t);
const seconds = (t) => ((t - startedAt) / 1000).toFixed(1).padStart(6);
const hostEndedAt = host[host.length - 1].t;
const numbers = new Set([...host, ...join].map((line) => line.id));
const differences = [];
const facts = [];
let undrawnLines = 0;
let linesAfterTheHostStopped = 0;

function nearest(lines, t) {
  let best = null;

  for (const line of lines) {
    if (best === null || Math.abs(line.t - t) < Math.abs(best.t - t)) {
      best = line;
    }
  }

  return best && Math.abs(best.t - t) <= kMostAwayInTime ? best : null;
}

// Where the host's was at that moment, in a straight line between the host's lines either side. Nothing if they're too far apart in time to say, or it died between them.
function placeAt(lines, t) {
  let before = null;
  let after = null;

  for (const line of lines) {
    if (line.t <= t) {
      before = line;
    } else if (!after) {
      after = line;
    }
  }

  if (!before || !after || after.t - before.t > kMostAwayInTime * 1.2 || before.alive !== after.alive) {
    return null;
  }

  const along = (t - before.t) / (after.t - before.t);

  return {
    x: Math.round(before.x + (after.x - before.x) * along),
    y: Math.round(before.y + (after.y - before.y) * along),
    z: Math.round(before.z + (after.z - before.z) * along),
  };
}

// Runs of the same kind of difference become one line
function noteDifference(open, id, what, t, hostSays, joinSays) {
  const key = `${id} ${what}`;

  if (open[key] && t - open[key].last <= kMostAwayInTime * 2) {
    open[key].last = t;
    open[key].times++;
  } else {
    open[key] = { id, what, first: t, last: t, times: 1, hostSays, joinSays };
    differences.push(open[key]);
  }
}

for (const id of numbers) {
  const hostLines = host.filter((line) => line.id === id);
  const joinLines = join.filter((line) => line.id === id);
  const hostStates = hostLines.filter((line) => line.type !== undefined);
  const joinStates = joinLines.filter((line) => line.type !== undefined);
  const hostKind = (hostStates.find((line) => line.alive === 1) || {}).type;
  const name = `${kKinds[hostKind] || 'NPC'} ${id}`;
  const open = {};

  facts.push(`${name}: the host has ${hostStates.length} lines from ${hostStates.length ? seconds(hostStates[0].t) : '-'} s, and the joiner ${joinStates.length} from ${joinStates.length ? seconds(joinStates[0].t) : '-'} s. Sent ${Math.max(0, ...hostStates.map((line) => line.sent))} messages, ${Math.max(0, ...joinStates.map((line) => line.sent))} arrived${hostKind === 4 ? ' (an alien has older messages of its own, so none are counted)' : ''}.`);

  const hostDeath = hostLines.find((line) => line.death === 1);
  const joinDeath = joinLines.find((line) => line.death === 1);
  const hostDead = hostStates.find((line) => line.alive === 0);
  const joinDead = joinStates.find((line) => line.alive === 0);
  const joinGone = joinLines.find((line) => line.gone === 1);

  if (hostDeath || hostDead || joinDeath || joinDead) {
    facts.push(`${name}: dead on the host at ${hostDead ? seconds(hostDead.t) : 'never'} s and on the joiner at ${joinDead ? seconds(joinDead.t) : 'never'} s. The death message said code ${hostDeath ? hostDeath.code : '-'}, corpse time ${hostDeath ? hostDeath.time : '-'}.`);

    if (hostDead && !joinDead) {
      noteDifference(open, name, 'dead on the host, not on the joiner', hostDead.t, 'dead', 'alive or no ghost');
    }

    if (joinDead && !hostDead) {
      noteDifference(open, name, 'dead on the joiner, not on the host', joinDead.t, 'alive', 'dead');
    }
  }

  if (joinGone) {
    facts.push(`${name}: the joiner's ghost went at ${seconds(joinGone.t)} s.`);
  }

  for (const seen of joinStates) {
    // A ghost the joiner isn't drawing is a far one that nobody can see. Those step from room to room by themselves, and the host only logs what it has near.
    if (!seen.drawn) {
      undrawnLines++;
      continue;
    }

    // The two games don't stop together, and after the host's log ends there's nothing to compare with
    if (seen.t > hostEndedAt) {
      linesAfterTheHostStopped++;
      continue;
    }

    // The joiner's line comes a moment after the host's it's a copy of. So a change is only a difference if no line of the host's near that moment says the same.
    const hostSaidThisNear = (same) => hostStates.some((line) => Math.abs(line.t - seen.t) <= kMostAwayInTime && same(line));
    const real = nearest(hostStates, seen.t);

    if (!real) {
      noteDifference(open, name, 'the joiner draws a ghost, the host has nothing near that time', seen.t, 'nothing', `at ${seen.x} ${seen.y} ${seen.z}`);
      continue;
    }

    // Each log says a line a second, not at the same moment, and an alien runs several metres in a second. So this works out where the host's was at the moment of the joiner's line.
    const place = placeAt(hostStates, seen.t);
    const distance = place ? Math.round(Math.hypot(place.x - seen.x, place.y - seen.y, place.z - seen.z)) : 0;

    if (distance > mostApart) {
      noteDifference(open, name, `more than ${mostApart} mm apart`, seen.t, `at ${place.x} ${place.y} ${place.z}`, `at ${seen.x} ${seen.y} ${seen.z}, ${distance} mm away`);
    }

    if (!hostSaidThisNear((line) => line.alive === seen.alive)) {
      noteDifference(open, name, 'alive on one and dead on the other', seen.t, real.alive ? 'alive' : 'dead', seen.alive ? 'alive' : 'dead');
    }

    if (!hostSaidThisNear((line) => (line.cloak !== 0) === (seen.cloak !== 0))) {
      noteDifference(open, name, 'cloaked on one and not the other', seen.t, `cloak ${real.cloak}`, `cloak ${seen.cloak}`);
    } else if (!hostSaidThisNear((line) => Math.abs(line.cloak - seen.cloak) <= 16384)) {
      noteDifference(open, name, 'cloaked by very different amounts', seen.t, `cloak ${real.cloak}`, `cloak ${seen.cloak}`);
    }

    // A model asked for an animation it hasn't got plays its first one instead, so the numbers can agree while the picture doesn't
    if (seen.seqok === 0) {
      noteDifference(open, name, "the joiner's model hasn't got this animation", seen.t, `sequence ${real.seq} ${real.sub}`, `sequence ${seen.seq} ${seen.sub}`);
    }

    if (!hostSaidThisNear((line) => (line.beams || 0) === (seen.beams || 0))) {
      noteDifference(open, name, 'different beams on', seen.t, `beams ${real.beams}`, `beams ${seen.beams}`);
    }

    // An animation changing near this moment isn't a difference, only the two lines not being at the same moment
    if (!hostSaidThisNear((line) => line.seq === seen.seq && line.sub === seen.sub)) {
      noteDifference(open, name, 'a different animation', seen.t, `sequence ${real.seq} ${real.sub}`, `sequence ${seen.seq} ${seen.sub}`);
    }
  }

  // A long wait for a message means the ghost isn't being kept up to date. A dead one has nothing more to be told.
  for (const seen of joinStates) {
    if (seen.alive === 1 && seen.sent > 0 && seen.age > kMostAwayInTime * 2) {
      noteDifference(open, name, 'no message for over 2 seconds', seen.t, '-', `${seen.age} ms since the last`);
    }
  }
}

console.log(`Host ${path.basename(hostFolder)}, joiner ${path.basename(joinFolder)}. Times are seconds from the first line in either log.`);
console.log('');
facts.forEach((text) => console.log(text));
console.log('');

const worthListing = differences
  .filter((row) => row.last - row.first >= kShortestThatCounts || /dead|hasn't got/.test(row.what))
  .sort((a, b) => a.first - b.first);

if (worthListing.length === 0) {
  console.log(`No difference lasting ${kShortestThatCounts / 1000} seconds or more.`);
} else {
  console.log('  from     to  NPC                  what differs                                 host says                  joiner says');

  for (const row of worthListing) {
    console.log(`${seconds(row.first)} ${seconds(row.last)}  ${row.id.padEnd(20).slice(0, 20)} ${row.what.padEnd(44)} ${String(row.hostSays).padEnd(26)} ${row.joinSays}`);
  }
}

console.log('');
console.log(`${worthListing.length} differences listed, and ${differences.length - worthListing.length} shorter ones left out. ${undrawnLines} lines of ghosts the joiner wasn't drawing weren't compared, because each machine only draws what's near its own player, and ${linesAfterTheHostStopped} came after the host's log had ended. The two logs were compared by the Windows clock, so this only works for two games on one machine.`);
