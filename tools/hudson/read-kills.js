// Reads the two logs of a pair run with killkind=, and says what each machine really saw: was the level's own NPC there on the host, did the joiner see it, did the killer kill it, and did its death move the level on for each machine. Written 2026-11-06 by Melanikus for the AvP port.
// It only reads. A line in the code only shows the code is there. This shows it did its job in a running game, and the two machines are compared rather than either one believed.
//   node read-kills.js <the host's run folder> <the joiner's run folder> <host or join> [the doors its death opens, with commas]
// from the folder that holds the game copies, each run folder being <copy>\runs\<date and time> (run-folders.js says why).
// It prints one line of tab-separated results, then what went wrong, if anything. It gives back 0 only if everything was seen.
'use strict';
const fs = require('fs');
const path = require('path');
const { runFolder } = require('./run-folders.js');

const [hostGiven, joinGiven, killer, opens] = process.argv.slice(2);

if (!hostGiven || !joinGiven || (killer !== 'host' && killer !== 'join')) {
    console.log('usage: node read-kills.js <host run> <joiner run> <host or join> [doors]');
    process.exit(2);
}

const hostFolder = runFolder(hostGiven, 'The host\'s run folder');
const joinFolder = runFolder(joinGiven, 'The joiner\'s run folder');

function linesOf(folder, name) {
    try {
        return fs.readFileSync(path.join(folder, name), 'latin1').split(/\r?\n/);
    } catch (e) {
        return [];
    }
}

const hostLog = linesOf(hostFolder, 'avp_log.txt');
const joinLog = linesOf(joinFolder, 'avp_log.txt');
const hostResult = linesOf(hostFolder, 'hudson-result.txt');
const joinResult = linesOf(joinFolder, 'hudson-result.txt');
const wrong = [];

function find(log, pattern) {
    return log.filter(l => pattern.test(l));
}

// The smoke test comes first. Before anything else starts, the two players turn to each other and each checks it can see the other. Each game's run.txt says how that went. A failed one fails this row whatever else the run did, and so does a run.txt that says nothing about it.
function smokeOf(folder) {
    const said = linesOf(folder, 'run.txt').find(l => /^smoke: /.test(l));
    return said ? said.slice('smoke: '.length) : 'failed: its run.txt says nothing about it';
}

const hostSmoke = smokeOf(hostFolder);
const joinSmoke = smokeOf(joinFolder);

if (!hostSmoke.startsWith('passed')) {
    wrong.push('smoke test on the host: ' + hostSmoke);
}

if (!joinSmoke.startsWith('passed')) {
    wrong.push('smoke test on the joiner: ' + joinSmoke);
}

// Made on the host: the level's own NPC, by where the level file puts it
const madeLine = find(hostLog, /^killnamed: on the host: level name /)[0];
const nothing = find(hostLog, /^killnamed: on the host: nothing of that kind/)[0];
const levelName = madeLine ? (madeLine.match(/level name ([0-9a-f]+)/) || [])[1] : null;
let number = madeLine ? (madeLine.match(/object number (-?\d+)/) || [])[1] : null;
const woke = find(hostLog, /^killnamed: on the host: it was asleep/)[0];

if (woke) {
    number = (woke.match(/object number (-?\d+)/) || [])[1];
}

if (!madeLine) {
    wrong.push(nothing ? 'not made: ' + nothing : "the host's log never looked for it");
}

const title = madeLine ? madeLine.slice(madeLine.lastIndexOf(', ') + 2) : '-';
const fromItsPlace = madeLine ? (madeLine.match(/(\d+) mm from where/) || [])[1] : '-';

// Seen on the joiner: a ghost with the same number
const ghostLines = find(joinLog, /^killnamed: a ghost on the joiner: object number /);
const seen = number !== null && ghostLines.some(l => (l.match(/object number (-?\d+)/) || [])[1] === number);

if (madeLine && !seen) {
    wrong.push('no ghost on the joiner with object number ' + number + ', of ' + ghostLines.length + ' ghosts of that kind it saw');
}

// Brought over, shot at and dead
const brought = find(hostLog, /^killnamed: on the host: brought it/).length > 0;
const shooting = killer === 'host'
    ? find(hostLog, /^killnamed: the host is shooting at it/).length > 0
    : find(joinLog, /^killnamed: the joiner is shooting at object number /).some(l => (l.match(/object number (-?\d+)/) || [])[1] === number);
const died = find(hostLog, /^killnamed: on the host it has died/).length > 0;
const healthLines = find(hostLog, /^killnamed: on the host: health /).map(l => parseInt((l.match(/health (-?\d+)/) || [])[1], 10));
const healthFirst = healthLines.length ? healthLines[0] : '-';
const healthLast = healthLines.length ? healthLines[healthLines.length - 1] : '-';
const levelRestarted = find(hostLog, /^campaign: the level is starting again/).length > 0;
const goneNotDead = find(hostLog, /^killnamed: on the host it has gone from the list/).length > 0;

// What the joiner's game did with its ghost, once it was brought near
const ghostLines2 = find(joinLog, /^killnamed: on the joiner: object number /).filter(l => (l.match(/object number (-?\d+)/) || [])[1] === number);
const near = ghostLines2.filter(l => parseInt((l.match(/(\d+) mm from me/) || [])[1], 10) <= 15000);
const drawnYes = near.filter(l => /drawn here yes/.test(l)).length;
const modelYes = near.filter(l => /has a model yes/.test(l)).length;
const inViewYes = near.filter(l => /in view yes/.test(l)).length;
const drawnWords = near.length ? 'near joiner for ' + near.length + ' s, drawn ' + drawnYes + ', has a model ' + modelYes + ', in view ' + inViewYes : 'never near the joiner';

