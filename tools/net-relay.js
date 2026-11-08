'use strict';
// A pretend long-distance network, for testing two copies of the game on one machine. Written 2026-11-07 by Melanikus for the AvP port.
// Modified 2026-11-08 by Melanikus for the AvP port (from Secondary's read of it, AvP-docs review-net-relay-secondary.md):
//   each packet's fate comes from its own random numbers, so a seed gives the same conditions whatever the other direction does;
//   packets leave in the order they came unless --reorder holds one back, so jitter alone no longer reorders; the settings and the
//   seed are written as it starts (RELAY-LOG-1), so a run that's killed still has them; the summary gives the real hold times beside
//   the ones asked for; the summary and stop files are named by the relay from a run number, never taken as paths, and the summary has
//   to be a new file; an error after the joiner's side is open is logged, not the end of the run; and --self-test checks all of this
//   with no socket opened
//
// WHAT IT'S FOR
//
// Two copies of the game on one machine talk to each other in no time at all, and nothing is ever lost. Real players are on different
// continents: a packet takes 100 ms or more to arrive, the time varies, some packets never arrive, a few come twice or in the wrong order.
// This sits between the two copies and does all of that to their packets, on purpose, so we can see how the game copes.
//
// HOW IT SITS BETWEEN THEM
//
// The host listens on 127.0.0.1, as a test run always does. This listens on another of the machine's own addresses, 127.0.0.2 by default, on
// the same port numbers. The joiner is told to look for the game at 127.0.0.2, so it finds this, and this passes everything on to the host
// from 127.0.0.1 and passes the host's answers back. The game's messages are never read or changed, only held back, dropped or repeated.
// It takes the same ports as the host because a found game's answer names the port to join, and that's the host's own.
//
// It only ever uses addresses that start with 127, the machine talking to itself, so nothing goes to any network. It refuses any other.
// It starts no other program, and writes nothing but its own summary.
//
// HOW TO RUN IT
//
//   node tools/net-relay.js --delay 150 --jitter 30 --loss 2 --burst 20 --seconds 300 --run 1
//
// Every setting is for each direction, and a setting can be given for one direction only with "up-" (joiner to host) or "down-" (host to
// joiner) in front of it, such as --down-delay 120. Times are in milliseconds and chances in percent:
//   --delay     how long each packet is held, one way. 100 is Brazil to the USA, 150 Brazil to Europe, 250 Brazil to Japan.
//   --jitter    each packet's hold is moved by up to this much, either way, at random. Packets still leave in the order they came, as
//               on a real path: one drawn shorter than the packet before it waits for that one. So jitter doesn't reorder; --reorder
//               does. A hold can't be below 0, so with jitter larger than delay the average comes out above --delay.
//   --loss      the chance a packet is dropped
//   --burst     the chance that a drop is the start of a burst: 2 to 4 more packets after it are dropped too, so 3 to 5 in all. So with
//               bursts on, more is lost than --loss says (2% with 20% bursts loses about 3.2%): read the summary's "dropped", not --loss.
//   --reorder   the chance a packet is held back long enough to arrive after the next ones (as long again as twice the jitter, and 20 ms)
//   --dupe      the chance a packet is sent twice. The copy follows 1 to 11 ms after the time the first would leave in order, so a
//               copy of a packet --reorder holds back arrives before it.
//   --drop-first  how many of the first packets on the game port are dropped for certain. --down-drop-first 1 loses the host's first
//               answer to a joiner, which carries ENet's cookie, so it shows a joiner still gets in when that answer is lost
//   --seed      where the random numbers start. Each packet has its own: they come from the seed, its port, its direction and its place
//               in that line (the 1st, the 2nd...). So under one seed the Nth packet from joiner to host always meets the same fate,
//               whatever the host sends. A run still differs if the game sends different packets, but the conditions are the same.
//   --seconds   it stops after this long, and writes its summary. It also stops on Ctrl+C, and when its input closes: so a program that
//               starts it with no input (or input from NUL) has to keep its input open, or it stops at once.
//   --listen    the address the joiner is given (127.0.0.2), --to the host's (127.0.0.1), --ports the ports (23513,23514)
//   --run       a run number, 1 to 999999. With it, the relay keeps its settings and summary in net-relay-run-N.txt, in the folder it's
//               started in, and stops (writing its summary) as soon as a file net-relay-run-N.stop appears there. Another program can't
//               always send Ctrl+C on Windows, so that's how a run that started it stops it. The summary file has to be new: if one is
//               there already, the relay refuses to start, so it never writes over anything it didn't make. The settings go in it as soon
//               as it starts, and the counts are added at the end. The relay makes both names itself from the number: it never takes a
//               file's name or folder from its settings.
//   --self-test  checks the parts that decide each packet's fate, with no socket opened, and says ok or FAILED for each (no other setting)

