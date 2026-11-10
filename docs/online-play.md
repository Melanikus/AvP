# Playing online

This is for players. It says how to play over the internet today, how the relay will work once it's live, what the game's log says about the network, and what is and isn't private.

The game's files and its log are in `Documents\My Games\Aliens versus Predator`. `avp_log.txt` is the run going now. `avp_log.1.txt` to `avp_log.5.txt` are the five before it, newest first.

## 1. Today: playing by address

One player hosts, and the others join by typing the host's internet address on the join page. The address shows as dots, so it stays off streams and screenshots. Hold Tab to see what you typed.

**Every join asks the host first,** however it's joined: by address, from the list, or on your own network, co-op included. Nobody gets into a game without the host's yes.
- **The host** sees `Ripley asks to join (25 s). Esc to answer` on the screen. In the Esc menu, or on the co-op lobby page, two rows come first: **Let Ripley in** and **Say no to Ripley**. The selection stays where it was, so a key pressed by chance can't let anyone in.
- **The joiner** sees `Asking the host to let you in (25 s)`, and Esc gives up.
- **One at a time:** while the host is answering one player, another who asks is told the host is busy, and can try again in a moment.
- **30 seconds** with no answer counts as no.

**The host forwards two UDP ports on their router to their PC:**

| Port | What it's for |
|---|---|
| **23513** | the game itself: every packet of play goes through it |
| **23514** | how a joiner finds the game. A joiner's game asks on this port what the game is (its name, level and style, and how many players are in it) before it connects, and the host's game answers. Without it, a joiner's search finds nothing, even when the address is right |

- **Both are needed.** A joiner forwards nothing.
- There's no automatic forwarding (UPnP). The router's own page does it: look for "port forwarding" or "virtual server".
- **The game's port can be changed.** Set `PortNumber` under `[Networking]` in `AliensVsPredator.cfg`. It has to be the same on every machine in the game, and it can't be 23514. 23514 itself can't be changed.
- **The Windows firewall** asks the first time the game hosts, and maybe the first time it joins. Allow it on private networks.

**Some connections can't host at all:**
- many mobile connections;
- some fibre ones;
- anything behind carrier-grade NAT (one internet address shared between many homes);
- IPv6-only connections.

A port can't be forwarded on any of those. Those players can still join: someone on an ordinary home connection hosts. **The relay, below, is for them.**

## 2. Soon: through the relay

**It isn't live yet.** A release names no server yet, so for now everything in this section is only for testers. The game finds its server in **servers.cfg**, a short text file beside the game's `bin` folder, and never anywhere else: not in the settings, not on the command line.

**What it is:** a server of ours that carries a game's packets between the host and each joiner. Nobody has to forward anything, and a player behind carrier-grade NAT can host. It also keeps a **list of games**, so you pick a game instead of typing an address.

**What you'll see:**
- **While the page of games found looks,** an alien's tail turns in the middle, and one line says what it's doing: looking on your own network, then asking the list. It asks again every 5 seconds. If it finds nothing in a minute, it stops and says so, and **Enter looks again**. Esc goes back.
- **The games** are a table: **Game | Level | Players | Region | Ping**. The list's games come after the ones found on your own network.
  - **Region** is the game's relay, or `this network` for a game on your own network.
  - **Ping** is about how long a packet takes from the host to you through the relay: your own time to that relay, plus the host's. It shows `?` until your game has timed the relay, and `-` for a game on your own network.
  - The table keeps itself up to date every 5 seconds, and **the selection stays on the game you picked**, wherever it moves. A game that has ended shows `gone` once, then goes.
  - A game for another version of the game shows its version, as `V 1.13`, and can't be joined. A full game shows `full`, and can't be joined either.
  - A game on a level you don't have isn't shown. A co-op campaign game still is, as on your own network: its page tells you if you're missing its map.
  - The page shows at most 10 games, the newest first. A name too long for its column ends with two dots, and a letter the game's font doesn't have shows as `?`.
- **Joining** takes a few seconds while the relay introduces you to the host: the tail turns, with `Reaching the relay in saopaulo`, then `The relay is introducing you to the host`. It gives up by itself after about 26 seconds, and **Esc gives up at once**.
- **Then the host is asked** (above). If the answer is no, or the host is busy, or doesn't answer, or you press Esc, you're back on the list, with that game still selected and a line saying which.
- **Hosting:**
  - the game times each relay and opens on the nearest, and its game goes on the list;
  - a host open through the relay hears only the relay, so **friends on your own network join through the list too**;
  - if no relay answers, the game is open on your own network instead, as it always was.
