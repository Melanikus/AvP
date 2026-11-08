# Playing the tester build

**This is a pre-release for testers, not a release.** It has the security fixes so far, co-op campaigns with a lobby, the
Custom maps page and the in-game co-op pages. Read "Stay safe" before you play, and please send us your logs.

## What you need
- Windows 8 or later.
- Your own copy of **Aliens versus Predator Classic 2000** from **Steam**. The game's data isn't in this download, and we
  don't distribute it.
- Microsoft's Visual C++ Redistributable, the **x86** one, from Microsoft's website. If Windows says a file such as
  `VCRUNTIME140.dll` is missing, this is what it needs. Don't get DLLs from any other site.

**GOG:** we've never tried a GOG copy, so we can't tell you where its files go. If you try one, tell us what you find.

## 1. Check the download
From the release page, download `AvP-<version>-windows-x86.zip` and `SHA256SUMS`, into the same folder.

1. **The zip is what the release says.** In PowerShell, in that folder:
   ```powershell
   $hash, $name = (Get-Content SHA256SUMS) -split '  ', 2
   if ((Get-FileHash -Algorithm SHA256 $name).Hash.ToLower() -eq $hash) { "SAME: $name" } else { "DIFFERENT: $name" }
   ```
   `DIFFERENT` means don't use it.
2. **GitHub built it, from this repository's release workflow.** With the GitHub CLI (`gh`), signed in with `gh auth login`:
   ```
   gh attestation verify AvP-<version>-windows-x86.zip --repo Melanikus/AvP
   ```
   Look for **Verification succeeded**, naming `Melanikus/AvP` and `.github/workflows/release.yml`. Anything else, don't use
   it, and tell us. [PENDING: this wording is from gh's documentation, not seen yet. The first real pre-release checks it once,
   and this line is corrected to match.]
3. **Every file inside is whole.** Unzip, then in the unzipped folder:
   ```powershell
   Get-Content SHA256SUMS.txt | ForEach-Object {
     $hash, $file = $_ -split '  ', 2
     if ((Get-FileHash -Algorithm SHA256 $file).Hash.ToLower() -ne $hash) { "DIFFERENT: $file" }
   }
   ```
   Nothing printed means every file matches.

## 2. Where to paste (Steam)
Your **game folder** is `steamapps\common\Aliens versus Predator Classic`, inside your Steam library. It holds `bin`,
`avp_rifs`, `avp_huds` and the rest.

1. **Back up** what you're about to replace, somewhere outside the game folder:
   - `bin\AvP_Classic.exe`
   - any file in `bin` with the same name as a `.dll` in the zip
   - the `shaders` folder in the game folder, if there is one
2. **Paste `AvPx.exe` and every `.dll`** from the zip into `bin`.
3. **Rename** `bin\AvPx.exe` to `AvP_Classic.exe`, replacing Steam's, which you backed up.
4. **Paste the `shaders` folder** into the **game folder**, beside `bin` and `avp_rifs`, not inside `bin`. The game reads its
   shaders from the folder Steam starts it in, which is the game folder.
5. **The M5 guns' files:** paste the zip's `avp_huds`, `MelPyth` and `graphics` folders into the **game folder**, beside
   `shaders`. Windows asks whether to merge `avp_huds` with the one already there: say yes. These are new files: none of them
   replaces one of Steam's, and your own `marwep.rif` is left as it is.

   | Folder in the zip | What's in it |
   |---|---|
   | `avp_huds` | `MelPyth_MarHUD.RIF`, `MelPyth_MarWep.rif`, `ShotgunPickup.rif`, `Staff_Hud.RIF`, `StaffPickup.RIF` |
   | `MelPyth\fastfile` | `common.ffl`, the new guns' sounds |
   | `graphics\MelPyth\HUDs\Marine` | the 15 pictures (`.rim`) |

   `MelPyth_MarWep.rif` holds the M5 versions of the pulse rifle, with its ammo counter, and of the pistols. With it the game
   draws those guns from it; without it, from your own `marwep.rif`, as before.

6. **Press Play** in Steam.

**To undo it,** in Steam: the game's **Properties**, **Installed Files**, **Verify integrity of game files**. That puts back
Steam's own files that you replaced. The files we added stay until you delete them, and the original game doesn't use them.
Don't verify while you're testing: it puts Steam's exe back over ours.

## 3. Co-op in five lines
1. **The host** chooses **Co-op campaign** on the Multiplayer page, picks the campaign, the level and the difficulty, and
   presses **Start** when everyone's in. Everyone plays the campaign's species. The host's page says who'll be left behind.
