// Where a Hudson tool may read from: a run's folder, or a game copy's runs folder, named on the command line. Written 2026-11-09 by
// Melanikus for the AvP port, for read-level-counts.js, read-play-pair.js, ghost-compare.js, read-hostile.js and read-kills.js.
//
// A folder named on the command line comes from outside the tool, so it's held to what such a folder is: a run's folder is
// <copy>\runs\<date and time> (run-hudson.ps1 makes them so), and a copy's runs folder is <copy>\runs, both under the folder the tool
// is run from, which is the one holding the game copies. Each part is checked, and the path is made again from the folder the tool
// runs in and those checked parts alone. So nothing typed can lead a tool anywhere else: not up and out with .., not to another drive,
// and not to a folder of another kind.
'use strict';
const path = require('path');

// A game copy's folder name: letters, digits, '.', '_' and '-', not starting with a '.', so never '.' or '..'
const kCopyName = /^[A-Za-z0-9][A-Za-z0-9._-]{0,99}$/;
// A run's folder name, as run-hudson.ps1 makes it: yyyy-MM-dd-HHmmss
const kRunName = /^[0-9]{4}-[0-9]{2}-[0-9]{2}-[0-9]{6}$/;

// The parts of a folder, as a path from the folder this is run in, or null if it isn't under it
function partsOf(given) {
  if (typeof given !== 'string' || given === '' || given.includes('\0')) {
    return null;
  }

  const relative = path.relative(process.cwd(), path.resolve(process.cwd(), given));

  if (relative === '' || path.isAbsolute(relative)) {
    return null;
  }

  return relative.split(/[\\/]+/);
}

function refuse(what) {
  console.log(`${what} must be <copy>\\runs\\<date and time> (or for a whole copy, <copy>\\runs), under the folder this is run from: the one that holds the game copies`);
  process.exit(2);
}

// A run's folder: <copy>\runs\<date and time>. what says which one, in the words a refusal uses.
function runFolder(given, what) {
  const parts = partsOf(given);

  if (!parts || parts.length !== 3 || !kCopyName.test(parts[0]) || parts[1] !== 'runs' || !kRunName.test(parts[2])) {
    refuse(what);
  }

  return path.join(process.cwd(), path.basename(parts[0]), 'runs', path.basename(parts[2]));
}

// A copy's runs folder: <copy>\runs
function runsFolder(given, what) {
  const parts = partsOf(given);

  if (!parts || parts.length !== 2 || !kCopyName.test(parts[0]) || parts[1] !== 'runs') {
    refuse(what);
  }

  return path.join(process.cwd(), path.basename(parts[0]), 'runs');
}

// Whether a name read from a runs folder is a run's, as run-hudson.ps1 names them
function isRunName(name) {
  return kRunName.test(name);
}

module.exports = { runFolder, runsFolder, isRunName };
