// Names the files Snyk Code couldn't parse in one library's source.
//
// Snyk Code's report (SARIF) says how many files of each kind (".h", ".cpp" and so on) it read, and how many it couldn't parse,
// but never which ones. So this scans smaller and smaller groups of that library's files: a group with none of them is done, a
// group that is all of them is done, and any other group is cut in half and each half looked at again, until every one is named.
// The second half's count is worked out from the first's (the group's count, less the first half's), so it isn't scanned.
//
// It then checks its own answer with two more scans: the named files alone must all fail, and the library without them must
// have none left. If a file only fails next to certain others, those two scans disagree with the names, and the report says so.
//
//   node name_unparsed.js <library source folder> <work folder> <report.json> [most scans, 400 if not given]
//
// It needs the Snyk CLI on the PATH and SNYK_TOKEN set. Every group is a copy, in the work folder, of only that group's files,
// with one tiny C file of our own added, so that Snyk always has one file it can read. Nothing in the library is changed.
// Written 2026-11-09 by Secondary for #174, as a one-off probe on a throwaway branch: it runs `snyk code test` many times.
'use strict';
const fs = require('fs');
const path = require('path');
const { spawnSync } = require('child_process');

// Every regular file under top, as paths with "/" from top, in order. A link is never followed: the scan job doesn't copy
// links (each is written down where it's collected), so a link here would be a file the gate never saw.
function listFiles(top) {
  const out = [];
  const walk = (rel) => {
    for (const e of fs.readdirSync(path.join(top, rel), { withFileTypes: true })) {
      const r = rel ? rel + '/' + e.name : e.name;
      if (e.isSymbolicLink()) continue;
      if (e.isDirectory()) walk(r);
      else if (e.isFile()) out.push(r);
    }
  };
  walk('');
  return out.sort();
}

// A file's kind, the way Snyk's report names it: the end of its name from the last dot, in small letters
const kindOf = (name) => {
  const m = /\.[^./]+$/.exec(name);
  return m ? m[0].toLowerCase() : '';
};

// From a report: how many files of a kind Snyk counted at all, and how many of them it didn't read (any type but SUPPORTED)
const coverageOf = (sarif) => {
  const runs = sarif && sarif.runs;
  const cov = runs && runs[0] && runs[0].properties && runs[0].properties.coverage;
  if (!Array.isArray(cov)) throw new Error('the report has no coverage list');
  return cov;
};
const counted = (cov, kind) => cov.filter((c) => String(c.lang).toLowerCase() === kind).reduce((n, c) => n + c.files, 0);
const notRead = (cov, kind) =>
  cov.filter((c) => c.type !== 'SUPPORTED' && (kind === null || String(c.lang).toLowerCase() === kind)).reduce((n, c) => n + c.files, 0);

// Our own file in every group, which any C reader can read, so a group is never left with nothing Snyk can read
const SENTINEL = 'avpx-probe-sentinel/sentinel.c';
const SENTINEL_TEXT = '/* Added by the probe, not part of the library. */\nint avpx_probe_sentinel(void)\n{\n\treturn 0;\n}\n';

// The real scan: snyk code test on one folder, retried like the gate retries it (exit 1 is findings, which is fine here: only
// the coverage is read; exit 2 or more is an error). Returns the parsed report.
function snykScan(dir, sarifPath) {
  for (let attempt = 1; attempt <= 3; attempt++) {
    fs.rmSync(sarifPath, { force: true });
    const r = spawnSync('snyk', ['code', 'test', dir, '--sarif-file-output=' + sarifPath], { stdio: ['ignore', 'ignore', 'inherit'] });
    if ((r.status === 0 || r.status === 1) && fs.existsSync(sarifPath)) return JSON.parse(fs.readFileSync(sarifPath, 'utf8'));
    console.log(`Snyk Code error (exit ${r.status}${r.error ? ', ' + r.error.message : ''}) on attempt ${attempt}`);
    if (attempt < 3) Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, 30000);
  }
  throw new Error('Snyk Code failed three times on ' + dir);
}