2. **Each joiner** types the host's IP address to join. There's no list of games yet.
3. **The host forwards UDP ports 23513 and 23514** on their router to their PC: 23513 is the game, and 23514 is how a joiner finds
   it. A joiner needs nothing. There's no automatic forwarding (UPnP). The game's port, 23513, can be changed with
   `PortNumber` under `[Networking]` in `AliensVsPredator.cfg`, in `Documents\My Games\Aliens versus Predator`, and it has to
   be the same for everyone. 23514 can't be changed.
   - **Some connections can't host at all:** many mobile ones, some fibre ones, and IPv6-only ones, where a port can't be
     forwarded. Those players can still join. Someone on an ordinary home connection hosts.
   - **The Windows firewall** asks the first time the game hosts, and maybe the first time it joins. Allow it on private networks.
   - **Everyone plays the same copy of the game,** with the same level files. Nothing checks that yet, so a different edition or a
     changed level can make doors and lifts drift apart between players.
4. **A custom map** has to be the same file on every machine, in `avp_rifs\Custom`. Maps aren't sent over the network, and the
   host's page checks each joiner's copy. Anyone without it is told so and stays out.
5. **There's no saving in co-op.** If the host leaves, the game ends for everyone.

**What lag looks like, and is normal:** another player can overshoot when turning, then snap back. Enemies can move in small
steps. After a lost packet there's a short pause, then everything catches up at once. Tell us if it's worse than that.

**On a VPN or a tunnel:** the game's packets are up to about 1420 bytes, so a VPN can split them, and some firewalls drop the
pieces. If a game stalls only over a VPN, try without it.

## 4. What we'd like you to try
- **Finish a campaign in co-op,** any of the three, and tell us which.
- **Try the lobby** (who's in, what's chosen, who'll be left behind) and the **Custom maps** page.
- **Press F2 whenever something looks weird.** The screen says "Mark 1 saved" (then 2, 3 and so on). The game saves a picture of
  that moment as `mark-1.png` beside the log, and adds a line to the log: the time, the level, where you were and which way you
  faced, your species, health and weapon, and in a network game each connection's round trip. No names or addresses. It marks
  at most once a second, and keeps the newest 20 pictures. Tell us the mark's number when you describe what you saw.
  If you've given F2 to one of your controls, F2 does that instead and marks nothing. [PENDING: the mark key is in train 3.]
- **The M5 guns:**
  - **In single player or the co-op campaign, as a marine:** open the console (the key under Esc, left of 1) and type
    `GIVEALLWEAPONS`, in capitals. It gives every marine gun: the shotgun, the sniper rifle, the revolver, the M79 and the rest.
    It's the only way to get the sniper rifle for now: no level places one yet.
  - **In other multiplayer games:** a marine picks the shotgun, revolver or M79 class. There's no sniper rifle there.
  - **The Predator's staff:** the host turns **Allow Staff** on, in a multiplayer game that isn't co-op.
- **Send the log folder** when you're done, as below.

## 5. Logs: please send them
They're in `Documents\My Games\Aliens versus Predator`.

- **`avp_log.txt` is this run.** `avp_log.1.txt` to `avp_log.5.txt` are the five runs before it, newest first.
- **After a crash,** start the game again, then send `avp_log.1.txt`: that's the run that crashed. Two kinds of crash end the game
  before it can write anything, so a log with no crash line can still be a crash. Tell us what happened when you send it.
- **A map that won't load** also writes `<map name>_errorlog.txt` beside the map. [PENDING: once LEVELLOG-1 is in.]
- **What to send:** the log, what you did, and for a co-op problem the logs from every machine. If a joiner is dropped while a
  level loads, the line to look for is "the longest gap between answers".

**A log is safe to post as it is.** It never holds your Windows user name, your computer's or network's name, or any network address
or port: those are written as `<user>`, `<computer>`, `<address>` and so on. It does hold:
- at the top, the game's version, the build it came from and your Windows version;
- after a crash, what kind of crash it was and where in the program, as numbers. Nothing from memory, and nothing about your machine;
- times: how long a load or a join took, round trips, the time of day;
- **the names players typed in the lobby, and your game profile's name.** If you don't want yours in a posted log, play under a
  name you're happy to show.

If your Windows user name is also an ordinary word, such as "admin", that word is left out everywhere in the log, so a line may read
oddly. That's on purpose: it leaves out too much rather than too little.

## Stay safe
- **Only play co-op with people you trust,** on a LAN or by IP address. The network code has had a lot of hardening, but this
  is a test build.
- **Only load your own saves and maps,** or maps from people you trust.
- **Your antivirus may warn about the build:** it's new and unsigned. Don't run a build your antivirus blocks, and don't turn
  your antivirus off. Tell us instead.
- **The security scans have limits.** Microsoft's D3DX library and its shader compiler come as binaries, so they can't be
  scanned.

## Found a problem?
Open an issue on GitHub with what you did, and attach your log.

**If it's a security bug,** don't open a public issue. On the repository's page, open the **Security** tab and choose
**Report a vulnerability**. Only the maintainers see what you send there.