- **There'll be nothing to set.** A release will ship a servers.cfg that already has our server's line in it.

**For testers only.** Until online play is on for players, the game uses servers.cfg **only when it starts with `-dev`**. Without `-dev` it doesn't even read the file, so it reaches no server at all. servers.cfg has one line per server: its address, written plainly, its fingerprint and its name, with spaces between them:

```
# A line starting with # is a note. The game skips notes and empty lines.
203.0.113.7  <64 small hex digits>  New York
127.0.0.1:8443:27500  <64 small hex digits>  My test server
```

- **The address** is four numbers with dots, never a name. The server's list of games answers HTTPS on port 443 there, and its relay on port 27500, unless the line gives both ports, as `address:listport:relayport`.
- **The fingerprint** proves the server. Before the game sends the server anything that matters, it checks the server's keys against the fingerprint, and a server whose keys don't match is sent nothing more. Take a server's line only from whoever runs that server.
- **The name** is what the game shows, up to 31 letters.
- A line the game can't use is skipped, and the log says which line and why (below). Two lines with the same name or the same address are both skipped. For now the game uses the first good line.

Two copies of the game on one PC: start the second with `-dev -secondcopy`, so it keeps its own settings, log and saves in `Documents\My Games\Aliens versus Predator (second copy)`.

## 3. What the log's network lines mean

Some of these lines have `Error:` in front of them in the log. The words after it are the same.

**Lines starting `net:`**

| Line | What it means | What to do |
|---|---|---|
| `net: this game answers searches on UDP port 23514 from now` | the host is open to joiners' searches | nothing |
| `net: this game can't answer searches: UDP port 23514 couldn't be opened, so a search won't find it` | something else on this PC holds port 23514, maybe a second copy of the game | close the other program, or the other copy |
| `net: the first search question came … s after this game began answering, …` | a joiner's search reached the host | nothing. If no such line comes when someone searches, port 23514 isn't forwarded, or a firewall drops it |
| `net: searches in the last … s: …` (every minute), and `net: searches while this game was open, …` (at the end) | how many searches came, and how many were answered | "too many at once" means one address asked faster than the game answers, which is normal for a few |
| `net: connection N round trip N ms loss N% resends N …` | how one connection is doing, by number, never by address | nothing. These are for us when you send a log |
| `net: connection N is struggling: …` and `… is fine again: …` | that connection lost many packets for a while, then recovered | if it keeps happening, check your Wi-Fi or connection |
| `net: in N bytes, out N bytes, in the last N s` | how much went each way | nothing |
| `net: while loading, the longest gap between answers was N ms, and the patience is N ms` | how close a level load came to dropping someone | if a joiner is dropped while loading, send us this line |
| `net: the settings file's PortNumber is …, which the game can't use, so it uses …` | `PortNumber` in the settings isn't a usable port (or it's 23514) | fix it in `AliensVsPredator.cfg` |
| `net: the host's connection closed` | the host left, or the connection to it ended | nothing |
| `net: left without hearing back from N connection(s), which may have gone already` | on leaving, some players didn't answer the goodbye | nothing |
| `net: this game is finding the nearest relay` | hosting through the relay: timing each relay | nothing |
| `net: this game is opening through the nearest relay, N ms away` | the nearest relay, and its round trip | nothing |
| `net: the relay has opened this game's room, and it's going on the list` | the room is open, and the list is told | nothing |
| `net: no relay answered, so this game is open on this network only` | no relay answered in time | try again later. Your own network can still join |
| `net: the relay the list named can't be used, so this game is open on this network only` | the relay at the server's address can't be used, such as one with no port it can take | send us the log |
| `net: this game won't answer searches on this network while it goes through a relay` | hosting through the relay, a search on your own network won't find the game | nothing. Friends there join from the list, like everyone else |
| `net: this game's room on the relay is gone, so nobody more can join through it: …` | the room couldn't be kept. The reason follows, one of: "the relay didn't answer" (no room within 10 s), "the relay is too busy just now", "the relay has too many games from this address", "the relay said no, for a reason this version doesn't know", "the relay stopped answering, so the room has gone" (no answer to the room's keeps for 45 s). The game is then open on your own network | host again, to get back on the relay |
| `net: this game is open on this network instead` | after a lost room, the game is open on your own network | nothing |
| `net: while going through a relay, N packet(s) from elsewhere were dropped` | packets from anyone but the relay reached the host, and were refused | nothing. That's the protection working |
| `Net_ConnectToSession - couldn't join through the relay: …` and `Relay: couldn't join the game, because …` | a join through the relay failed. The reason is one of the game's own: "the relay knows no game with that code", "that game is full", "the relay is too busy just now", "the relay didn't answer", "the game's host didn't answer through the relay", "no relay could be reached", "the relay said no, for a reason this version doesn't know", "the join was given up" (you pressed Esc, or closed the game during the wait) | for "full" or "no game", pick another game. For "a reason this version doesn't know", update the game. For the others, try again |

