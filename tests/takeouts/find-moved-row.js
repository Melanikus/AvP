// Finds where a take-out row's text went, when the code it breaks has been changed or moved. Written 2026-11-07 by Melanikus for the AvP port.
//
// Use it when the Take-outs check says a row can't find its text:
//
//   node tests/takeouts/find-moved-row.js <list> <row>            shows the places that look most like the old text
//   node tests/takeouts/find-moved-row.js <list> <row> --write    and puts the best one in the row, for you to check
//
// <list> is the list's name, which is the test's name (for example npc_crashes_test). <row> is the row's number,
// counted from 0, the way the check's log gives it.
//
// HOW IT CHOOSES: it cuts the old text and the file into words (names, numbers and single marks like ; or {), and
// gives each place in the file, of the same number of lines, a score: how many words it shares with the old text,
// out of the larger count of the two. The best places come first.
//
// IT NEVER DECIDES FOR YOU. A changed line can look like the old one and do something else, and the row's "to" was
// written for the old text. After --write, read the place in the file, check that "to" still breaks what the row's
// name says, look at the change with git diff, and run the row (tests/takeouts/run.ps1 <list> -From <row> -To <row>).
//
// A list can come from anyone's pull request, so what it says is checked before it's used: the list's name has to be
// letters, digits and underscores, and every file a row names has to be inside the repository.

'use strict';

const fs = require('fs');
const path = require('path');

const repository = path.resolve(__dirname, '..', '..');

function stop(words) {
	console.error(words);
	process.exit(2);
}

// --- What was asked for ---

const args = process.argv.slice(2);
const write = args.includes('--write');
const plain = args.filter((a) => a !== '--write');

if (plain.length !== 2) {
	stop('usage: node tests/takeouts/find-moved-row.js <list> <row> [--write]');
}

const [listName, rowText] = plain;

if (!/^[A-Za-z0-9_]+$/.test(listName)) {
	stop(`A list's name is letters, digits and underscores only, the way a test is named, and "${listName}" isn't`);
}

if (!/^[0-9]+$/.test(rowText)) {
	stop(`A row is a number counted from 0, and "${rowText}" isn't`);
}

const listFile = path.join(repository, 'tests', 'takeouts', `${listName}.json`);

if (!fs.existsSync(listFile)) {
	stop(`There's no list called ${listName}: no ${path.relative(repository, listFile)}`);
}

// The list is read as it is on disk, line ends and all, so that --write can change one string and leave the rest
const listOnDisk = fs.readFileSync(listFile, 'utf8');
let list;

try {
	list = JSON.parse(listOnDisk);
} catch (error) {
	stop(`${path.relative(repository, listFile)} isn't readable as JSON: ${error.message}`);
}

const rows = Array.isArray(list.rows) ? list.rows : [];
const rowNumber = Number(rowText);

if (rowNumber >= rows.length) {
	stop(`${listName} has ${rows.length} rows, numbered 0 to ${rows.length - 1}, so there's no row ${rowNumber}`);
}

const row = rows[rowNumber];
const changes = Array.isArray(row.changes) ? row.changes : [];

// --- Reading the files a row names ---

// A file a row names has to be a relative path inside the repository, so a list can't point this anywhere else
function readNamedFile(name) {
	if (typeof name !== 'string' || name === '' || path.isAbsolute(name) || name.includes(':') || name.split(/[\\/]/).includes('..')) {
		return { problem: `it names "${String(name)}", which isn't a path inside the repository` };
	}

	const whole = path.resolve(repository, name);

	if (!whole.startsWith(repository + path.sep) || !fs.existsSync(whole) || !fs.statSync(whole).isFile()) {
		return { problem: `it names ${name}, and there's no such file` };
	}

	// latin1 keeps every byte as it is, whatever the file holds. Windows line ends are read as plain ones, the way the
	// runner reads them.
	return { text: fs.readFileSync(whole, 'latin1').replace(/\r\n/g, '\n') };
}

function timesFound(text, part) {
	let count = 0;

	for (let at = text.indexOf(part); at >= 0; at = text.indexOf(part, at + 1)) {
		count++;
	}

	return count;
}

// --- Scoring a place against the old text ---

function wordsOf(text) {
	return text.match(/[A-Za-z_][A-Za-z0-9_]*|[0-9]+|\S/g) || [];
}

// How many words two lists share, counting a word as often as it's in both
function wordsShared(a, b) {
	const counts = new Map();
	let shared = 0;

	for (const word of a) {
		counts.set(word, (counts.get(word) || 0) + 1);
	}

	for (const word of b) {
		const left = counts.get(word) || 0;

		if (left > 0) {
			shared++;
			counts.set(word, left - 1);
		}
	}

	return shared;
}

// Where on a line a place starts: at the old text's first word, unless the old text started with the line's own
// spaces or tabs, when it starts at the line's beginning.
function startLike(oldLine, line) {
	const words = wordsOf(oldLine);

	if (/^\s/.test(oldLine) || words.length === 0) {
		return 0;
	}

	const at = line.indexOf(words[0]);
	return at > 0 ? at : 0;
}

// Where on a line a place stops: just after the old text's last word, or at the line's end if that word isn't there
function endLike(oldLine, line) {
	const words = wordsOf(oldLine);

	if (words.length === 0) {
		return line.length;
	}

	const lastWord = words[words.length - 1];
	const at = line.lastIndexOf(lastWord);
	return at >= 0 ? at + lastWord.length : line.length;
}

