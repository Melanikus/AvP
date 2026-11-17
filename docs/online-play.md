# Playing online

This is for players. It says how online play works, what the game's log says about the network, and what is and isn't private.

**Release builds play no online games.** v1.0.0-beta.2 has no online play (ruling R-GOAL-9: online play is off in beta.2). It's built
without `AVPX_ONLINE_PLAY`, ships no `servers.cfg`, and never reads one, even one added by hand. Its Multiplayer page shows only your
name and **Online**, and Online says "This version of the game plays no online games: online play comes in a later beta." It doesn't
load or make an identity for this PC either. Online play comes back in a later beta. Everything below describes the test builds made
with `AVPX_ONLINE_PLAY`, and that later release.

The game's files and its log are in `Documents\My Games\Aliens versus Predator`. `avp_log.txt` is the run going now. `avp_log.1.txt` to
`avp_log.5.txt` are the five before it, newest first.

## 1. How a game is played

**Every game goes through a server's relay, sealed end to end** (#214). There's no game at home, no joining by a typed address, and no
search for games on your own network. Friends on the same network join through the list, like everyone else.
- **Nobody forwards a port.** The relay carries a game's packets between the host and each joiner, so a player behind carrier-grade NAT,
  a mobile connection or an IPv6-only one can host too.
- **Sealed:** version 2 of the relay's protocol. The relay carries the game, and neither the relay nor anyone on the way can read or
  change it. If the relay can't be reached, or is lost while you play, the online game doesn't start, or it ends. It never goes on
  unsealed.
- **The Windows firewall** may ask the first time the game goes online. Allow it on private networks.

**Every join asks the host first.** Nobody gets into a game without the host's yes.
- **The host** sees `Ripley asks to join (25 s). Esc to answer` on the screen. In the Esc menu, or on the co-op lobby page, two rows come
  first: **Let Ripley in** and **Say no to Ripley**. The selection stays where it was, so a key pressed by chance can't let anyone in.
- **The joiner** sees `Asking the host to let you in`, and Esc gives up.
- **One at a time:** while the host is answering one player, another who asks is told the host is busy, and can try again in a moment.
- **30 seconds** with no answer counts as no.

Two copies of the game on one PC: start the second with `-dev -secondcopy`, so it keeps its own settings, log and saves in
`Documents\My Games\Aliens versus Predator (second copy)`.

## 2. Servers: servers.cfg, your yes, and your access

**servers.cfg** is a short text file in the game's folder, beside `bin`. It has one line per server. The game finds its servers there and
nowhere else: not in the settings, not on the command line.
- **Take a server's line only from whoever runs that server.** Each line holds the server's fingerprint, which proves the server: before
  the game sends a server anything that matters, it checks the server's keys against it, and a server whose keys don't match is sent
  nothing more.
- **Every good line shows, and every line is used the same way** (rulings R-SRV-1 and R-SRV-2). Adding a server is adding a line. Nobody
  picks one server to play on (R-SRV-3): you see the games of every server that has let you in.
- A line's name is what the game shows, up to 31 letters.
- **Limits:** the file is at most 16 KB and 64 lines. A line the game can't use is skipped, and both the Online page and the log give its
  number and why. The line itself is never written in the log. Two lines with the same name, or the same address, are both skipped.

**The Online page** (Multiplayer, then **Online**) has a row for every good line, the ones the game ships with too.
- **Each row's first line** is the server's name, whether it's one of ours, and its last round trip, as "(ours) - 42 ms". With no round
  trip yet, it says "Asking..." while the server is being asked, or "no answer" for one that didn't answer.
- **Each row's second line** says where you stand on that server. For example:
  - `No yes yet: press Enter to see this server and say yes or no`;
  - `Waiting for approval: give its owner your code ...`;
  - `Refused: its owner hasn't let this PC in`;
  - `Approved: 3 games`.
- **Enter on a row** does the next thing that server needs: first your yes (below), then a request to its owner to let this PC in. A
  request is sent at most once a minute for each server, and Enter again checks.
