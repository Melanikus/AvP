# Playing a test build

**Test builds are not releases.** They exist so people can try the work in progress. Nothing is released until it's
safe, and known security bugs are still open: read "Stay safe" below before you play.

## Which build?
Builds are made only when the project's owner starts one, from `avpx_main`, and only if the security scans passed on that
commit. There may be none to download: known security bugs are still open, so automatic builds and stable tags stay off
until those are closed.

## What you need
- Windows 8 or later.
- Your own copy of **Aliens versus Predator Classic 2000** from Steam. The game's data is not in this repository, and we
  don't distribute it.
- Microsoft's Visual C++ Redistributable for Visual Studio, the **x86** one, from Microsoft's website. If Windows says a file
  such as `VCRUNTIME140.dll` is missing, this is what it needs. Don't get DLLs from any other site.

## Install, by hand
1. **Copy your game to a new folder.** Don't install into the Steam folder: a test build adds its own program and libraries,
   and Steam may overwrite or remove them. A copy keeps your real game untouched. This folder is your *test folder*.
2. **Get the build.** On GitHub, open **Actions**, then **Build**, then the run you want, and download
   the `avp-...` file under **Artifacts**. GitHub asks you to sign in, and keeps each build for 30 days.
3. **Check it.** Unzip it, then, in PowerShell, in the unzipped folder:
   ```powershell
   Get-Content SHA256SUMS.txt | ForEach-Object {
     $hash, $file = $_ -split '  ', 2
     if ((Get-FileHash -Algorithm SHA256 $file).Hash.ToLower() -ne $hash) { "DIFFERENT: $file" }
   }
   ```
   If it prints nothing, every file matches. If it prints `DIFFERENT`, don't use that build. A match shows that your download
   isn't damaged. It doesn't show who made it, because the list of hashes comes inside the same download.
4. **Copy** `AvPx.exe`, every `.dll` and the `shaders` folder into your test folder.
5. **Start** `AvPx.exe` from your test folder.

Or, from a copy of this repository (the script isn't in the build) and with the GitHub CLI signed in:
```
powershell -ExecutionPolicy Bypass -File tools\install-build.ps1 -GameFolder <your test folder>
```
It does steps 2 to 4 and keeps your previous files in a backup folder beside your test folder. `-ExecutionPolicy Bypass` lets
this one command run this one script, and changes nothing else on your machine.

## Build it yourself instead
Install Visual Studio 2026 with the C++ tools and vcpkg, then:
```
git clone https://github.com/Melanikus/AvP.git
cd AvP
git checkout <commit>
msbuild win32\VS2026\AvP.sln /p:Configuration=Release /p:Platform=Win32
```
The game and its DLLs are in `win32\VS2026\Win32\Release`. The first build takes a while, because it builds the libraries
from source.

## Stay safe
- **Only load your own saves and levels.** Crafted save files and custom levels are not fully hardened yet.
- **Don't host or join multiplayer games with people you don't trust.** That means any network game, on a LAN or by IP
  address.
- Your antivirus may warn about the build: it's new and unsigned. Don't run a build your antivirus blocks, and don't turn your
  antivirus off. Matching hashes don't make a warning safe. Tell us instead.
- The security scans have limits. Microsoft's D3DX library comes as a binary, so it can't be scanned. Four reports against
  libvorbis are accepted until a set date: see `.github/snyk-accepted.json`.

## Turning on the new look
A new install looks the way the game always has: the new lighting and shadows are off until you choose them. They're under **Rendering options** in the audio and video options, in the main menus and while playing:
- **Rendering:** Gouraud (Classic), Per-pixel, Per-pixel + normals, or Relief. Relief draws the same as Per-pixel + normals for now.
- **Shadows:** yes or no.
- **Auto-generate maps:** works out maps for textures that have none. Turning it on takes effect from the next level.

What you choose is saved in `AliensVsPredator.cfg`. To put them on keys, type these in the console: `BIND <key> LIGHTING` moves on to the next rendering, and `BIND <key> SHADOWS` switches shadows. Those change the game until it closes, and only the menu saves.

## What may look different
The game now refuses damaged or unexpected files instead of reading them, so you may meet these:
- **A level that won't load, or loads with parts missing.** Every level from the game should load as it always did. If one
  doesn't, please tell us which: that's a bug in our new checks.
- **A profile missing from the list.** A profile file that was renamed by hand no longer shows up, because the name inside
  it doesn't match the file's name.
- **The game stopping with an out-of-memory message.** It used to crash without saying why.
- **Music going quiet.** If a music file can't be read part of the way through, the music stops instead of playing noise.

## Found a problem?
Open an issue on GitHub with what you did, and attach `avp_log.txt` from `Documents\My Games\Aliens versus Predator`.

If it's a security bug, don't put the details in a public issue. Open one that only says you have something to report
privately, and we'll arrange how.