if (madeLine && !brought) {
    wrong.push('it was never brought to the killer');
}

if (madeLine && !shooting) {
    wrong.push('the ' + killer + ' never shot at it');
}

if (madeLine && !died) {
    wrong.push('it did not die: health went from ' + healthFirst + ' to ' + healthLast + (goneNotDead ? ', and it went off the list with no death' : '') + (levelRestarted ? ', and the level started again' : ''));
}

if (madeLine && near.length && drawnYes === 0) {
    wrong.push('the joiner had the ghost near and never drew it');
}

// The level moved on: its own target, on each machine. The host marks it where it happened, and the joiner hears of it.
const hostEvent = levelName !== null && find(hostLog, new RegExp('^level event: ' + levelName + ' happened on this machine')).length > 0;
const joinEventLine = levelName !== null ? find(joinLog, new RegExp('^level event: ' + levelName + ' set off here'))[0] : undefined;
const joinEvent = !!joinEventLine;
const joinTargetHere = joinEvent && /its target is here/.test(joinEventLine);

if (died && !hostEvent) {
    wrong.push('it died, and the host logged no level event for it');
}

if (died && !joinEvent) {
    wrong.push('it died, and the joiner never heard the level event');
}

if (joinEvent && !joinTargetHere) {
    wrong.push("the joiner heard it, and its target isn't on that machine");
}

// A door it should open, on each machine
const doors = opens && opens !== '-' ? opens.split(',') : [];
const doorsMoved = (log) => doors.filter(d => find(log, new RegExp('^walk: door .*' + d.trim().replace(/[.*+?^${}()|[\]\\]/g, '\\$&') + '.* can be gone through now')).length > 0).length;
const hostDoors = doorsMoved(hostLog);
const joinDoors = doorsMoved(joinLog);

if (died && doors.length && hostDoors !== doors.length) {
    wrong.push('the host saw ' + hostDoors + ' of ' + doors.length + ' doors open: ' + doors.join(', '));
}

if (died && doors.length && joinDoors !== doors.length) {
    wrong.push('the joiner saw ' + joinDoors + ' of ' + doors.length + ' doors open: ' + doors.join(', '));
}

// What ended the level, if anything. The test's own word writes one line, and my tool ends a run with a reason of its own, so a level whose end has neither was ended by the game. The joiner shows it went on too: it starts another level, or hears the campaign is over.
const gameEnded = find(hostLog, /^net: the level's end was reached on the host/).length > 0;
const testSaidSo = find(hostLog, /^net: the test says this level's end/).length > 0;
const toolEnded = hostResult.some(l => /ended=the named NPC has been killed/.test(l));
const endedBy = testSaidSo ? 'by the test' : (gameEnded ? 'by its chain' : (toolEnded ? "didn't end, the test stopped it" : "didn't end"));
const levelsOn = (log) => find(log, /^campaign: \d+ places for players/).length;
const joinerWentOn = find(joinLog, /^net: the campaign is over/).length > 0 || levelsOn(joinLog) > 1;

// Crashes, and refusals in what should be honest play
const crashes = find(hostLog, /^crash:/).length + find(joinLog, /^crash:/).length;
const refusals = find(hostLog, /^net: refused:/).length + find(joinLog, /^net: refused:/).length;

if ((gameEnded || testSaidSo) && !joinerWentOn) {
    wrong.push('the level ended on the host, and the joiner never went on');
}

if (crashes) {
    wrong.push(crashes + ' crash lines');
}

if (refusals) {
    wrong.push(refusals + ' refusals: ' + find(hostLog.concat(joinLog), /^net: refused:/)[0]);
}

const rate = (result) => (result.find(l => /^frames_each_second_asked=/.test(l)) || 'rate not said').replace('frames_each_second_', '');

const wayOfDying = (find(joinLog, /^net: the host says an NPC has died, way of dying /)[0] || '').replace(/.*way of dying /, '') || '-';

console.log(['smoke host ' + (hostSmoke.startsWith('passed') ? 'passed' : 'FAILED') + ' joiner ' + (joinSmoke.startsWith('passed') ? 'passed' : 'FAILED'), 'made ' + (madeLine ? 'yes' : 'NO'), 'name ' + title, 'off its place ' + fromItsPlace + ' mm', woke ? 'was asleep' : 'awake', 'seen on joiner ' + (seen ? 'yes' : 'NO'), drawnWords, 'killed by ' + killer + ' ' + (died ? 'yes' : 'NO'), 'way of dying ' + wayOfDying, 'health ' + healthFirst + ' to ' + healthLast, 'chain host ' + (hostEvent ? 'yes' : 'NO'), 'chain joiner ' + (joinEvent ? (joinTargetHere ? 'yes' : 'heard, no target') : 'NO'), 'doors host ' + hostDoors + '/' + doors.length, 'doors joiner ' + joinDoors + '/' + doors.length, 'level ended ' + endedBy, 'joiner went on ' + (gameEnded || testSaidSo ? (joinerWentOn ? 'yes' : 'NO') : '-'), 'levels started host ' + levelsOn(hostLog) + ' joiner ' + levelsOn(joinLog), 'host ' + rate(hostResult), 'joiner ' + rate(joinResult)].join('\t'));

for (const w of wrong) {
    console.log('  ' + w);
}

process.exit(wrong.length ? 1 : 0);
