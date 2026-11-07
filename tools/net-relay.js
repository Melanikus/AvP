'use strict';
// A pretend long-distance network, for testing two copies of the game on one machine. Written 2026-11-07 by Melanikus for the AvP port.
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
//   node tools/net-relay.js --delay 150 --jitter 30 --loss 2 --burst 20 --seconds 300
//
// Every setting is for each direction, and a setting can be given for one direction only with "up-" (joiner to host) or "down-" (host to
// joiner) in front of it, such as --down-delay 120. Times are in milliseconds and chances in percent:
//   --delay     how long each packet is held, one way. 100 is Brazil to the USA, 150 Brazil to Europe, 250 Brazil to Japan.
//   --jitter    each packet's delay is moved by up to this much, either way, at random
//   --loss      the chance a packet is dropped
//   --burst     the chance that a drop is the start of a burst: 2 to 4 more packets after it are dropped too, so 3 to 5 in all
//   --reorder   the chance a packet is held back long enough to arrive after the next ones
//   --dupe      the chance a packet is sent twice
//   --drop-first  how many of the first packets on the game port are dropped for certain. --down-drop-first 1 loses the host's first
//               answer to a joiner, which carries ENet's cookie, so it shows a joiner still gets in when that answer is lost
//   --seed      the random numbers start from this, so a run can be repeated exactly
//   --seconds   it stops after this long, and writes its summary. It also stops when its input closes, or on Ctrl+C.
//   --listen    the address the joiner is given (127.0.0.2), --to the host's (127.0.0.1), --ports the ports (23513,23514)
//   --summary   a file to write the summary to as well
//   --stop-file  it stops, and writes its summary, as soon as this file exists

const dgram = require('dgram');
const fs = require('fs');

// ---- The settings, checked before anything opens

// Only the machine's own addresses: four numbers, each 0 to 255, the first 127
function IsLoopback(address)
{
	const parts = /^(\d{1,3})\.(\d{1,3})\.(\d{1,3})\.(\d{1,3})$/.exec(address);

	return parts !== null && parts.slice(1).every((part) => Number(part) <= 255) && Number(parts[1]) === 127;
}

function Refuse(why)
{
	process.stderr.write(`net-relay: ${why}\n`);
	process.exit(2);
}

const kDirections = ['up', 'down'];
const kChances = ['loss', 'burst', 'reorder', 'dupe'];
const kTimes = ['delay', 'jitter'];
const kCounts = ['drop-first'];