const dgram = require('dgram');
const fs = require('fs');
const { performance } = require('perf_hooks');

// ---- The settings, checked before anything opens

// Only the machine's own addresses: four numbers, each 0 to 255, the first 127
function IsLoopback(address)
{
	const parts = /^(\d{1,3})\.(\d{1,3})\.(\d{1,3})\.(\d{1,3})$/.exec(address);

	return parts !== null && parts.slice(1).every((part) => Number(part) <= 255) && Number(parts[1]) === 127;
}

// A setting this won't take. It's thrown, so the self-test can see the refusal; when the relay runs, it's said and the relay stops with
// code 2 before anything is opened.
class Refusal extends Error {}

function Refuse(why)
{
	throw new Refusal(why);
}

const kDirections = ['up', 'down'];
const kChances = ['loss', 'burst', 'reorder', 'dupe'];
const kTimes = ['delay', 'jitter'];
const kCounts = ['drop-first'];

function ReadSettings(words)
{
	const settings = { listen: '127.0.0.2', to: '127.0.0.1', ports: [23513, 23514], seconds: 0, seed: 1, run: 0, up: {}, down: {} };

	for (const direction of kDirections)
	{
		for (const name of kTimes.concat(kChances, kCounts))
		{
			settings[direction][name] = 0;
		}
	}

	for (let i = 0; i < words.length; i++)
	{
		const word = words[i];
		const value = words[i + 1];

		if (!/^--[a-z-]+$/.test(word) || value === undefined)
		{
			Refuse(`"${word}" isn't a setting with a value after it`);
		}

		i++;

		const name = word.slice(2);
		const plain = name.replace(/^(up|down)-/, '');
		const directions = name.startsWith('up-') ? ['up'] : name.startsWith('down-') ? ['down'] : kDirections;

		if (kTimes.includes(plain) || kChances.includes(plain) || kCounts.includes(plain))
		{
			const number = Number(value);
			const most = kTimes.includes(plain) ? 10000 : 100;
			const shape = kCounts.includes(plain) ? /^\d{1,3}$/ : /^\d+(\.\d+)?$/;

			if (!shape.test(value) || number > most)
			{
				Refuse(`--${name} has to be a ${kCounts.includes(plain) ? 'whole ' : ''}number from 0 to ${most}`);
			}

			for (const direction of directions)
			{
				settings[direction][plain] = number;
			}
		}
		else if (name === 'listen' || name === 'to')
		{
			if (!IsLoopback(value))
			{
				Refuse(`--${name} has to be one of this machine's own addresses, starting with 127`);
			}

			settings[name] = value;
		}
		else if (name === 'ports')
		{
			const ports = value.split(',').map(Number);

			if (ports.length < 1 || ports.length > 4 || !ports.every((port) => Number.isInteger(port) && port >= 1024 && port <= 65535))
			{
				Refuse('--ports has to be 1 to 4 port numbers from 1024 to 65535, with commas between');
			}

			settings.ports = ports;
		}
		else if (name === 'seconds' || name === 'seed')
		{
			if (!/^\d{1,9}$/.test(value))
			{
				Refuse(`--${name} has to be a whole number`);
			}

			settings[name] = Number(value);
		}
		else if (name === 'run')
		{
			if (!/^\d{1,6}$/.test(value) || Number(value) < 1)
			{
				Refuse('--run has to be a whole number from 1 to 999999');
			}

			settings.run = Number(value);
		}
		else
		{
			Refuse(`"--${name}" isn't one of its settings`);
		}
	}

	if (settings.listen === settings.to)
	{
		Refuse('--listen and --to have to be different addresses, or it would take the host\'s own ports');
	}

	return settings;
}