// A row's text can start partway along its first line and stop partway along its last. The lines between are always
// whole lines, so they're taken as the file has them. The first and last lines are cut the way the old text was: kept
// as the old text had them when the file's line still starts or ends that way, and cut at the old words if not.
function shapeLikeTheOld(oldLines, placeLines) {
	const shaped = placeLines.slice();
	const last = shaped.length - 1;
	const firstOld = oldLines[0];
	const lastOld = oldLines[last];

	if (last === 0) {
		const start = startLike(firstOld, shaped[0]);
		const end = endLike(firstOld, shaped[0]);
		return end > start ? shaped[0].slice(start, end) : shaped[0];
	}

	shaped[0] = shaped[0].endsWith(firstOld) ? firstOld : shaped[0].slice(startLike(firstOld, shaped[0]));
	shaped[last] = shaped[last].startsWith(lastOld) ? lastOld : shaped[last].slice(0, endLike(lastOld, shaped[last]));
	return shaped.join('\n');
}

function nearestPlaces(fileText, oldText, howMany) {
	const fileLines = fileText.split('\n');
	const oldLines = oldText.split('\n');
	const oldWords = wordsOf(oldText);
	const places = [];

	for (let first = 0; first + oldLines.length <= fileLines.length; first++) {
		// Scored as it would go in the row, cut the way the old text was, so the rest of a long last line doesn't count
		const text = shapeLikeTheOld(oldLines, fileLines.slice(first, first + oldLines.length));
		const placeWords = wordsOf(text);
		const larger = Math.max(oldWords.length, placeWords.length);

		if (larger === 0) {
			continue;
		}

		const shared = wordsShared(oldWords, placeWords);

		if (shared > 0) {
			places.push({ line: first + 1, shared, larger, score: shared / larger, text });
		}
	}

	places.sort((a, b) => b.score - a.score || a.line - b.line);
	return places.slice(0, howMany);
}

// Text from a list is shown with a margin, so none of its lines can start a line of this tool's own words
function showWithMargin(text) {
	return text.split('\n').map((line) => `      | ${line}`).join('\n');
}

// --- The row ---

console.log(`${listName}, row ${rowNumber}: "${String(row.name).replace(/[\r\n]+/g, ' / ')}"`);

let proposedRow = null;
let written = 0;

changes.forEach((change, index) => {
	const which = changes.length > 1 ? `change ${index + 1} of ${changes.length}` : 'its change';
	const file = readNamedFile(change.file);

	if (file.problem) {
		console.log(`  ${which}: ${file.problem}. Put the right file in the row by hand.`);
		return;
	}

	const oldText = typeof change.from === 'string' ? change.from : '';
	const found = oldText === '' ? 0 : timesFound(file.text, oldText);

	if (found === 1) {
		console.log(`  ${which}, in ${change.file}: its text is there once, as it should be. Nothing to do.`);
		return;
	}

	console.log(found === 0
		? `  ${which}, in ${change.file}: its text isn't there. It looks for:`
		: `  ${which}, in ${change.file}: its text is there ${found} times, and has to be there once. It looks for:`);
	console.log(showWithMargin(oldText));

	if (found > 1) {
		console.log('    Make "from" longer by hand, with the lines around the one you mean, until it matches only there.');
		return;
	}

	const places = nearestPlaces(file.text, oldText, 3);

	if (places.length === 0) {
		console.log('    Nothing in the file shares a word with it. The code it broke may be in another file now, or gone.');
		return;
	}

	console.log('    The places most like it, best first:');
	places.forEach((place, rank) => {
		console.log(`    ${rank + 1}. from line ${place.line}, sharing ${place.shared} of ${place.larger} words:`);
		console.log(showWithMargin(place.text));
	});

	const best = places[0];

	if (timesFound(file.text, best.text) !== 1) {
		console.log('    The best place\'s text is in the file more than once, so it can\'t go in the row as it is. Make it longer by hand.');
		return;
	}

	console.log('    The row\'s "to" was written for the old text. Check it still breaks what the row\'s name says.');

	if (!write) {
		return;
	}

	// The change is made in the list's own text, so its layout stays as it is: the old "from", written the way JSON
	// writes it, has to be in the list exactly once, or nothing is written.
	const oldInList = `"from": ${JSON.stringify(oldText)}`;
	const listText = proposedRow === null ? listOnDisk : proposedRow;
	const lineEnd = listText.includes('\r\n') ? '\r\n' : '\n';
	const flat = listText.replace(/\r\n/g, '\n');

	if (timesFound(flat, oldInList) !== 1) {
		console.log(`    Not written: the old "from" isn't in ${listName}.json exactly once in that form. Change the row by hand.`);
		return;
	}

	proposedRow = flat.replace(oldInList, () => `"from": ${JSON.stringify(best.text)}`).replace(/\n/g, lineEnd);
	written++;
});

if (write && written > 0) {
	// Read it back before it's saved: the row has to hold the new text, and the list has to still be JSON
	const check = JSON.parse(proposedRow);

	if (!Array.isArray(check.rows) || check.rows.length !== rows.length) {
		stop('Not written: the list didn\'t read back the same length. Change the row by hand.');
	}

	fs.writeFileSync(listFile, proposedRow, 'utf8');
	console.log(`Written: ${written} change(s) in ${path.relative(repository, listFile)}. Look at it with git diff, and run the row before you commit it.`);
} else if (write) {
	console.log('Nothing written.');
}
