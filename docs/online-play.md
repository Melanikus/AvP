# Playing online

This is for players. It says how to play over the internet today, how the relay will work once it's live, what the game's log says about the network, and what is and isn't private.

The game's files and its log are in `Documents\My Games\Aliens versus Predator`. `avp_log.txt` is the run going now. `avp_log.1.txt` to `avp_log.5.txt` are the five before it, newest first.

## 1. Today: playing by address

One player hosts, and the others join by typing the host's internet address on the join page. The address shows as dots, so it stays off streams and screenshots. Hold Tab to see what you typed.

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

**It isn't live yet.** The relay server's address isn't built into the game until its domain is bought, so for now everything in this section is only for testers.

**What it is:** a server of ours that carries a game's packets between the host and each joiner. Nobody has to forward anything, and a player behind carrier-grade NAT can host. It also keeps a **list of games**, so you pick a game instead of typing an address.

**What you'll see:**
- **On the page of games found,** the list's games appear after the ones found on your own network, as `Name (2/4)`: the game's name, then how many play and how many fit.
  - A game for another version of the game shows its version, as `Name (V 1.13)`, and can't be joined.
  - A full game can't be joined.
  - A game on a level you don't have isn't shown. A co-op campaign game still is, as on your own network: its page tells you if you're missing its map.
  - The page shows at most 10 games, the newest first.
- **Joining** takes a few seconds while the relay introduces you to the host. It gives up by itself after about 26 seconds, and **Esc gives up at once**.
- **Hosting:**
  - the game times each relay and opens on the nearest, and its game goes on the list;
  - a host open through the relay hears only the relay, so **friends on your own network join through the list too**;
  - if no relay answers, the game is open on your own network instead, as it always was.
- **There's nothing to set.** The relay server's name will be built into the game.

**For testers only.** The settings below are read **only when the game starts with `-dev`**, for a test with a relay server on the same machine. Without `-dev` they're ignored, so a settings file someone shares can't point your game at another server. Note the heading is `[Network]`, not the `[Networking]` that `PortNumber` is under:

```
[Network]
RelayServer = localhost
RelayListPort = 8443
```

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
| `net: no relay the list named could be looked up, so this game is open on this network only` | the relays' names gave no address | check your internet connection |
| `net: this game's room on the relay is gone, so nobody more can join through it: …` | the relay stopped answering, or ended the room (the reason follows) | host again |
| `net: this game is open on this network instead` | after a lost room, the game is open on your own network | nothing |
| `net: while going through a relay, N packet(s) from elsewhere were dropped` | packets from anyone but the relay reached the host, and were refused | nothing. That's the protection working |
| `Net_ConnectToSession - couldn't join through the relay: …` and `Relay: couldn't join the game, because …` | a join through the relay failed. The reason is one of the game's own: "the relay knows no game with that code", "that game is full", "the relay is too busy just now", "the relay didn't answer", "the game's host didn't answer through the relay", "no relay could be reached", "the join was given up" (you pressed Esc) | for "full" or "no game", pick another game. For the others, try again |

**Lines starting `Relay list:`** These are about the list of games, which the game reads over HTTPS.

| Line | What it means | What to do |
|---|---|---|
| `Relay list: the game is on the list` and `… off the list` | a host's game was listed, then taken off | nothing |
| `Relay list: no games from the list, because …` | reading the list failed. The reason is the game's own: "no relay server is set" (normal until it's live), "the relay server couldn't be reached", "… took too long to answer", "… certificate wasn't accepted", "… said no", or a reason the list's answer was refused, such as "the answer has more after its end" | for "couldn't be reached" or "took too long", check your connection. Send us any other |
| `Relay list: the relay server's certificate wasn't accepted` | Windows didn't accept the server's HTTPS certificate, so the game refused to talk to it | check your PC's date and time. If those are right, tell us: the game never talks to a server Windows doesn't trust |
| `Relay list: the relay's name looked up to …, which only -dev allows` (and the other "looked up to" lines) | a relay's name led to an address that can't be a relay out on the internet, such as your own network's, so that relay is left out | nothing. That's the protection working |
| `Relay list: a relay's name gave no address, so that relay is left out` | a relay's name couldn't be looked up | check your connection |
| `Relay list: the list of relays was refused, …` | the list's answer wasn't what the game expects, so it keeps the last good one | send us the log |
| `Relay list: the settings' RelayServer is for tests only, with -dev, so the game's own is used` | the settings name a relay server, but the game started without `-dev`, so they're ignored | nothing, unless you're testing |
| `Relay list: couldn't start, …` | the game couldn't start its list calls | send us the log |
| `Relay list: N earlier piece(s) of news came too fast to keep` | some list lines were left out | nothing |

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