// ---- Each packet's own random numbers

// Stirs a 32-bit number so that numbers close together come out far apart (the last step of MurmurHash3). Not for anything secret.
function Stir(x)
{
	x = Math.imul(x ^ (x >>> 16), 0x85ebca6b);
	x = Math.imul(x ^ (x >>> 13), 0xc2b2ae35);
	return (x ^ (x >>> 16)) >>> 0;
}

// The random numbers for one packet: a stream that starts from the seed, the port, the direction and the packet's place in its line.
// Nothing else goes into it, so nothing that happens on another line, or the other way, can change what this packet meets.
function PacketRandom(seed, port, direction, number)
{
	let state = Stir(Stir(Stir(Stir((seed >>> 0) ^ 0x9e3779b9) ^ (port >>> 0)) ^ (direction === 'up' ? 1 : 2)) ^ (number >>> 0)) || 1;

	return function Random()
	{
		// mulberry32: a small, well-known generator
		state = (state + 0x6D2B79F5) >>> 0;
		let t = state;
		t = Math.imul(t ^ (t >>> 15), t | 1);
		t ^= t + Math.imul(t ^ (t >>> 7), t | 61);
		return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
	};
}

// ---- What happens to one packet, going one way

// Each direction of each port is a line: it keeps its own burst, its own count of packets, and when its last packet in order leaves
function NewLine()
{
	return { burstLeft: 0, nextNumber: 0, lastLeave: -Infinity, highestSent: -1 };
}

// Decides one packet's fate, from its own random numbers and its line's burst. It gives back why it was dropped ('first', 'burst' or
// 'loss'), or null and three times in milliseconds: hold, how long it's held in the ordinary way; back, how much longer --reorder holds it
// (0 for most); and copyGap, how long after its ordinary time a second copy follows (0 for no copy). It changes nothing but the line's
// burst.
function Fate(how, line, number, isGamePort, random)
{
	const chance = (percent) => random() * 100 < percent;

	// The first packets on the game port, dropped for certain: so a test knows the first answer to a joiner, the one with ENet's cookie in
	// it, never arrives, and the joiner has to ask again
	if (isGamePort && number < how['drop-first'])
	{
		return { dropped: 'first' };
	}

	if (line.burstLeft > 0)
	{
		line.burstLeft--;
		return { dropped: 'burst' };
	}

	if (chance(how.loss))
	{
		const startsBurst = chance(how.burst);

		if (startsBurst)
		{
			// This one and 2 to 4 more: 3 to 5 in all
			line.burstLeft = 2 + Math.floor(random() * 3);
		}

		return { dropped: 'loss', startsBurst };
	}

	// Held for the delay, moved by up to the jitter either way, and never below 0
	const hold = Math.max(0, how.delay + (random() * 2 - 1) * how.jitter);

	// Held back past the ones behind it: as long again as twice the jitter, and a little more
	const back = chance(how.reorder) ? 2 * how.jitter + 20 : 0;

	// A copy follows a moment after the packet's ordinary time
	const copyGap = chance(how.dupe) ? 1 + random() * 10 : 0;

	return { dropped: null, hold, back, copyGap };
}

