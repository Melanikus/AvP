// Reads the two logs of a hostile=yes pair run, and says for each bad message whether the machine it went to named the right reason for refusing it.
// A source check only shows that a line is in the code. This shows that the line stopped a message in a running game.
// It only reads. Run it with node:
//   node read-hostile.js <the host's run folder> <the joiner's run folder>
// from the folder that holds the game copies, each run folder being <copy>\runs\<date and time> (run-folders.js says why).
// A row passes only if the sender's log says it was sent, and the other machine's log names exactly that row's reason, in the order the rows went out.
// The whole run passes only if every row that counts passed, and both games ended by themselves with no crash.
'use strict';
const fs = require('fs');
const path = require('path');
const { runFolder } = require('./run-folders.js');

const here = __dirname;
const [hostGiven, joinGiven] = process.argv.slice(2);

if (!hostGiven || !joinGiven) {
  console.log('Give it two folders: the host\'s run and the joiner\'s run.');
  process.exit(2);
}

const hostDir = runFolder(hostGiven, 'The host\'s run folder');
const joinDir = runFolder(joinGiven, 'The joiner\'s run folder');

function logLines(dir) {
  return fs.readFileSync(path.join(dir, 'avp_log.txt'), 'latin1').split(/\r?\n/);
}

function runResult(dir, lines) {
  const file = path.join(dir, 'hudson-result.txt');
  const text = fs.existsSync(file) ? fs.readFileSync(file, 'latin1') : '';
  return {
    ended: (text.match(/ended=(.*?) exit=/) || [])[1] || 'no result file',
    exit: (text.match(/exit=(-?\d+)/) || [])[1] || '?',
    clean: /shutdown=clean/.test(text),
    crash: lines.filter((l) => /^crash:/.test(l)),
  };
}

const rows = fs.readFileSync(path.join(here, 'hostile-rows.txt'), 'latin1').split(/\r?\n/)
  .filter((l) => l && !l.startsWith('#'))
  .map((l) => {
    const [who, starts, reason, when] = l.split('\t');
    return { who, starts, reason, when };
  });

const host = logLines(hostDir);
const join = logLines(joinDir);

// What each machine says it sent, in order, and what each one refused, in order
function sentLines(lines) {
  return lines.map((l) => l.match(/^hostile: row (\S+): (.*)$/)).filter(Boolean).map((m) => (m[1] === 'SURVIVE' ? 'SURVIVE: ' + m[2] : m[2]));
}

function refusedLines(lines) {
  return lines.map((l) => l.match(/^net: refused: (.*?)(?: \(number (\d+) of these\))?$/)).filter(Boolean).map((m) => ({ why: m[1], number: m[2] ? +m[2] : 0 }));
}

const sent = { join: sentLines(join), host: sentLines(host), joinself: sentLines(join) };
const refused = { host: refusedLines(host), join: refusedLines(join) };

// A species game keeps the species scores. The host's log says which kind of game this is.
const isSpeciesGame = host.some((l) => /^hostile: this is a species game/.test(l));

// The kinds of run that aren't a list of rows
const hostKind = (host.map((l) => l.match(/^hostile: kind (\w+)/)).filter(Boolean)[0] || [])[1];

if (hostKind) {
  const joinKind = (join.map((l) => l.match(/^hostile: kind (\w+)/)).filter(Boolean)[0] || [])[1];
  const blocks = host.map((l) => l.match(/^hostile: blocks (\d+) of (\d+) strategy, (\d+) of (\d+) display/)).filter(Boolean);
  const memory = host.map((l) => l.match(/^hostile: memory (\d+) KB/)).filter(Boolean).map((m) => +m[1]);
  const most = (i) => Math.max(0, ...blocks.map((m) => +m[i]));
  const reasons = {};
  for (const r of refusedLines(host)) reasons[r.why] = Math.max(reasons[r.why] || 0, r.number);
  const hostResult = runResult(hostDir, host);
  const joinResult = runResult(joinDir, join);
  let bad = 0;
  console.log('host: most blocks in use ' + most(1) + ' of ' + (blocks[0] ? blocks[0][2] : '?') + ' strategy, ' + most(3) + ' of ' + (blocks[0] ? blocks[0][4] : '?') + ' display');
  console.log('host: memory from ' + memory[0] + ' KB to at most ' + Math.max(0, ...memory) + ' KB, a rise of ' + (Math.max(0, ...memory) - memory[0]) + ' KB');
  for (const why of Object.keys(reasons)) console.log('host refused, up to number ' + reasons[why] + ': ' + why);
  for (const l of host.filter((x) => /sent more bytes|has sent nothing|timed out|left the game|has gone/.test(x)).slice(0, 8)) console.log('host: ' + l.slice(0, 120));
  for (const [name, r] of [['host', hostResult], ['joiner', joinResult]]) {
    console.log(name + ': ended "' + r.ended + '", exit ' + r.exit + ', shutdown clean ' + r.clean + ', crashes ' + r.crash.length);
    r.crash.slice(0, 2).forEach((c) => console.log('  ' + c.slice(0, 120)));
  }
  if (hostResult.exit !== '0' || !hostResult.clean || hostResult.crash.length || joinResult.crash.length) bad++;
  if (joinKind === 'drain') {
    const short = Object.keys(reasons).filter((w) => /short of blocks/.test(w));
    if (!short.length) { console.log('NEVER REACHED WHAT IS KEPT BACK, so this drained nothing to the limit'); bad++; }
    if (!sentLines(host).some((l) => l.startsWith('SURVIVE: this player is being killed'))) { console.log('THE HOST NEVER GOT AS FAR AS BEING KILLED'); bad++; }
  }
  if (joinKind === 'hold') {
    const rise = Math.max(0, ...memory) - memory[0];
    if (rise >= 60 * 1024) { console.log('THE HOST HELD TOO MUCH'); bad++; }
    if (!host.some((l) => /sent more bytes in a second/.test(l))) { console.log('THE FLOOD NEVER REACHED THE LIMIT, so this held nothing to speak of'); bad++; }
    if (!sentLines(join).some((l) => l.startsWith('this game stops answering') || /stops answering/.test(l))) { console.log('THE JOINER NEVER STOPPED ANSWERING'); bad++; }
  }
  console.log(bad ? 'FAILED: ' + bad + ' things above' : 'PASSED');
  process.exit(bad ? 1 : 0);
}

