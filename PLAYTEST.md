# Playing the tester build

**This is a pre-release for testers, not a release.** It has the security fixes so far, co-op campaigns with a lobby, the
Custom maps page and the in-game co-op pages. Read "Stay safe" before you play, and please send us your logs.

## What you need
- Windows 10 or 11.
- Your own copy of **Aliens versus Predator Classic 2000** from **Steam**. The game's data isn't in this download, and we
  don't distribute it.

Nothing else needs installing. The parts of Microsoft's Visual C++ runtime the game uses come with it, in `bin`, as Microsoft makes
them and signed by Microsoft, and Windows 10 and 11 have the rest. If Windows ever says a DLL is missing, tell us which one, and don't
download it from any site.

**GOG:** we've never tried a GOG copy, so we can't tell you where its files go. If you try one, tell us what you find.

## 1. Check the download
From the release page, download `AvP-<version>-windows-x86.zip` and `SHA256SUMS`, into the same folder. A beta's version ends in
`-beta.` and its number, such as `AvP-1.0.0-beta.0-windows-x86.zip`, and the highest number is the newest. The release page also links
VirusTotal's report on the zip.

1. **The zip is what the release says.** In PowerShell, in that folder:
   ```powershell
   $hash, $name = (Get-Content SHA256SUMS) -split '  ', 2
   if ((Get-FileHash -Algorithm SHA256 -LiteralPath $name).Hash.ToLower() -eq $hash) { "SAME: $name" } else { "DIFFERENT: $name" }
   ```
   `DIFFERENT` means don't use it.
2. **GitHub built it, from this repository's release workflow.** With the GitHub CLI (`gh`), signed in with `gh auth login`:
   ```
   gh attestation verify AvP-<version>-windows-x86.zip --repo Melanikus/AvP
   ```
   It should finish by saying the verification succeeded, naming `Melanikus/AvP` and `.github/workflows/release.yml`. If it
   reports a failure or names anything else, don't use the download, and tell us.
3. **Every file inside is whole.** After you've unzipped it (section 2), in PowerShell, in the game folder:
   ```powershell
   Get-Content SHA256SUMS.txt | ForEach-Object {
     $hash, $file = $_ -split '  ', 2
     if ((Get-FileHash -Algorithm SHA256 -LiteralPath $file).Hash.ToLower() -ne $hash) { "DIFFERENT: $file" }
   }
   ```
   Nothing printed means every file matches.

## 2. Where to unzip (Steam)
Your **game folder** is `steamapps\common\Aliens versus Predator Classic`, inside your Steam library. It holds `bin`,
`avp_rifs`, `avp_huds` and the rest. The zip is laid out the same way.

1. **Unzip everything into the game folder.** When Windows asks, say yes to replacing files and merging folders.
   - `bin\AvP_Classic.exe` replaces Steam's, and the game's DLLs go beside it in `bin`.
   - `shaders` goes in the game folder itself, beside `bin`: the game reads its shaders from the folder Steam starts it in.
   - The M5 guns' files go into `avp_huds`, `MelPyth` and `graphics`. They're new files: none of them replaces one of
     Steam's, and your own `marwep.rif` is left as it is.

   | Folder | What the zip puts there |
   |---|---|
   | `bin` | `AvP_Classic.exe` and the game's DLLs |
   | `shaders` | the shaders |
   | `avp_huds` | `MelPyth_MarHUD.RIF`, `MelPyth_MarWep.rif`, `ShotgunPickup.rif`, `Staff_Hud.RIF`, `StaffPickup.RIF` |
   | `MelPyth\fastfile` | `common.ffl`, the new guns' sounds |
   | `graphics\MelPyth\HUDs\Marine` | the 15 pictures (`.rim`) |
   | the game folder itself | `PLAYTEST.md` (this guide), `License.txt`, `BUILD.txt` and `SHA256SUMS.txt` |
   | `licences` | each library's own licence, `README.txt`, which says which is whose and where the source of the ones under the GNU licences is, and in `licences\sources` the source of OpenAL Soft |

   `bin\d3dx9_43.dll` and `bin\D3DCompiler_43.dll` are Microsoft's D3DX. Using them means accepting Microsoft's licence terms for
   them, in `licences\D3DX.txt`.

   `MelPyth_MarWep.rif` holds the M5 versions of the pulse rifle, with its ammo counter, and of the pistols. With it the game
   draws those guns from it; without it, from your own `marwep.rif`, as before.
2. **Press Play** in Steam.
3. **To undo it,** in Steam: the game's **Properties**, **Installed Files**, **Verify integrity of game files**. That puts
   back Steam's own files. The files we added stay until you delete them, and the original game doesn't use them.
   Don't verify while you're testing: it puts Steam's exe back over ours.

## 3. Co-op in five lines
1. **The host** chooses **Co-op campaign** on the Multiplayer page, picks the campaign, the level and the difficulty, and
   presses **Start** when everyone's in. Everyone plays the campaign's species. The host's page says who'll be left behind.
2. **Each joiner** chooses **Join Existing Game**, then **Find Games**, which lists the games on the same network (two PCs at home,
   say). Or **Open IP Address**, to type the host's address: the only way over the internet. **The host lets each joiner in:** the
   host sees "*name* asks to join", and chooses **Let *name* in** or **Say no to *name*** at the top of the in-game menu (Esc), or of
   the Co-op campaign page. A request nobody answers in 30 seconds counts as no. Everyone needs the same beta: one beta can't play
   with another.
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
  If you've given F2 to one of your controls, F2 does that instead and marks nothing.
- **The M5 guns:**
  - **As a marine:** open the console and type `GIVEALLWEAPONS`, in capitals. (On a UK keyboard the console is the key under Esc;
    on others it may be another key, or none, so tell us which key works for you.) It gives every marine
    gun: the shotgun, the sniper rifle, the revolver, the M79 and the rest. In single player and the co-op campaign it's the only
    way to get the sniper rifle for now, since no level places one yet. Other multiplayer games leave the sniper rifle out.
  - **In other multiplayer games,** a marine can also pick the shotgun, revolver or M79 class.
  - **The Predator's staff:** the host turns **Allow Staff** on, in a multiplayer game that isn't co-op.
- **The console's test commands are on in this beta,** typed in capitals, and using them doesn't stop you saving, unlocking levels
  or finishing a campaign. Among them: `GIVEALLWEAPONS`, `GOD` (can't be killed), `GIMME_CHARGE` (a Predator's field charge),
  `SHOWFPS`, `SHOWPOLYCOUNT`, `LIGHT`, `WIREFRAMEMODE 0` to `3` and `MOTIONTRACKERSPEED`. `MARINEBOT`, `PREDOBOT`, `XENOBORG`,
  `ALIENBOT`, `PREDALIENBOT` and `PRAETORIANBOT` make an enemy in front of you in single player. A network game refuses all six,
  and `TIMESCALE`, because each would change everyone's game.
- **Send the log folder** when you're done, as below.

## 5. Logs: please send them
They're in `Documents\My Games\Aliens versus Predator`.

- **`avp_log.txt` is this run.** `avp_log.1.txt` to `avp_log.5.txt` are the five runs before it, newest first.
- **After a crash,** start the game again, then send `avp_log.1.txt`: that's the run that crashed. Two kinds of crash end the game
  before it can write anything, so a log with no crash line can still be a crash. Tell us what happened when you send it.
- **A map that won't load,** or one that loads with something left out, also writes `<map name>_errorlog.txt` beside the map. If
  the game can't write in that folder, the file goes in `Documents\My Games\Aliens versus Predator` instead, and the log says where.
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