// A packet's ordinary time to leave, in this machine's milliseconds: when its hold is up, but no earlier than the packet before it on its
// line. So packets come out in the order they went in. A packet --reorder holds back still takes its place in the order (the next ones
// aren't kept waiting for it), and then leaves later.
function OrdinaryLeave(line, nowMs, hold)
{
	const leave = Math.max(nowMs + hold, line.lastLeave);
	line.lastLeave = leave;

	return leave;
}

// The run's two files, named from the run number alone, in the folder the relay was started in. No run number, no files.
function RunFiles(run)
{
	if (!Number.isInteger(run) || run < 1 || run > 999999)
	{
		return { summaryFile: '', stopFile: '' };
	}

	return { summaryFile: `net-relay-run-${run}.txt`, stopFile: `net-relay-run-${run}.stop` };
}

// ---- The relay itself: the counts, the sockets and the summary

function RunRelay(settings)
{
	const counts = {};
	const lines = {};
	const sockets = [];
	let strangers = 0;
	let laterErrors = 0;

	function CountsFor(port, direction)
	{
		const key = `${port} ${direction}`;

		if (!counts[key])
		{
			counts[key] = { arrived: 0, sent: 0, dropped: 0, droppedFirst: 0, inBursts: 0, duplicated: 0, reordered: 0,
				asked: 0, askedLeast: Infinity, askedMost: 0, askedAll: 0, realLeast: Infinity, realMost: 0, realAll: 0 };
		}

		return counts[key];
	}

	function LineFor(port, direction)
	{
		const key = `${port} ${direction}`;

		if (!lines[key])
		{
			lines[key] = NewLine();
		}

		return lines[key];
	}

	// Holds, drops or repeats a packet, then sends it with send(packet). Nothing in the packet is read or changed.
	function Pass(port, direction, packet, send)
	{
		const arrived = performance.now();
		const count = CountsFor(port, direction);
		const line = LineFor(port, direction);
		const number = line.nextNumber++;
		const fate = Fate(settings[direction], line, number, port === settings.ports[0], PacketRandom(settings.seed, port, direction, number));

		count.arrived++;

		if (fate.dropped)
		{
			count.dropped++;
			count.droppedFirst += fate.dropped === 'first' ? 1 : 0;
			count.inBursts += fate.dropped === 'burst' || fate.startsBurst ? 1 : 0;
			return;
		}

		const ordinary = OrdinaryLeave(line, arrived, fate.hold);

		// The packet itself, at its ordinary time or held back past it; and a second copy, a moment after its ordinary time
		const leaves = [ordinary + fate.back];

		if (fate.copyGap > 0)
		{
			count.duplicated++;
			leaves.push(ordinary + fate.copyGap);
		}

		leaves.forEach((leave, copy) => {
			const asked = leave - arrived;

			count.asked++;
			count.askedLeast = Math.min(count.askedLeast, asked);
			count.askedMost = Math.max(count.askedMost, asked);
			count.askedAll += asked;

			setTimeout(() => {
				// How long it was really held. Windows wakes a program's timers in steps, so this can be later than asked.
				const real = performance.now() - arrived;

				count.realLeast = Math.min(count.realLeast, real);
				count.realMost = Math.max(count.realMost, real);
				count.realAll += real;

				if (copy === 0)
				{
					if (number < line.highestSent)
					{
						count.reordered++;
					}

					line.highestSent = Math.max(line.highestSent, number);
				}

				count.sent++;
				send(packet);
			}, Math.max(0, leave - performance.now()));
		});
	}

	// More than any test needs, so a flood of strangers can't make it open sockets without end
	const kMostConnections = 16;
	const kForgetAfterMilliseconds = 120000;

	function Open(port)
	{
		// The joiner's side: what the joiner thinks is the host
		const front = dgram.createSocket('udp4');
		const behind = new Map();
		let bound = false;

		sockets.push(front);

		front.on('message', (packet, from) => {
			const key = `${from.address}:${from.port}`;

			// Only the machine's own programs. Nothing else can reach a socket on 127, but it's checked anyway.
			if (!IsLoopback(from.address))
			{
				strangers++;
				return;
			}

			let link = behind.get(key);

			if (!link)
			{
				if (behind.size >= kMostConnections)
				{
					strangers++;
					return;
				}

				// The host's side, for this one joiner's socket: the host sees this as the joiner
				const socket = dgram.createSocket('udp4');

				link = { socket, lastUsed: Date.now() };
				behind.set(key, link);
				sockets.push(socket);

				socket.on('message', (answer, sender) => {
					// Only the host's own port is passed back
					if (sender.address !== settings.to || sender.port !== port)
					{
						strangers++;
						return;
					}

					link.lastUsed = Date.now();
					Pass(port, 'down', answer, (out) => front.send(out, from.port, from.address));
				});

				socket.on('error', (error) => process.stderr.write(`net-relay: port ${port}, the host's side: ${error.message}\n`));
				socket.bind(0, settings.to);
			}

			link.lastUsed = Date.now();
			Pass(port, 'up', packet, (out) => link.socket.send(out, port, settings.to));
		});

		// Before it's open, an error means it can't do its job, so it stops. Once it's open, an error (such as Windows saying a joiner that
		// has quit can't be reached) is said and counted, and the run goes on.
		front.on('error', (error) => {
			if (!bound)
			{
				process.stderr.write(`net-relay: can't use ${settings.listen}:${port}: ${error.message}\n`);
				process.exit(2);
			}

			laterErrors++;
			process.stderr.write(`net-relay: port ${port}, the joiner's side: ${error.message}\n`);
		});

		front.bind(port, settings.listen, () => {
			bound = true;
			process.stdout.write(`net-relay: ${settings.listen}:${port} passes to ${settings.to}:${port}\n`);
		});

		// A joiner that has gone quiet has its socket closed
		setInterval(() => {
			for (const [key, link] of behind)
			{
				if (Date.now() - link.lastUsed > kForgetAfterMilliseconds)
				{
					link.socket.close();
					behind.delete(key);
				}
			}
		}, 10000).unref();
	}

	// The settings and the seed, written as it starts, so a run that's killed rather than stopped still has a record of its conditions
	const describe = (how) => `delay ${how.delay} ms, jitter ${how.jitter} ms, loss ${how.loss}%, bursts ${how.burst}% of drops, reorder ${how.reorder}%, dupes ${how.dupe}%, the first ${how['drop-first']} on the game port dropped`;
	const start = [
		`net-relay: started ${new Date().toISOString()}, passing ${settings.listen} to ${settings.to}, ports ${settings.ports.join(',')}`,
		`  joiner to host: ${describe(settings.up)}`,
		`  host to joiner: ${describe(settings.down)}`,
		`  seed ${settings.seed}: each packet's fate comes from the seed, its port, its direction and its place in its line; packets keep their order unless --reorder holds one back`,
	];
	const startText = start.join('\n') + '\n';

	const { summaryFile, stopFile } = RunFiles(settings.run);

	if (summaryFile)
	{
		try
		{
			// 'wx': made new, or refused if it's there already
			fs.writeFileSync(summaryFile, startText, { flag: 'wx' });
		}
		catch (error)
		{
			process.stderr.write(`net-relay: ${summaryFile} can't be made new: ${error.code === 'EEXIST' ? 'it\'s there already' : error.message}\n`);
			process.exit(2);
		}
	}

	process.stdout.write(startText);

	let finished = false;

	function Finish(why)
	{
		if (finished)
		{
			return;
		}

		finished = true;

		const summary = [`net-relay: stopped: ${why}`];
		const range = (least, most, all, n) => n ? `${least.toFixed(0)} to ${most.toFixed(0)} ms, ${(all / n).toFixed(0)} on average` : 'none';

		for (const key of Object.keys(counts).sort())
		{
			const c = counts[key];
			const [port, direction] = key.split(' ');

			summary.push(`  port ${port}, ${direction === 'up' ? 'joiner to host' : 'host to joiner'}: ${c.arrived} arrived, ${c.sent} sent, ${c.dropped} dropped (${c.inBursts} in bursts, ${c.droppedFirst} of the first), ${c.duplicated} duplicated, ${c.reordered} reordered; held as asked ${range(c.askedLeast, c.askedMost, c.askedAll, c.asked)}; really ${range(c.realLeast, c.realMost, c.realAll, c.sent)}`);
		}

		summary.push(`  from anyone else: ${strangers}, refused; errors after the joiner's side was open: ${laterErrors}`);

		const text = summary.join('\n') + '\n';

		process.stdout.write(text);

		if (summaryFile)
		{
			// Its own file, made new as it started: the settings, then the counts
			fs.writeFileSync(summaryFile, startText + text);
		}

		for (const socket of sockets)
		{
			try
			{
				socket.close();
			}
			catch (error)
			{
				// Already closed
			}
		}

		process.exit(0);
	}

	for (const port of settings.ports)
	{
		Open(port);
	}

	if (settings.seconds > 0)
	{
		setTimeout(() => Finish(`${settings.seconds} seconds were up`), settings.seconds * 1000);
	}

	// Another program can't always send Ctrl+C on Windows, so a run that started this one can stop it by making a file
	if (stopFile)
	{
		setInterval(() => {
			if (fs.existsSync(stopFile))
			{
				Finish(`${stopFile} appeared`);
			}
		}, 500);
	}

	process.on('SIGINT', () => Finish('Ctrl+C'));
	process.stdin.on('end', () => Finish('its input closed'));
	process.stdin.resume();
}