**Lines starting `Relay list:`** These are about the list of games, which the game reads over HTTPS.

| Line | What it means | What to do |
|---|---|---|
| `Relay list: the game is on the list` and `… off the list` | a host's game was listed, then taken off | nothing |
| `Relay list: no games from the list, because …` | reading the list failed. The reason is the game's own: "no relay server is set" (normal until it's live: no `-dev`, or no good line in servers.cfg), "the relay server couldn't be reached", "… took too long to answer", "… secure connection couldn't be made", "… said no", "the relay server's keys don't match its line in servers.cfg, …", or a reason the list's answer was refused, such as "the answer has more after its end" | for "couldn't be reached" or "took too long", check your connection. For the keys, see below. Send us any other |
| `Relay list: the server is servers.cfg line N` (with `, on this PC` or `, on this network` for a server at home) | which line of servers.cfg the game uses | nothing |
| `Relay list: servers.cfg line N isn't used: …` | that line was skipped. The reason says what's wrong with it, such as "its fingerprint isn't 64 small hex digits", "its address can't be a server's", or "another line has the same name, so neither is used". The line itself is never written in the log | fix that line, or get it again from whoever runs the server |
| `Relay list: servers.cfg isn't used: …`, or `Relay list: there's no servers.cfg in the game's folder, …` (and the other lines about finding the file) | the whole file was skipped, such as one over 16 KB or of more than 64 lines, or the game couldn't find or read it | put a good servers.cfg beside the game's `bin` folder |
| `Relay list: the relay server's keys match its line in servers.cfg` | the server proved it's the one the line names | nothing |
| `Relay list: the relay server's keys don't match its line in servers.cfg, so nothing more is sent to it` | the server at that address isn't the one the line names: the line is wrong or old, or something else answered at that address. Nothing more goes to it until the game restarts | get the line again from whoever runs the server. If it's right, tell them |
| `Relay list: the relay server showed another key than the one that matched its line, so nothing was sent to it` | after it matched, the server showed another key on a later call, so that call was dropped before anything went | tell whoever runs the server, and send us the log |
| `Relay list: couldn't start, …` | the game couldn't start its list calls | send us the log |
| `Relay list: N earlier piece(s) of news came too fast to keep` | some list lines were left out | nothing |
| `Relay list: the games page looks for games again` | Enter on the page of games found, after it had stopped looking | nothing |

**Lines starting `online screens:`** These are about asking the host. They never name the player.

| Line | What it means | What to do |
|---|---|---|
| `online screens: a player asks to join this game` | someone has joined and is waiting for the host's answer | answer from the Esc menu |
| `online screens: the host let the player in` and `… said no to the player` | the host's answer | nothing |
| `online screens: every screen will be shown at the menus, …` and the other `-onlinescreens` lines | a tester asked for every screen, with made-up games, for pictures (`-dev -onlinescreens`) | nothing, unless you're testing |

## 4. What's private, and what isn't

**What's private:**
- **The log never holds an address.** No network address or port, and nothing that identifies a relay room: no code, token, ticket or pass. Connections are numbers. The log is safe to post as it is. It does hold the names players typed in the lobby, and your profile's name.
- **The relay server writes no address down,** in its logs or anywhere else. It sees your address while you play, as any server you talk to does.

**What isn't private:**
- **The list shows your game's name** to everyone reading the list. Don't put anything private in it.
- **Today's online play isn't encrypted,** by address or through the relay (version 1 of the relay's protocol). Anyone on the path can read and change the game's packets:
  - your Wi-Fi;
  - your provider;
  - the relay, once it's live.

  The game's packets are only play, but don't type anything private in the game's chat.
- **Version 2 will encrypt every packet end to end,** so even the relay can't read a game. It's designed (`CRYPTO.md` in the relay server's repository), and comes later.

**Only play with people you trust** while this is a test build.
