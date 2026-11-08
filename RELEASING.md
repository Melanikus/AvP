# Making a release

Written for someone who has never done this and can't ask anyone. If you've just found this project, years from now, this should be enough.

## What a release is

A release is a zip of the game's program, its libraries, its shaders and its notes, with a list of hashes, on the GitHub Releases page.
It doesn't include the game's data: players bring their own copy of the game.

A release is different from a test build. A test build is kept for 30 days for people to try (`.github/workflows/build.yml`, and
`PLAYTEST.md`). A release is meant to stay, and it's a promise that we know of no open security hole in it.

## Before you start

Read `RELEASE-BLOCKERS.md`. If anything is listed there, the workflow will refuse, and it's right to: fix those things first.

## Making one

1. On GitHub, go to **Actions**, then **Release**, then **Run workflow**. Leave the branch on `avpx_main`, and type a version: three plain
   numbers, such as `1.0.0`, or for a beta of a version, those, then `-beta.` and the beta's number, such as `1.0.0-beta.0`.
   - A version's tag is `v` and its numbers, such as `v1.0.0`, and its download is `AvP-1.0.0-windows-x86.zip`.
   - A beta's tag is `v` and its version too, such as `v1.0.0-beta.0`. Its download is `AvP-1.0.0-beta.0-windows-x86.zip`, and it's
     marked as a pre-release.
   - **A beta's number is the next one.** A version's first beta is `-beta.0`, then `-beta.1`, and so on, as many in a day as you need. The
     check reads the tags and releases already on GitHub, and refuses a number that's been used, one below the highest, or one that skips
     ahead (the user's ruling, 2026-11-08). Betas come before their version: once `v1.0.0` is out, the next betas are `1.0.1-beta.0` or
     `1.1.0-beta.0`.
   - A draft you delete doesn't count, so its number is free again. A draft that's still waiting holds its number: the run refuses to make
     a second one, so publish or delete it before the next beta.
   - Nothing else is taken: no `v` in front (the tag adds it), no `0` in front of a number (`1`, not `01`, so one version can't be written
     two ways), at most 6 digits in each of the three numbers and 4 in a beta's, so `-beta.9999` is a version's last. Each wrong way is
     refused with its own reason.
   - The first beta was named `betarelease` and its day, `betarelease20261107`, which allowed one a day. That name isn't taken any more.
     The one published that way stays as it is.
2. The first job checks whether this commit may be released. If it refuses, the last lines of its log say why, starting `REFUSED:`.
   `tools/check-release.ps1`, `tools/check-no-hudson.ps1` and `tools/check-new-functions.ps1` are the full list of what it checks. Nothing has been
   built or published at this point.
3. The second job builds the game and checks the package. `tools/check-release-package.ps1` is the list of what it looks for. The first
   build on a clean machine takes a while, because the libraries are built from source.
4. The third job sends the zip to VirusTotal, which runs about 70 virus scanners on a file, and waits for their answers. It waits for you
   first: GitHub shows **Review deployments**, because the VirusTotal key is kept in the `release` environment. Approve it.
   - If an answer can't be trusted (not the shape VirusTotal's answers are, about another file, or from fewer than 20 scanners), the run stops
     there, red, and no draft is made. Its log says why, starting `REFUSED:`. `tools/check-virustotal.ps1` is the list of what it checks.
   - If any scanner flags the zip, the run goes on to the draft, so that you can read what was found, and then ends red (the user's ruling,
     2026-11-08).
5. The fourth job waits for you too. Approve it, and it makes a draft release. Its text says what VirusTotal found, with the link to the
   report. If any scanner flagged the zip, the text starts by saying so, in bold: don't publish it before reading the report.
   - **Approve it within a day of the build.** The package the build made is kept for one day (`retention-days: 1` in `release.yml`), and
     this job downloads it after you approve. A day later it's gone, and the job stops with no draft. Nothing is harmed: start the run
     again.
6. Go to **Releases** and open the draft. Nobody without write access can see it. Check the commit it names, write what changed, and try the
   zip if you can.
   - **For a beta, the text gives the install steps first, then a short list of what's fixed or added** (the user's ruling, 2026-11-08).
     The install steps are the ones in `PLAYTEST.md`, which testers also get inside the zip.
7. Press **Publish release**. That's the moment it's public, and GitHub makes the tag then. If you've turned on immutable releases, as below,
   it can't be changed after that, so a mistake means a new version number.

If you decide against it, delete the draft. Nothing else was made, so the version number is still free. VirusTotal keeps the zip it was
sent: anyone with VirusTotal's paid access can download it. It's a build of open-source code, so that gives away nothing that isn't public.

## What it can't prove

The checks run on GitHub's machines, and their own machine may have a copy of a file a player's doesn't. Nothing here starts the game
itself on a clean Windows, so a person has to. `RELEASE-BLOCKERS.md` lists that until someone has done it the first time. Do it again for
each release, with the zip from the draft, before you press Publish.

## When Microsoft fixes its C runtime

