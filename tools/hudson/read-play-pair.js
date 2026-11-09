// Reads the two logs of a pair of Hudsons playing a level on foot (run-net-pair.ps1 -Play), and says what each did, side by side, plainly enough to read without knowing the code. It only reads.
// Written 2026-11-07 by Melanikus for the AvP port. Run it with node:
//   node read-play-pair.js <the host's run folder> <the joiner's run folder>
//
// What it says, for each machine:
// - how the run ended, and the play_ numbers from hudson-result.txt
// - every hop, which is a cheat: when (t=, the Windows clock, which both games on one machine share), how far, and why
// - every fall the game noticed, every wait for the other, and every gun he held
// - what he did at each lift: walking to it, waiting for the car, stepping on, riding, stepping off, or giving up
// Then, for both machines together:
// - each step's switch: when each machine saw it change, and how far apart the two were
// - each lift's car: each change of state on each machine, in time order, so that a car that moved on one machine and not the other shows
'use strict';
const fs = require('fs');
const path = require('path');

const [hostFolder, joinFolder] = process.argv.slice(2);

if (!hostFolder || !joinFolder) {
  console.log('Give it two folders: the host\'s run and the joiner\'s run.');
  process.exit(2);
}

function readRun(folder, who) {
  const text = fs.readFileSync(path.join(folder, 'avp_log.txt'), 'latin1');
  const resultFile = path.join(folder, 'hudson-result.txt');
  const result = fs.existsSync(resultFile) ? fs.readFileSync(resultFile, 'latin1') : '';
  const run = { who, folder, hops: [], falls: [], waits: [], guns: [], lift: [], watch: [], liftlog: [], liftNames: {}, touches: [], ended: '', numbers: '' };

  const numbers = result.match(/play_steps=.*/);
  run.numbers = numbers ? numbers[0].trim() : '(no play_ numbers in hudson-result.txt)';
  const ended = result.match(/ended=([^\r\n]*?) exit=(\d+)/);
  run.ended = ended ? `exit ${ended[2]}: ${ended[1]}` : '(no result line)';

  for (const line of text.split(/\r?\n/)) {
    let m;

    if ((m = line.match(/cheat (\d+): he's moved (\d+) mm to [-\d]+ [-\d]+ [-\d]+, because (.*?)(?:, t=(\d+))?, at [-\d]+ [-\d]+ [-\d]+ in room (\S+)/))) {
      run.hops.push({ n: Number(m[1]), mm: Number(m[2]), why: m[3], t: m[4] ? Number(m[4]) : null, room: m[5] });
    } else if (/: cheat \d+:/.test(line)) {
      run.hops.push({ n: Number(line.match(/cheat (\d+)/)[1]), mm: null, why: line.replace(/^.*?cheat \d+: /, ''), t: null, room: '' });
    } else if ((m = line.match(/^touch: t=(\d+) the two touch, at (-?\d+ -?\d+ -?\d+), (near a lift|not near a lift)/))) {
      run.touches.push({ t: Number(m[1]), at: m[2], nearALift: m[3] === 'near a lift' });
    } else if ((m = line.match(/he fell (\d+) mm, t=(\d+)(.*?), at /))) {
      run.falls.push({ mm: Number(m[1]), t: Number(m[2]), onto: m[3] });
    } else if (/he waits for the other Hudson|beside him after a wait|goes on without him/.test(line)) {
      run.waits.push(line.replace(/^play: (step \d+ of \d+|following): /, ''));
    } else if (line.startsWith('play: gun:')) {
      run.guns.push(line.slice(11));
    } else if ((m = line.match(/^lift: t=(\d+) me (?!near )(.*)$/))) {
      // What he did at a lift. The "near" lines, ten a second, are for the lift test's own reader, and are left out here
      // so that they don't swamp this.
      run.lift.push({ t: Number(m[1]), what: m[2] });
    } else if ((m = line.match(/^play: watch: t=(\d+) (\S+) (?:is|was .*?, and is now) (.*)$/))) {
      run.watch.push({ t: Number(m[1]), name: m[2], state: m[3] });
    } else if ((m = line.match(/^liftlog: name=(\w+) is (.*)$/))) {
      run.liftNames[m[1]] = m[2];
    } else if ((m = line.match(/^liftlog: t=(\d+) name=(\w+) y=(-?\d+) state=(\d) end=(\w+) sent=(-?\d+)/))) {
      run.liftlog.push({ t: Number(m[1]), name: m[2], y: Number(m[3]), state: Number(m[4]), end: m[5], sent: Number(m[6]) });
    }
  }

  return run;
}

const kStates = ['stopped', 'about to move', 'going up', 'going down'];
const runs = [readRun(hostFolder, 'host'), readRun(joinFolder, 'joiner')];
const firstT = Math.min(...runs.flatMap((r) => [...r.watch, ...r.liftlog, ...r.lift].map((x) => x.t)).filter((t) => t > 0));
const at = (t) => (t === null || t === undefined ? '   (no time)' : `${((t - firstT) / 1000).toFixed(2).padStart(8)} s`);

console.log(`Times are seconds after the first timed line in either log (t=${firstT}).\n`);

for (const run of runs) {
  console.log(`==== The ${run.who}: ${run.folder}`);
  console.log(`  ended: ${run.ended}`);
  console.log(`  ${run.numbers}`);
  console.log(`  hops: ${run.hops.length}`);
  for (const hop of run.hops) {
    console.log(`    ${at(hop.t)}  cheat ${hop.n}: ${hop.mm === null ? '' : hop.mm + ' mm, '}${hop.why}${hop.room ? ', from ' + hop.room : ''}`);
  }
  // The user's rule: never on top of each other. A touch away from a lift fails the run; one at a lift is only counted.
  const awayFromLifts = run.touches.filter((touch) => !touch.nearALift);
  console.log(`  frames the two touched: ${run.touches.length}, of which ${awayFromLifts.length} were away from a lift${awayFromLifts.length ? ', which FAILS the rule' : ''}`);
  for (const touch of awayFromLifts.slice(0, 20)) {
    console.log(`    ${at(touch.t)}  at ${touch.at}`);
  }
  console.log(`  falls over 2 m: ${run.falls.length}`);
  for (const fall of run.falls) {
    console.log(`    ${at(fall.t)}  ${fall.mm} mm${fall.onto}`);
  }
  // Each wait has a line when it starts and one when it ends. Only the long ones, and those he gave up on, are listed.
  const endings = run.waits.filter((w) => /after a wait|goes on without him/.test(w));
  const long = endings.filter((w) => /goes on without him/.test(w) || Number((w.match(/ (\d+\.\d) seconds/) || [])[1]) >= 3);
  console.log(`  waits for the other: ${endings.length}, of which ${long.length} were 3 seconds or more, or ended without him`);
  for (const wait of long) {
    console.log(`    ${wait.slice(0, 160)}`);
  }
  console.log(`  guns: ${run.guns.length ? '' : 'none said'}`);
  for (const gun of run.guns) {
    console.log(`    ${gun}`);
  }
  console.log(`  at the lifts: ${run.lift.length ? '' : 'nothing'}`);
  for (const step of run.lift) {
    console.log(`    ${at(step.t)}  ${step.what.slice(0, 220)}`);
  }
  console.log('');
}

console.log('==== Each step\'s switch, as each machine saw it change');
const switchNames = [...new Set(runs.flatMap((r) => r.watch.map((w) => w.name)))];
for (const name of switchNames) {
  console.log(`  ${name}`);
  const host = runs[0].watch.filter((w) => w.name === name);
  const join = runs[1].watch.filter((w) => w.name === name);
  for (let i = 0; i < Math.max(host.length, join.length); i++) {
    const h = host[i], j = join[i];
    const apart = h && j ? `, ${Math.abs(h.t - j.t)} ms apart` : h ? ', and the joiner never saw this' : ', and the host never saw this';
    console.log(`    host ${h ? at(h.t) + ' ' + h.state : '            -'}  |  joiner ${j ? at(j.t) + ' ' + j.state : '-'}${apart}`);
  }
}

console.log('\n==== Each lift\'s car, as each machine saw it: every change of state, in time order');
const liftNames = [...new Set(runs.flatMap((r) => r.liftlog.map((l) => l.name)))];
for (const name of liftNames) {
  const title = runs[0].liftNames[name] || runs[1].liftNames[name] || name;
  const changes = [];
  for (const run of runs) {
    let last = null;
    for (const line of run.liftlog.filter((l) => l.name === name)) {
      if (last === null || line.state !== last) {
        changes.push({ who: run.who, ...line });
      }
      last = line.state;
    }
  }
  changes.sort((a, b) => a.t - b.t);
  const stops = (who) => changes.filter((c) => c.who === who && c.state === 0).map((c) => c.y);
  console.log(`  ${title} (${name})`);
  for (const c of changes) {
    console.log(`    ${at(c.t)}  ${c.who.padEnd(6)} ${kStates[c.state] || 'state ' + c.state}, at height ${c.y}, end ${c.end}${c.sent >= 0 ? ', messages ' + c.sent : ''}`);
  }
  const hostStops = stops('host').join(' '), joinStops = stops('joiner').join(' ');
  console.log(`    stops on the host: ${hostStops || 'none'}; on the joiner: ${joinStops || 'none'}${hostStops === joinStops ? ', the same' : ', NOT the same'}`);
}