// ---- The self-test: the parts that decide each packet's fate, with no socket opened

function SelfTest()
{
	let failed = 0;
	const check = (good, what) => {
		process.stdout.write(`  ${good ? 'ok' : 'FAILED'}  ${what}\n`);
		failed += good ? 0 : 1;
	};
	const draws = (random, n) => Array.from({ length: n }, () => random());
	const same = (a, b) => a.length === b.length && a.every((x, i) => x === b[i]);
	const plain = { delay: 0, jitter: 0, loss: 0, burst: 0, reorder: 0, dupe: 0, 'drop-first': 0 };

	process.stdout.write('Addresses and settings:\n');
	check(IsLoopback('127.0.0.1') && IsLoopback('127.255.255.255'), 'an address starting 127 is the machine\'s own');
	check(!IsLoopback('128.0.0.1') && !IsLoopback('10.0.0.1'), 'and one that doesn\'t isn\'t');
	check(!IsLoopback('127.0.0.256') && !IsLoopback('127.1') && !IsLoopback('127.0.0.1\n') && !IsLoopback(' 127.0.0.1'), 'nor is anything that isn\'t four numbers of 0 to 255');
	const refuses = (words) => { try { ReadSettings(words); return false; } catch (error) { return error instanceof Refusal; } };
	check(refuses(['--to', '192.168.0.1']) && refuses(['--listen', '127.0.0.1']), 'it refuses a host that isn\'t on this machine, and a listen address that\'s the host\'s');
	check(refuses(['--loss', '101']) && refuses(['--delay', '10001']) && refuses(['--ports', '80']) && refuses(['--seed', '-1']) && refuses(['--color', '1']), 'and settings out of range or unknown');
	check(ReadSettings(['--down-delay', '120']).down.delay === 120 && ReadSettings(['--down-delay', '120']).up.delay === 0, 'a setting can be for one direction only');
	check(refuses(['--run', '0']) && refuses(['--run', '1234567']) && refuses(['--run', '..\\x']) && refuses(['--run', 'C:/x']) && refuses(['--summary', 'x.txt']) && refuses(['--stop-file', 'x']),
		'a run number is a whole number from 1 to 999999, and no setting takes a file\'s name');
	const files = RunFiles(ReadSettings(['--run', '7']).run);
	check(files.summaryFile === 'net-relay-run-7.txt' && files.stopFile === 'net-relay-run-7.stop' && RunFiles(0).summaryFile === '',
		'the run\'s files are named from its number, in the folder it starts in, and without a number there are none');

	process.stdout.write('Each packet\'s own random numbers:\n');
	check(same(draws(PacketRandom(7, 23513, 'up', 40), 8), draws(PacketRandom(7, 23513, 'up', 40), 8)), 'the same seed, port, direction and place give the same numbers');
	check(!same(draws(PacketRandom(7, 23513, 'up', 40), 8), draws(PacketRandom(7, 23513, 'up', 41), 8)), 'the next packet gets other numbers');
	check(!same(draws(PacketRandom(7, 23513, 'up', 40), 8), draws(PacketRandom(7, 23513, 'down', 40), 8)), 'and so does the same place the other way');
	check(!same(draws(PacketRandom(7, 23513, 'up', 40), 8), draws(PacketRandom(7, 23514, 'up', 40), 8)), 'and on the other port');
	check(!same(draws(PacketRandom(7, 23513, 'up', 40), 8), draws(PacketRandom(8, 23513, 'up', 40), 8)), 'and under another seed');
	const mean = draws(PacketRandom(1, 23513, 'up', 0), 20000).reduce((a, b) => a + b, 0) / 20000;
	check(mean > 0.49 && mean < 0.51, 'the numbers are spread evenly between 0 and 1');

	process.stdout.write('A line\'s fates don\'t depend on the other way:\n');
	const lossy = Object.assign({}, plain, { delay: 150, jitter: 30, loss: 5, burst: 20, reorder: 2, dupe: 1 });
	const upFates = (interleave) => {
		const up = NewLine(), down = NewLine(), seen = [];
		const other = PacketRandom(99, 1, 'up', 0);

		for (let n = 0; n < 2000; n++)
		{
			// Some number of the host's packets come between each of the joiner's, a different number each way of running
			for (let k = interleave ? Math.floor(other() * 4) : 0; k > 0; k--)
			{
				Fate(lossy, down, down.nextNumber, true, PacketRandom(3, 23513, 'down', down.nextNumber++));
			}

			const fate = Fate(lossy, up, n, true, PacketRandom(3, 23513, 'up', n));
			seen.push(fate.dropped ? fate.dropped : `${fate.hold} ${fate.back} ${fate.copyGap}`);
		}

		return seen;
	};
	check(same(upFates(false), upFates(true)), 'under one seed, every one of 2000 packets from joiner to host meets the same fate, however the host\'s packets come between them');

	process.stdout.write('Drops and bursts:\n');
	const bursty = Object.assign({}, plain, { loss: 3, burst: 100 });
	const line = NewLine();
	let burst = 0, bursts = [], dropped = 0;
	for (let n = 0; n < 20000; n++)
	{
		const fate = Fate(bursty, line, n, false, PacketRandom(5, 23514, 'up', n));

		// A burst is the drop that starts it and the 'burst' drops after it. The next packet after a burst can start another at once, so
		// two bursts can touch: each is measured from its own start.
		if (fate.dropped !== 'burst' && burst > 0)
		{
			bursts.push(burst);
			burst = 0;
		}

		burst = fate.startsBurst ? 1 : fate.dropped === 'burst' ? burst + 1 : burst;
		dropped += fate.dropped ? 1 : 0;
	}
	check(bursts.length > 100 && bursts.every((b) => b >= 3 && b <= 5) && [3, 4, 5].every((b) => bursts.includes(b)), 'with every drop starting a burst, each burst is 3, 4 or 5 drops long');
	check(dropped / 20000 > 0.03 * 3 * 0.8, 'so far more is lost than --loss says: read the summary\'s dropped count');
	const first = Object.assign({}, plain, { 'drop-first': 2 });
	check(Fate(first, NewLine(), 0, true, PacketRandom(1, 1, 'up', 0)).dropped === 'first' && Fate(first, NewLine(), 1, true, PacketRandom(1, 1, 'up', 1)).dropped === 'first'
		&& Fate(first, NewLine(), 2, true, PacketRandom(1, 1, 'up', 2)).dropped === null && Fate(first, NewLine(), 0, false, PacketRandom(1, 1, 'up', 0)).dropped === null,
		'--drop-first drops the first packets on the game port, and only those');

	process.stdout.write('Holds and order:\n');
	// Sends a line of 3000 packets, one every 16.7 ms (60 a second), and gives back when each copy leaves, in the order they leave
	const sendLine = (how, seed) => {
		const orderLine = NewLine(), leaves = [];
		let heldBack = 0, copies = 0, lowest = Infinity;

		for (let n = 0; n < 3000; n++)
		{
			const fate = Fate(how, orderLine, n, false, PacketRandom(seed, 23513, 'up', n));
			const ordinary = OrdinaryLeave(orderLine, n * 16.7, fate.hold);

			lowest = Math.min(lowest, fate.hold);
			heldBack += fate.back > 0 ? 1 : 0;
			leaves.push({ n, first: true, t: ordinary + fate.back });

			if (fate.copyGap > 0)
			{
				copies++;
				leaves.push({ n, first: false, t: ordinary + fate.copyGap, gap: fate.copyGap });
			}
		}

		leaves.sort((a, b) => a.t - b.t || a.n - b.n);
		return { leaves, heldBack, copies, lowest };
	};
	// How many packets' first copies leave after a packet sent later (as the relay counts "reordered")
	const overtakenIn = (leaves) => {
		let highest = -1, overtaken = 0;

		for (const leave of leaves.filter((l) => l.first))
		{
			overtaken += leave.n < highest ? 1 : 0;
			highest = Math.max(highest, leave.n);
		}

		return overtaken;
	};
	const jittery = Object.assign({}, plain, { delay: 100, jitter: 30 });
	const steady = sendLine(jittery, 11);
	check(overtakenIn(steady.leaves) === 0, 'with jitter of 30 ms and a packet every 16.7 ms, packets still leave in the order they came');
	check(steady.lowest >= 70, 'and no packet is held less than the delay take away the jitter');
	const shaken = sendLine(Object.assign({}, jittery, { reorder: 10 }), 12);
	const overtaken = overtakenIn(shaken.leaves);
	check(shaken.heldBack > 3000 * 0.07 && shaken.heldBack < 3000 * 0.13, 'with --reorder 10, about one packet in ten is held back');
	check(overtaken <= shaken.heldBack && overtaken >= shaken.heldBack - 3,
		'and only those arrive after packets sent later (all of them but any among the last few, which nothing comes after)');
	let negative = false;
	for (let n = 0; n < 1000; n++) { negative = negative || Fate(Object.assign({}, plain, { delay: 10, jitter: 50 }), NewLine(), n, false, PacketRandom(13, 1, 'up', n)).hold < 0; }
	check(!negative, 'a hold is never below 0, even with jitter larger than the delay');
	const doubled = sendLine(Object.assign({}, jittery, { dupe: 100 }), 14);
	const gaps = doubled.leaves.filter((l) => !l.first).map((l) => l.gap);
	check(doubled.copies === 3000 && gaps.every((g) => g >= 1 && g <= 11), 'with --dupe 100 every packet has a copy, 1 to 11 ms after it');
	check(overtakenIn(doubled.leaves) === 0, 'and the copies don\'t put the packets themselves out of order');

	process.stdout.write(failed ? `SOME CHECKS FAILED (${failed})\n` : 'All checks passed\n');
	return failed ? 1 : 0;
}

// ---- Which of the two it does

if (process.argv.length === 3 && process.argv[2] === '--self-test')
{
	process.exit(SelfTest());
}

let settings;

try
{
	settings = ReadSettings(process.argv.slice(2));
}
catch (error)
{
	if (!(error instanceof Refusal))
	{
		throw error;
	}

	process.stderr.write(`net-relay: ${error.message}\n`);
	process.exit(2);
}

RunRelay(settings);
