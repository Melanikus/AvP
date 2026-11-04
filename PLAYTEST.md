# Playing a test build

**Test builds are not releases.** They exist so people can try the work in progress. Nothing is released until it's
safe, and known security bugs are still open: read "Stay safe" below before you play.

## Which build?
- `avpx_main` is **test**. Every commit there has passed the security scans, and each one gets a build.
- Tags (`v1.2.3`) are **stable**. A tag only builds if it is on `avpx_main` and the scans passed on that exact commit.

## What you need
- Windows 8 or later.
- Your own copy of **Aliens versus Predator Classic 2000** from Steam. The game's data is not in this repository, and we
  don't distribute it.

## Install, by hand
1. **Copy your game to a new folder.** Don't install into the Steam folder: a test build replaces the game's program, and
   Steam may overwrite it. A copy keeps your real game untouched. This folder is your *test folder*.
2. **Get the build.** On GitHub, open **Actions**, then **Build**, then the run for the commit or tag you want, and download
   the `avp-...` file under **Artifacts**. GitHub asks you to sign in, and keeps each build for 30 days.
3. **Check it.** Unzip it, then, in PowerShell, in the unzipped folder:
   ```powershell
   Get-Content SHA256SUMS.txt | ForEach-Object {
     $hash, $file = $_ -split '  ', 2
     if ((Get-FileHash -Algorithm SHA256 $file).Hash.ToLower() -ne $hash) { "DIFFERENT: $file" }
   }
   ```
   If it prints nothing, every file matches. If it prints `DIFFERENT`, don't use that build.
4. **Copy** `AvPx.exe`, every `.dll` and the `shaders` folder into your test folder.
5. **Start** `AvPx.exe` from your test folder.

Or, with the GitHub CLI signed in, `tools\install-build.ps1 -Branch avpx_main -GameFolder <your test folder>` does steps 2 to 4
and keeps your previous files in a backup folder.

## Build it yourself instead
Install Visual Studio 2026 with the C++ tools and vcpkg, then:
```
git clone https://github.com/Melanikus/AvP.git
cd AvP
git checkout <tag or commit>
msbuild win32\VS2026\AvP.sln /p:Configuration=Release /p:Platform=Win32
```
The game and its DLLs are in `win32\VS2026\Win32\Release`. The first build takes a while, because it builds the libraries
from source.

## Stay safe
- **Only load your own saves and levels.** Crafted save files and custom levels are not fully hardened yet.
- **Don't host or join LAN games with machines you don't trust.**
- Your antivirus may warn about the build: it's new and unsigned. Check the hashes above. Tell us rather than
  turning your antivirus off.

## Found a problem?
Open an issue on GitHub with what you did, and attach `avp_log.txt` from `Documents\My Games\Aliens versus Predator`.