function nameUnparsed(top, work, options) {
  const scan = (options && options.scan) || snykScan;
  const mostScans = (options && options.mostScans) || 400;
  const notes = [];
  let scans = 0;
  let groupNumber = 0;

  // Copies the given files (and the sentinel) into a new folder, scans it, and returns its coverage
  const scanCopy = (files, withSentinel) => {
    if (scans >= mostScans) throw new Error(`stopped at ${mostScans} scans, the most allowed`);
    scans++;
    const dir = path.join(work, 'group-' + ++groupNumber);
    for (const f of files) {
      fs.mkdirSync(path.dirname(path.join(dir, f)), { recursive: true });
      fs.copyFileSync(path.join(top, f), path.join(dir, f));
    }
    if (withSentinel) {
      fs.mkdirSync(path.join(dir, path.dirname(SENTINEL)), { recursive: true });
      fs.writeFileSync(path.join(dir, SENTINEL), SENTINEL_TEXT);
    }
    const cov = coverageOf(scan(dir, path.join(work, 'group-' + groupNumber + '.sarif')));
    fs.rmSync(dir, { recursive: true, force: true });
    return cov;
  };

  // A file goes unread in one of two ways: Snyk counts it but doesn't read it (FAILED_PARSING, say), or Snyk leaves it out of its
  // count altogether (the gate catches that by counting the files itself). For a group of files of one kind, scanned on their own,
  // this gives both: notRead, and missing (how many fewer Snyk counted than are there). Snyk must also read the sentinel, or the
  // scan read nothing and the probe stops.
  const unreadIn = (files, kind) => {
    const cov = scanCopy(files, true);
    const expected = files.length + (kind === kindOf(SENTINEL) ? 1 : 0);
    if (cov.filter((c) => c.type === 'SUPPORTED' && String(c.lang).toLowerCase() === kindOf(SENTINEL)).reduce((n, c) => n + c.files, 0) < 1)
      throw new Error('Snyk did not read the sentinel, so the scan read nothing');
    const missing = expected - counted(cov, kind);
    if (missing < 0) throw new Error(`Snyk counted ${counted(cov, kind)} ${kind} files in a group of ${expected}`);
    return { notRead: notRead(cov, kind), missing };
  };

  // The halving: files is a group of one kind, and known is what unreadIn found for it. A group is done when nothing in it is
  // unread, or when every file in it is unread the same way. Each named file comes back with how: "not read" or "not counted".
  const halve = (files, known, kind) => {
    if (known.notRead + known.missing === 0) return [];
    if (known.notRead === files.length) return files.map((f) => ({ file: f, how: 'not read' }));
    if (known.missing === files.length) return files.map((f) => ({ file: f, how: 'not counted' }));
    if (files.length === 1) throw new Error(`one file, ${files[0]}, with counts that can't be: ${JSON.stringify(known)}`);
    const mid = Math.ceil(files.length / 2);
    const first = files.slice(0, mid);
    const second = files.slice(mid);
    const inFirst = unreadIn(first, kind);
    let inSecond = { notRead: known.notRead - inFirst.notRead, missing: known.missing - inFirst.missing };
    const fits = (n) => n >= 0 && n <= second.length;
    if (!fits(inSecond.notRead) || !fits(inSecond.missing) || !fits(inSecond.notRead + inSecond.missing)) {
      notes.push(`${kind}: the halves don't add up (${JSON.stringify(known)} in ${files[0]} .. ${files[files.length - 1]}, ${JSON.stringify(inFirst)} in its first half), so the second half was scanned too`);
      inSecond = unreadIn(second, kind);
    }
    return halve(first, inFirst, kind).concat(halve(second, inSecond, kind));
  };

  const cKinds = /^\.(c|cc|cpp|cxx|c\+\+|h|hh|hpp|hxx|h\+\+|inl|ipp|tpp|inc)$/;
  const all = listFiles(top);
  const full = scanCopy(all, false);
  // The kinds to look at: any Snyk didn't read some of, and any C or C++ kind it counted fewer of than there are
  const lookAt = new Set(full.filter((c) => c.type !== 'SUPPORTED').map((c) => String(c.lang).toLowerCase()));
  for (const f of all) if (cKinds.test(kindOf(f))) lookAt.add(kindOf(f));
  const kinds = {};
  for (const kind of [...lookAt].sort()) {
    const files = all.filter((f) => kindOf(f) === kind);
    const inFull = { notRead: notRead(full, kind), missing: files.length - counted(full, kind) };
    if (inFull.missing < 0) throw new Error(`Snyk counted ${counted(full, kind)} ${kind} files in the whole library, which has ${files.length}`);
    if (inFull.notRead + inFull.missing === 0) continue;
    // The whole group on its own first, so every count the halving uses was made the same way
    const alone = unreadIn(files, kind);
    if (JSON.stringify(alone) !== JSON.stringify(inFull)) notes.push(`${kind}: ${JSON.stringify(inFull)} in the whole library, but ${JSON.stringify(alone)} when only the ${kind} files are scanned`);
    kinds[kind] = { files: files.length, inWholeLibrary: inFull, onTheirOwn: alone, named: halve(files, alone, kind) };
  }

  // The checks of the answer, if anything was unread. The library without the named files must have nothing unread (even when
  // nothing was named: a file that fails only beside others is never named, and this is where it shows), the named files on their
  // own must all be unread, and each kind's count on its own must match the whole library's.
  const named = Object.values(kinds).flatMap((k) => k.named.map((n) => n.file));
  const unreadAll = (cov, files) => notRead(cov, null) + [...new Set(files.map(kindOf))].filter((k) => cKinds.test(k)).reduce((n, k) => n + files.filter((f) => kindOf(f) === k).length + (k === kindOf(SENTINEL) ? 1 : 0) - counted(cov, k), 0);
  const verified = { namedCount: named.length, namedAloneUnread: null, restUnread: null, ok: true };
  if (Object.keys(kinds).length) {
    const rest = all.filter((f) => !named.includes(f));
    verified.restUnread = unreadAll(scanCopy(rest, true), rest);
    if (named.length) verified.namedAloneUnread = unreadAll(scanCopy(named, true), named);
    const kindsMatch = Object.values(kinds).every((k) => JSON.stringify(k.onTheirOwn) === JSON.stringify(k.inWholeLibrary));
    verified.ok = kindsMatch && verified.restUnread === 0 && (named.length === 0 || verified.namedAloneUnread === named.length);
  }
  return { wholeLibrary: full, kinds, scans, notes, verified };
}