const place = { host: 0, join: 0 };
let failed = 0;
let notCounted = 0;
const out = [];

for (const row of rows) {
  const sender = row.who;
  const saidSent = sent[sender].some((l) => l.startsWith(row.starts));
  const notSent = sent[sender].find((l) => l.startsWith(row.starts.split(' ')[0] + ' NOT SENT'));
  let result;

  if ((row.when === 'species' && !isSpeciesGame) || (row.when === 'campaign' && isSpeciesGame)) {
    result = 'not counted: only a ' + (row.when === 'species' ? 'species game' : 'campaign') + ' reaches this';
    notCounted++;
  } else if (row.when === 'ifsent' && notSent) {
    result = 'not counted: ' + notSent;
    notCounted++;
  } else if (row.reason === '-') {
    result = saidSent ? 'sent' : 'NOT SENT';
  } else if (row.reason === 'SHOWN') {
    result = saidSent ? 'shown by its own line' : (notSent ? 'NOT SHOWN: ' + notSent : 'NEVER GOT HERE');
    if (!saidSent) failed++;
  } else if (!saidSent) {
    result = notSent ? 'NOT SENT: ' + notSent : 'NOT SENT';
    failed++;
  } else {
    // joinself is the joiner asking itself, so its refusal is in the joiner's own log. Everything else goes to the other machine.
    const list = sender === 'host' || sender === 'joinself' ? 'join' : 'host';
    const refusals = refused[list];
    let found = -1;

    // The joiner's own question comes before it has heard from anyone, so it isn't in the order of the host's rows
    const start = sender === 'joinself' ? 0 : place[list];

    for (let i = start; i < refusals.length; i++) {
      if (refusals[i].why === row.reason) {
        found = i;
        break;
      }
    }

    if (found < 0) {
      result = 'NOT REFUSED BY THIS REASON';
      failed++;
    } else {
      result = 'refused, number ' + refusals[found].number;

      if (sender !== 'joinself') {
        place[list] = found + 1;

        // One row sends many, so the refusals after the first belong to this row as well
        if (row.when === 'flood') {
          while (place[list] < refusals.length && refusals[place[list]].why === row.reason) {
            place[list]++;
          }
          result += ' to ' + refusals[place[list] - 1].number + ' in the log';
        }
      }
    }
  }

  out.push([row.who, row.starts, row.reason, result]);
}

for (const line of out) {
  console.log(line.join(' | '));
}


// A reason that came up that no row expected is worth a look, even if every row passed
const expected = new Set(rows.map((r) => r.reason));
for (const list of ['host', 'join']) {
  const others = new Map();
  for (const r of refused[list]) {
    if (!expected.has(r.why)) others.set(r.why, (others.get(r.why) || 0) + 1);
  }
  for (const [why, count] of others) {
    console.log('also refused on ' + list + ', no row expected it: ' + why + ' (' + count + ' lines)');
  }
}

// N4: four hundred small datagrams used to cost 131 KB each, about 52 MB. The biggest step up in a game's memory has to be much less.
const kMostMemoryStepKB = 20 * 1024;

for (const [name, lines] of [['host', host], ['joiner', join]]) {
  const readings = lines.map((l) => l.match(/^hostile: memory (\d+) KB/)).filter(Boolean).map((m) => +m[1]);
  let biggest = 0;

  for (let i = 1; i < readings.length; i++) {
    biggest = Math.max(biggest, readings[i] - readings[i - 1]);
  }

  console.log(name + ': ' + readings.length + ' memory readings, biggest step up ' + biggest + ' KB');

  if (readings.length < 5 || biggest >= kMostMemoryStepKB) {
    console.log('  THAT IS TOO MUCH, OR TOO FEW READINGS');
    failed++;
  }
}

const hostResult = runResult(hostDir, host);
const joinResult = runResult(joinDir, join);

for (const [name, r] of [['host', hostResult], ['joiner', joinResult]]) {
  console.log(name + ': ended "' + r.ended + '", exit ' + r.exit + ', shutdown clean ' + r.clean + ', crashes ' + r.crash.length);
  r.crash.slice(0, 5).forEach((c) => console.log('  ' + c.slice(0, 120)));
  if (r.exit !== '0' || !r.clean || r.crash.length) failed++;
}

console.log(failed ? 'FAILED: ' + failed + ' things above' : 'PASSED, with ' + notCounted + ' rows this kind of game can\'t reach');
process.exit(failed ? 1 : 0);
