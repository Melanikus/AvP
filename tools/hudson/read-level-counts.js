// Reads what each level holds when it starts, from the "hudson counts:" line of each run's log, and says where the
// four ways of playing a level disagree: single player, a skirmish, a network host, and the machine that joined it.
// The promise is that the mode plays as single player does, so the first three should hold the same things. A joiner
// runs no NPCs and only sees a ghost of some, so it's listed to read, not expected to match.
// It only reads. Run it with node, on one or more folders that hold run folders:
//   node read-level-counts.js <copy>\runs [<copy>\runs...] [--from <first run folder's name>]
// from the folder that holds the game copies (run-folders.js says why).
// --from leaves out older runs, so that it only reads one build's. Where a level has run more than once the latest counts.
'use strict';
const fs = require('fs');
const path = require('path');
const { runsFolder, isRunName } = require('./run-folders.js');

// These are what the promise is about. Anything else that differs is listed too, below them.
const npcKinds = ['alien', 'marine', 'predator', 'xenoborg', 'queen', 'facehugger', 'sentry gun', 'dormant predator', 'predator alien', 'seal', 'generator'];
const roles = ['single', 'skirmish', 'host', 'joiner'];

const args = process.argv.slice(2);
const fromAt = args.indexOf('--from');
const from = fromAt >= 0 ? args[fromAt + 1] : '';
const folders = args.filter((word, i) => i !== fromAt && i !== fromAt + 1);

if (folders.length === 0) {
  console.log('Give it at least one folder that holds run folders.');
  process.exit(2);
}

// "hudson counts: 93 in all, light 2, link switch 4" becomes { light: 2, 'link switch': 4 }
function readCounts(line) {
  const counts = {};

  for (const part of line.replace(/^hudson counts: \d+ in all,? */, '').split(', ')) {
    const match = part.match(/^(.*) (\d+)$/);

    if (match) {
      counts[match[1]] = Number(match[2]);
    }
  }

  return counts;
}

function roleOf(words) {
  if (/ net=host/.test(words)) return 'host';
  if (/ net=join/.test(words)) return 'joiner';
  if (/ listplace=/.test(words)) return 'skirmish';
  return 'single';
}

const levels = {};
let runsRead = 0;
let runsWithoutCounts = 0;

for (const folder of folders.map((given) => runsFolder(given, 'each folder of runs'))) {
  // Only the folders run-hudson.ps1 made, by their own names
  for (const name of fs.readdirSync(folder).filter(isRunName).map((found) => path.basename(found)).sort()) {
    const runFile = path.join(folder, name, 'run.txt');
    const logFile = path.join(folder, name, 'avp_log.txt');

    if (name < from || !fs.existsSync(runFile) || !fs.existsSync(logFile)) {
      continue;
    }

    const words = (fs.readFileSync(runFile, 'latin1').match(/^arguments=(.*)$/m) || [])[1] || '';
    const level = (words.match(/ level=([^ \r]+)/) || [])[1];

    if (!level) {
      continue;
    }

    runsRead++;
    const line = fs.readFileSync(logFile, 'latin1').split(/\r?\n/).find((text) => text.startsWith('hudson counts:'));

    if (!line) {
      runsWithoutCounts++;
      continue;
    }

    levels[level] = levels[level] || {};
    levels[level][roleOf(words)] = { counts: readCounts(line), run: name };
  }
}

let levelsThatDiffer = 0;
let levelsNotComplete = 0;

for (const level of Object.keys(levels).sort()) {
  const seen = levels[level];
  const missing = roles.filter((role) => !seen[role]);
  const kinds = new Set();

  for (const role of roles) {
    if (seen[role]) {
      Object.keys(seen[role].counts).forEach((kind) => kinds.add(kind));
    }
  }

  // Only the three that should match. A kind that's absent is 0 of it.
  const differs = [...kinds].filter((kind) => {
    const numbers = ['single', 'skirmish', 'host'].filter((role) => seen[role]).map((role) => seen[role].counts[kind] || 0);
    return new Set(numbers).size > 1;
  });
  const npcsHere = npcKinds.filter((kind) => kinds.has(kind));

  console.log(`== ${level}${missing.length ? `, no run as: ${missing.join(', ')}` : ''}`);

  for (const role of roles) {
    if (seen[role]) {
      const said = npcsHere.map((kind) => `${kind} ${seen[role].counts[kind] || 0}`).join(', ') || 'no NPCs';
      // A joiner's NPCs are ghosts, counted under their behaviour's number
      const ghosts = Object.keys(seen[role].counts).filter((kind) => kind.startsWith('ghost of behaviour')).map((kind) => `${kind.replace('ghost of behaviour', 'ghost')} x${seen[role].counts[kind]}`).join(', ');
      console.log(`   ${role.padEnd(8)} ${said}${ghosts ? `; ${ghosts}` : ''}  (${seen[role].run})`);
    }
  }

  for (const kind of differs) {
    console.log(`   DIFFERS ${kind}: ${roles.filter((role) => seen[role]).map((role) => `${role} ${seen[role].counts[kind] || 0}`).join(', ')}`);
  }

  levelsThatDiffer += differs.length ? 1 : 0;
  levelsNotComplete += missing.length ? 1 : 0;
}

console.log('');
console.log(`${Object.keys(levels).length} levels from ${runsRead} runs, ${runsWithoutCounts} runs said no counts, ${levelsNotComplete} levels are missing a way of playing, ${levelsThatDiffer} levels differ`);