- **Your access code** is shown once, at the top. It's the same for every server, because this PC has one identity. **Copy access code**
  puts it on the clipboard, to paste to a server's owner. **Name to send** is the name the owner sees with your request: up to 15 letters,
  digits, spaces and signs, with no `"` or `\`.
- **Make a new identity** gives this PC a new access code. It starts over on every server: each one has to approve you again, and the old
  identity can't be brought back. The page waits until every request's answer has come before it makes one.
- **Each run asks again.** As the Multiplayer page opens, the game asks every server you've said yes to whether it has let this PC in, so
  the game lists and the host page know without a visit to the Online page.

**Your yes comes first, for every server** (R-SRV-9: every line, shipped ones included, asks the player once at first use). The first time you press Enter on a
server's row, the game shows you that server: its name, its address as servers.cfg has it, where that is (on the internet, on this PC, or
on your own network), and the two ends of its key. It also says what its owner sees: "Its owner approves their own players, and sees your
name, your key and your address while you play."
- **No** comes first, so an Enter pressed by chance sends nothing. Nothing at all goes to a server before your yes.
- Your yes is kept on this PC as a hash, never as an address, in `%LOCALAPPDATA%\AVPx\servers-trusted.cfg`. It holds up to 64 yeses, and
  when it's full the oldest goes. To take back every yes, delete that file.
- A line with another address, port or key is another server, and you're asked again.

## 3. Finding and hosting games

**Finding games** (Multiplayer, then **Join game**, then **Find Games**):
- **Every server that has let you in** has its games in one table, and the heading names the servers that answered: "Games from" then
  their names. Only the first 8 servers in servers.cfg are asked.
- **The table** is **Game | Level | Players | Region | Ping**.
  - **Region** names the server the game is on, by its name in servers.cfg.
  - **Ping** is about how long a packet takes from the host to you: your own time to that game's relay, plus the host's. It shows `...`
    while your time is being measured, and `?` until it's known.
  - A game that has ended shows `gone` once, then goes. A full game shows `full`, and can't be joined. A game for another version of the
    game shows its version, and can't be joined either.
  - At most 10 games are shown. A name too long for its column ends with two dots, and a letter the game's font doesn't have shows as `?`.
- **A server that hasn't let you in yet** still shows its games, greyed, and they can't be joined. The words under the list say to ask on
  the Online page. A server you haven't said yes to says "say yes to it on the Online page, to see its games".
- **Search** shows only the games with what you type in their name. What you type stays on this PC. **Refresh** fetches the games again.
- **Joining** takes a few seconds while the relay introduces you to the host: `Reaching the relay`, then `The relay is introducing you to
  the host`. It gives up by itself after about 26 seconds, and **Esc gives up at once**. Then the host is asked (section 1). If the answer
  is no, or the host is busy, or doesn't answer, you're back on the list with a line saying which.

**Hosting** (Multiplayer, then **Start game** or **Co-op campaign**) opens the host page first: "Choose the server to host on:".
- **Nothing is chosen for you** (R-SRV-6, "No auto choice, they choose"). The page opens on its own question, and only Enter on a server's
  row chooses it.
- **Every server has a row** (R-SRV-1), in servers.cfg's order, never sorted by ping (R-SRV-5). Each row shows the server, its ping, its
  games, their players and its relay's region (R-SRV-7).
- **Only a server you can host on can be chosen:** you've said yes to it, its owner has approved this PC, and it answered the last time it
  was asked. Every other row says why it can't be chosen, and Enter on a server that didn't answer asks every server again.
- **Then the game opens through that server's relay,** sealed, and goes on that server's list. If the server doesn't answer within 30
  seconds, nothing is hosted, and the screen says so.

## 4. What the log's network lines mean

Some of these lines have `Error:` in front of them in the log. The words after it are the same. Lines that name a Hudson run are for our
test bot, not for players.

**Lines starting `net:`**

| Line | What it means | What to do |
|---|---|---|
| `net: connection N round trip N ms loss N% resends N …` | how one connection is doing, by number, never by address | nothing. These are for us when you send a log |
| `net: connection N is struggling: …` and `… is fine again: …` | that connection lost many packets for a while, then recovered | if it keeps happening, check your Wi-Fi or connection |
| `net: in N bytes, out N bytes, in the last N s` | how much went each way | nothing |
| `net: while loading, the longest gap between answers was N ms, and the patience is N ms` | how close a level load came to dropping someone | if a joiner is dropped while loading, send us this line |
| `net: the settings file's PortNumber is …, which the game can't use, so it uses …` | `PortNumber` under `[Networking]` in `AliensVsPredator.cfg` isn't a usable port | fix it, or remove it |
| `net: this player's identity is the one kept from earlier runs` | this PC's identity was found and loaded | nothing |
| `net: a new identity was made for this player and kept` | there was none, so one was made | nothing. Your access code is new, so servers have to approve you |
| `net: the kept identity: …`, then `net: so there's no identity this run until the player chooses to make a new one, and nothing goes through a relay` | the kept identity couldn't be used, for the reason given | make a new identity on the Online page |
| `net: the host's connection closed` | the host left, or the connection to it ended | nothing |
| `net: left without hearing back from N connection(s), which may have gone already` | on leaving, some players didn't answer the goodbye | nothing |
| `net: this game is finding the nearest relay` | hosting: its sealed channel to the relay is opening | nothing |
| `net: this game's sealed channel to the relay is open, N ms away` | the channel to the relay is open, and its round trip | nothing |
| `net: the relay has opened this game's room, and it's going on the list` | the room is open, and the list is told | nothing |
| `net: this game can't be hosted: choose a server to host on` | no server was chosen on the host page | choose one, then host |
| `net: this game can't be hosted online: you haven't said yes to the server in servers.cfg yet` | nothing was sent: the game asks you first | say yes to it on the Online page, then host |
| `net: this game can't be hosted online yet: the server hasn't answered yet` | the chosen server is still being asked | wait a moment and host again |
| `net: this game can't be hosted online: …` (with `, so it isn't hosted at all (sealed or nothing)` when the relay failed) | the game couldn't go through the server, for the reason given. It's never hosted unsealed instead | try again later. Send us the log for any reason you don't understand |
| `net: this game won't answer searches on this network while it goes through a relay` | hosting: nothing on your own network can find the game directly | nothing. Friends there join from the list |
| `net: this game's room on the relay is gone, so nobody more can join through it: …` | the room couldn't be kept. The reason follows, one of: "the relay didn't answer" (no room within 10 s), "the relay is too busy just now", "the relay has too many games from this address", "the relay said no, for a reason this version doesn't know", "the relay stopped answering, so the room has gone" (no answer to the room's keeps for 45 s). The online game then ends (the next line) | host again |
| `net: …, so this online game has ended: it can't go on sealed, and it never opens on this network instead` | the relay was lost, so the game ended rather than go on unsealed | host again |
| `net: while going through a relay, N packet(s) from elsewhere were dropped` | packets from anyone but the relay reached the game, and were refused | nothing. That's the protection working |
| `Net_ConnectToSession - couldn't join through the relay: …` and `Relay: couldn't join the game, because …` | a join through the relay failed. The reason is one of the game's own: "the relay knows no game with that code", "that game is full", "the relay is too busy just now", "the relay didn't answer", "the game's host didn't answer through the relay", "no relay could be reached", "the relay said no, for a reason this version doesn't know", "the join was given up" (you pressed Esc, or closed the game during the wait) | for "full" or "no game", pick another game. For "a reason this version doesn't know", update the game. For the others, try again |
| `net: couldn't join the game, because …` | the join was refused before anything was opened, for the reason given | do what the reason says |