The game and every library built for it take Microsoft's C and C++ runtime from Microsoft's own DLLs: the /MD switch, and
`vcpkg-overlay-triplets/x86-windows.cmake`. (It was built into them for a day, as /MT, until Bitdefender flagged the game built that way and the
user ruled it out: issue #111, card STATIC-UNDO-1, #125.) The runtime has two parts, and Microsoft fixes them in two different ways:

- **The Universal C runtime** (`ucrtbase.dll`) is part of Windows 10 and 11, so Windows Update fixes it, and the release has nothing to do.
- **The C++ part**, `vcruntime140.dll` and the C++ standard library's `msvcp140*.dll`, goes out in the release, beside the game in `bin`, so that
  a tester installs nothing. `tools/build-and-package.ps1` copies it from the redistributable folder of the Visual Studio that built the
  game, and `licences/README.txt` in the package names its version. Windows looks in the game's own folder first, so those copies are the
  ones the game uses, and Windows Update can't fix them: it only fixes the copies Microsoft installed itself.

So when Microsoft announces a security fix in the Visual C++ runtime (vcruntime, the C++ standard library, or the start-up code that every
program built with it carries inside itself), our release still has the hole until we build again with a Visual Studio that has the fix.
So:

1. Update GitHub's runner image, or the pinned Visual Studio version, to one with the fixed runtime.
2. Make a new release from the same commit, or a newer one, and publish it.
3. Say in the release notes which Microsoft fix it carries.

Until that new release is out, the old one counts as having an open hole, and `RELEASE-BLOCKERS.md` says so (AvP Tao #1).

## Building on a machine without vcpkg

The game's libraries are normally built by vcpkg from our pinned ports, as part of the build. A machine where vcpkg can't download sources
can't do that, so it takes the libraries GitHub built, once, and checks them before using them:

1. The owner starts **Build** by hand (Actions, "Run workflow", on avpx_main). It builds only scanned code on avpx_main, and it keeps the
   libraries it built as the artifact **libraries**, for a week. The run's log lists a SHA-256 for every file ("List the libraries'
   hashes"), the hash of that list ("The list itself: ..."), and the artifact's own hash ("Say the libraries' artifact hash"). It also records
   how the list was made, so that GitHub can vouch for it later ("Record how the libraries were built").
2. Whoever is allowed to go online downloads it: `gh run download <run number> --repo Melanikus/AvP -n libraries -D
   <folder>\vcpkg_installed\x86-windows`.
3. Before anything uses it, check it. If any of these fails, delete it all and don't use it.
   - `gh attestation verify <folder>\vcpkg_installed\x86-windows\SHA256SUMS.txt --repo Melanikus/AvP` must say the list was made by
     this repository's Build workflow.
   - The list's own hash must be the run log's "The list itself".
   - Every file must have the hash the list gives it, with no file missing and none extra.
4. Build against it with `/p:VcpkgManifestInstall=false /p:VcpkgApplocalDeps=false "/p:VcpkgInstalledDir=<folder>\vcpkg_installed\\"`.
5. It's never committed. Fetch it again when `vcpkg.json`'s baseline, an overlay port or the triplet changes, because then it's no longer
   what the game is built against.

## How someone checks a copy they downloaded

- `SHA256SUMS` beside the zip has the zip's hash, and `SHA256SUMS.txt` inside the zip has the hash of every file in it.
- `gh attestation verify <the zip> --repo Melanikus/AvP` asks GitHub whether that file really came out of this repository's workflow.
- `BUILD.txt` inside the zip names the commit.

## Reading a crash a tester sends

After a crash, the log's crash lines name each place as a program and a distance into it, such as `AvP_Classic.exe+1A2B3C`. Look it up
in the `AvPx.map` kept with that build: it's the same program, renamed in the package. The build makes `AvPx.exe`, and the package
carries it as `bin\AvP_Classic.exe`, the name Steam starts. A release keeps its map and symbols in its workflow run's
**release-symbols** artifact, and a test build in its **symbols** artifact. They're never in the package.

## What has to be set up on GitHub

The workflow file can't set these up for itself, and each of them is something a commit can't remove. If this project ever moves, set them up
again before the first release.

- **An environment called `release`,** under Settings, Environments. Add the owner as a required reviewer, so that the last two jobs wait for
  them. Leave "Prevent self-review" off, because the owner is also who starts the run. Under deployment branches, allow only `avpx_main`.
  **Turn "Allow administrators to bypass configured protection rules" off.** With it on, an admin could run the scan, which holds the
  VirusTotal key, and the last job, which can write to the repository, without the review these rules are there for.
- **The secret `VIRUSTOTAL_API_KEY`, in the `release` environment** (not the repository's own secrets), so that only a job under that
  environment can read it: the scan job. It's a VirusTotal account's API key; a free one allows 4 questions a minute and 500 a day, and a
  release asks a few dozen. GitHub never shows a secret again once it's saved, but anyone who can change the environment can replace it,
  so keep that to the owner.
- **A ruleset for tags matching `v*`,** that stops them being created, moved or deleted by anyone but a repository admin. The workflow never
  makes a tag. The owner's press of Publish does.
- **Immutable releases,** under Settings, General, so that nothing can be swapped in after a release is published.
- **Actions defaults,** under Settings, Actions, General: workflows get read-only access unless they ask for more, they can't approve pull
  requests, and actions have to be pinned to a full commit hash.

## Why it's built this way

- **Started by hand from `avpx_main`, not by pushing a tag.** A workflow comes from the commit it runs on, so a tag could point at a commit with
  the checks taken out.
- **A list of blockers in the repository.** A machine can't know what we know is wrong, so we write it down where the machine can read it. A
  pull request that takes a line out is a visible decision.
- **Four jobs.** Building runs a great deal of other people's code, from the libraries' own build scripts. That job can't write to anything.
  The scan reads answers from outside, VirusTotal's, so it can't write either, and it's the only job with the VirusTotal key. The job that
  can write has no copy of the code and no build tools, and reads nothing from outside but the numbers the scan checked.
- **A draft.** So a person reads it before anyone else can see it.