function main() {
  const [top, work, reportPath, most] = process.argv.slice(2);
  if (!top || !work || !reportPath) {
    console.error('usage: node name_unparsed.js <library source folder> <work folder> <report.json> [most scans]');
    process.exit(2);
  }
  if (most !== undefined && !/^[1-9][0-9]{0,3}$/.test(most)) {
    console.error('most scans must be a number from 1 to 9999');
    process.exit(2);
  }
  fs.mkdirSync(work, { recursive: true });
  const report = Object.assign({ library: top }, nameUnparsed(top, work, { mostScans: most ? Number(most) : 400 }));
  fs.writeFileSync(reportPath, JSON.stringify(report, null, 2) + '\n');
  const lines = [`### ${top}: ${report.scans} scans`];
  for (const [kind, k] of Object.entries(report.kinds)) {
    lines.push(`- ${kind}: ${k.named.length} named of ${k.files} (in the whole library: ${k.inWholeLibrary.notRead} not read, ${k.inWholeLibrary.missing} not counted)`);
    for (const n of k.named) lines.push('  - `' + n.file + '` (' + n.how + ')');
  }
  for (const n of report.notes) lines.push('- note: ' + n);
  lines.push(`- check of the answer: ${report.verified.ok ? 'RIGHT' : 'WRONG'} (${JSON.stringify(report.verified)})`);
  console.log(lines.join('\n'));
  if (process.env.GITHUB_STEP_SUMMARY) fs.appendFileSync(process.env.GITHUB_STEP_SUMMARY, lines.join('\n') + '\n\n');
  process.exit(report.verified.ok ? 0 : 1);
}

module.exports = { nameUnparsed, listFiles, kindOf };
if (require.main === module) main();