**Lines starting `Relay list:`** These are about servers.cfg, your yeses and access, and the lists of games, which the game reads over
HTTPS. A line about one server's answers starts `Relay list: servers.cfg line N:`, naming that server by its line's number.

| Line | What it means | What to do |
|---|---|---|
| `Relay list: this build plays no online games (it was made without AVPX_ONLINE_PLAY), so servers.cfg isn't read, and there are no servers` | a release build, or any build made without online play | nothing |
| `Relay list: servers.cfg isn't used: …`, or `Relay list: there's no servers.cfg in the game's folder, …` (and the other lines about finding the file) | the whole file was skipped, such as one over 16 KB or of more than 64 lines, or the game couldn't find or read it | put a good servers.cfg beside the game's `bin` folder |
| `Relay list: servers.cfg line N isn't used: …` | that line was skipped. The reason says what's wrong with it, such as "its fingerprint isn't 64 small hex digits", "its address can't be a server's", or "another line has the same name, so neither is used" | fix that line, or get it again from whoever runs the server |
| `Relay list: the list's own state is made from servers.cfg line N` (with `, on this PC` or `, on this network` for a server at home) | servers.cfg has at least one good line, and the list's own record keeping starts from the first. It doesn't choose a server: every line is used the same way | nothing |
| `Relay list: the player said yes to servers.cfg line N, and it's kept for the next time` (or `…, but it couldn't be kept, so it holds for this run only`) | you said yes to that server, and from then on the game may use it | nothing. If it couldn't be kept, you'll be asked again next time |
| `Relay list: servers-trusted.cfg isn't used, so every server is asked about again: …` | the file of your yeses isn't one the game wrote (a line it never writes, a link, or not your Windows user's), so none is taken from it | nothing: say yes again, and the file is written whole again |
| `Relay list: servers.cfg line N: the relay server's keys match its line in servers.cfg` | that server proved it's the one its line names | nothing |
| `Relay list: servers.cfg line N: the relay server's keys don't match its line in servers.cfg, so nothing more is sent to it` | the server at that address isn't the one the line names: the line is wrong or old, or something else answered there. Nothing more goes to it until the game restarts | get the line again from whoever runs the server. If it's right, tell them |
| `Relay list: servers.cfg line N: the relay server showed another key than the one that matched its line, so nothing was sent to it` | after it matched, the server showed another key on a later call, so that call was dropped before anything went | tell whoever runs the server, and send us the log |
| `Relay list: servers.cfg line N: …`, with another reason | reading that server's list failed, for the game's own reason: "the relay server couldn't be reached", "… took too long to answer", "… secure connection couldn't be made", "the relay server said no", or a reason its answer was refused, such as "the answer has more after its end" | for "couldn't be reached" or "took too long", check your connection. Send us any other |
| `Relay list: an access request waits for the player's yes to its server, so nothing was sent` | you asked for access to a server you haven't said yes to | say yes to it first, on its row |
| `Relay list: an access request goes only to a server a servers.cfg line names, and this one isn't, so nothing was sent` | a request was asked for a server that isn't in servers.cfg | nothing |
| `Relay list: a new identity waits until every access request's answer has come` | Make a new identity was pressed while a request was still out | wait, then make it |
| `Relay list: the game is on the list` and `… off the list` | your hosted game was listed, then taken off | nothing |
| `Relay list: no server was chosen to host on, so the game isn't put on any list` | a game was hosted with no server chosen | choose a server on the host page |
| `Relay list: this run has no identity to show joiners yet, so the game isn't put on the list` | this PC has no usable identity this run | make a new identity on the Online page |
| `Relay list: the game closed before it could come off the list, so the list forgets it by itself, within 45 seconds` | the game ended before it could say it was leaving | nothing |
| `Relay list: couldn't start, so hosts aren't listed and the list can't be read` | the game couldn't start its list calls | send us the log |
| `Relay list: N earlier piece(s) of news came too fast to keep` | some list lines were left out | nothing |

