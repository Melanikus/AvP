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
   numbers, such as `1.0.0`, or for a beta, `betarelease` and today's date as year, month and day, such as `betarelease20261107`.
   - A version's tag is `v` and its numbers, such as `v1.0.0`, and its download is `AvP-1.0.0-windows-x86.zip`.
   - A beta's tag is its name with no `v`, its download is `AvP-betarelease20261107-windows-x86.zip`, and it's marked as a pre-release.
   - Nothing else is taken, and a beta's date has to be a real day.
2. The first job checks whether this commit may be released. If it refuses, the last lines of its log say why, starting `REFUSED:`.
   `tools/check-release.ps1`, `tools/check-no-hudson.ps1` and `tools/check-new-functions.ps1` are the full list of what it checks. Nothing has been
   built or published at this point.
3. The second job builds the game and checks the package. `tools/check-release-package.ps1` is the list of what it looks for. The first
   build on a clean machine takes a while, because the libraries are built from source.
4. The third job waits for you. GitHub shows **Review deployments**. Approve it, and it makes a draft release.
5. Go to **Releases** and open the draft. Nobody without write access can see it. Check the commit it names, write what changed, and try the
   zip if you can.
6. Press **Publish release**. That's the moment it's public, and GitHub makes the tag then. If you've turned on immutable releases, as below,
   it can't be changed after that, so a mistake means a new version number.

If you decide against it, delete the draft. Nothing else was made, so the version number is still free.

## What it can't prove

The checks run on GitHub's machines, and their own machine may have a copy of a file a player's doesn't. Nothing here starts the game
itself on a clean Windows, so a person has to. `RELEASE-BLOCKERS.md` lists that until someone has done it the first time. Do it again for
each release, with the zip from the draft, before you press Publish.

## When Microsoft fixes its C runtime

The game has Microsoft's C and C++ runtime built in, so a player needs no Visual C++ install: see `vcpkg-overlay-triplets/x86-windows-static.cmake`.
OpenAL Soft's DLL has it built in too. The other side of that is that Windows Update can't fix it for us. When Microsoft announces a security fix in
the Visual C++ runtime (MSVC's C runtime, C++ standard library or vcruntime), the copy inside our release still has the hole until we build again.
So:

1. Update GitHub's runner image, or the pinned Visual Studio version, to one with the fixed runtime.
2. Make a new release from the same commit, or a newer one, and publish it.
3. Say in the release notes which Microsoft fix it carries.

Until that new release is out, the old one counts as having an open hole, and `RELEASE-BLOCKERS.md` says so (AvP Tao #1).

## Building on a machine without vcpkg

The game's libraries are normally built by vcpkg from our pinned ports, as part of the build. A machine where vcpkg can't download sources
can't do that, so it takes the libraries GitHub built, once, and checks them before using them:

1. The owner starts **Game build** by hand (Actions, "Run workflow"), on the commit the libraries are wanted for. Started by hand, it keeps the
   libraries it built as the artifact **static-libraries**, for a week. The run's log lists a SHA-256 for every file ("List the libraries'
   hashes"), the hash of that list ("The list itself: ..."), and the artifact's own hash ("Say the libraries' artifact hash").
2. Whoever is allowed to go online downloads it: `gh run download <run number> --repo Melanikus/AvP -n static-libraries -D
   <folder>\vcpkg_installed\x86-windows-static`.
3. Before anything uses it, check it: the hash of the downloaded `SHA256SUMS.txt` must be the run log's "The list itself", and every file
   must have the hash `SHA256SUMS.txt` gives it, with no file missing and none extra. If anything differs, delete it all and don't use it.
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

- **An environment called `release`,** under Settings, Environments. Add the owner as a required reviewer, so that the last job waits for them.
  Leave "Prevent self-review" off, because the owner is also who starts the run. Under deployment branches, allow only `avpx_main`.
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
- **Three jobs.** Building runs a great deal of other people's code, from the libraries' own build scripts. That job can't write to anything.
  The job that can write has no copy of the code and no build tools.
- **A draft.** So a person reads it before anyone else can see it.