function ReadSettings(words)
{
	const settings = { listen: '127.0.0.2', to: '127.0.0.1', ports: [23513, 23514], seconds: 0, seed: 1, summary: '', stopFile: '', up: {}, down: {} };

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
		else if (name === 'summary' || name === 'stop-file')
		{
			settings[name === 'summary' ? 'summary' : 'stopFile'] = value;
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

const settings = ReadSettings(process.argv.slice(2));

// ---- Random numbers that can be repeated: the same seed gives the same run

let state = settings.seed >>> 0 || 1;

function Random()
{
	// mulberry32: a small, well-known generator. It's for test conditions, not for anything secret.
	state = (state + 0x6D2B79F5) >>> 0;
	let t = state;
	t = Math.imul(t ^ (t >>> 15), t | 1);
	t ^= t + Math.imul(t ^ (t >>> 7), t | 61);
	return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
}

function Chance(percent)
{
	return Random() * 100 < percent;
}

// ---- What happens to one packet, going one way

const counts = {};

function CountsFor(port, direction)
{
	const key = `${port} ${direction}`;

	if (!counts[key])
	{
		counts[key] = { arrived: 0, sent: 0, dropped: 0, droppedFirst: 0, inBursts: 0, duplicated: 0, reordered: 0, heldLeast: Infinity, heldMost: 0, heldAll: 0 };
	}

	return counts[key];
}

// Each direction of each port keeps its own burst and its own order of arrival
const lines = {};

function LineFor(port, direction)
{
	const key = `${port} ${direction}`;

	if (!lines[key])
	{
		lines[key] = { burstLeft: 0, nextNumber: 0, highestSent: -1 };
	}

	return lines[key];
}

// Holds, drops or repeats a packet, then sends it with send(packet). Nothing in the packet is read or changed.
function Pass(port, direction, packet, send)
{
	const how = settings[direction];
	const count = CountsFor(port, direction);
	const line = LineFor(port, direction);
	const number = line.nextNumber++;

	count.arrived++;

	// The first packets on the game port, dropped for certain: so a test knows the first answer to a joiner, the one with ENet's cookie in it,
	// never arrives, and the joiner has to ask again
	if (port === settings.ports[0] && number < how['drop-first'])
	{
		count.dropped++;
		count.droppedFirst++;
		return;
	}

	if (line.burstLeft > 0)
	{
		line.burstLeft--;
		count.dropped++;
		count.inBursts++;
		return;
	}

	if (Chance(how.loss))
	{
		count.dropped++;

		if (Chance(how.burst))
		{
			// This one and 2 to 4 more: 3 to 5 in all
			line.burstLeft = 2 + Math.floor(Random() * 3);
			count.inBursts++;
		}

		return;
	}

	const copies = Chance(how.dupe) ? 2 : 1;

	if (copies === 2)
	{
		count.duplicated++;
	}

	for (let copy = 0; copy < copies; copy++)
	{
		let held = how.delay + (Random() * 2 - 1) * how.jitter;

		// Held back past the ones behind it: as long again as the most jitter, and a little more
		if (copy === 0 && Chance(how.reorder))
		{
			held += 2 * how.jitter + 20;
		}

		// A copy follows a moment after the first
		held += copy * (1 + Random() * 10);
		held = Math.max(0, held);

		count.heldLeast = Math.min(count.heldLeast, held);
		count.heldMost = Math.max(count.heldMost, held);
		count.heldAll += held;

		setTimeout(() => {
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
		}, held);
	}
}

// ---- The sockets

// More than any test needs, so a flood of strangers can't make it open sockets without end
const kMostConnections = 16;
const kForgetAfterMilliseconds = 120000;
const sockets = [];
let strangers = 0;

function Open(port)
{
	// The joiner's side: what the joiner thinks is the host
	const front = dgram.createSocket('udp4');
	const behind = new Map();

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

	front.on('error', (error) => Refuse(`can't use ${settings.listen}:${port}: ${error.message}`));
	front.bind(port, settings.listen, () => process.stdout.write(`net-relay: ${settings.listen}:${port} passes to ${settings.to}:${port}\n`));

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

// ---- The end, and its summary

let finished = false;

function Finish(why)
{
	if (finished)
	{
		return;
	}

	finished = true;

	const describe = (how) => `delay ${how.delay} ms, jitter ${how.jitter} ms, loss ${how.loss}%, bursts ${how.burst}% of drops, reorder ${how.reorder}%, dupes ${how.dupe}%, the first ${how['drop-first']} on the game port dropped`;
	const summary = [`net-relay: stopped: ${why}`, `  joiner to host: ${describe(settings.up)}`, `  host to joiner: ${describe(settings.down)}`, `  seed ${settings.seed}`];

	for (const key of Object.keys(counts).sort())
	{
		const c = counts[key];
		const [port, direction] = key.split(' ');
		const average = c.sent ? (c.heldAll / c.sent).toFixed(0) : '0';
		const least = c.sent ? c.heldLeast.toFixed(0) : '0';

		summary.push(`  port ${port}, ${direction === 'up' ? 'joiner to host' : 'host to joiner'}: ${c.arrived} arrived, ${c.sent} sent, ${c.dropped} dropped (${c.inBursts} in bursts, ${c.droppedFirst} of the first), ${c.duplicated} duplicated, ${c.reordered} reordered, held ${least} to ${c.heldMost.toFixed(0)} ms, ${average} on average`);
	}

	summary.push(`  from anyone else: ${strangers}, refused`);

	const text = summary.join('\n') + '\n';

	process.stdout.write(text);

	if (settings.summary)
	{
		fs.writeFileSync(settings.summary, text);
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
if (settings.stopFile)
{
	setInterval(() => {
		if (fs.existsSync(settings.stopFile))
		{
			Finish('the stop file appeared');
		}
	}, 500);
}

process.on('SIGINT', () => Finish('Ctrl+C'));
process.stdin.on('end', () => Finish('its input closed'));
process.stdin.resume();