**Lines starting `online screens:`** These are about asking the host. They never name the player.

| Line | What it means | What to do |
|---|---|---|
| `online screens: a player asks to join this game` | someone has joined and is waiting for the host's answer | answer from the Esc menu |
| `online screens: the host let the player in` and `… said no to the player` | the host's answer | nothing |
| `online screens: -onlinescreens shows every screen, with made-up games and no network. …`, then `online screens: showing …` | a tester asked for every screen, with made-up games, for pictures (`-dev -onlinescreens`) | nothing, unless you're testing |

## 5. What's private, and what isn't

**What's private:**
- **The log never holds an address.** No network address or port, and nothing that identifies a relay room: no code, token, ticket or
  pass. Connections are numbers. The log is safe to post as it is. It does hold the names players typed in the lobby, and your profile's
  name.
- **The relay server writes no address down,** in its logs or anywhere else. It sees your address while you play, as any server you talk
  to does.
- **A game through the relay is sealed end to end** (version 2 of the relay's protocol: `CRYPTO.md` in the relay server's repository).
  Neither the relay nor anyone on the way can read or change it. The relay sees who plays with whom, when, and how much, but not what.
- **Your yeses** are kept as hashes, never as addresses.

**What isn't private:**
- **A server's owner** approves their own players, and sees your name, your key and your address while you play. That's on the server's
  yes page, before you say yes.
- **A server's list shows your game's name** to everyone reading that list. Don't put anything private in it.

**Only play with people you trust** while this is a test build.
