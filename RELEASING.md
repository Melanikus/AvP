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
   numbers, such as `1.0.0`.
2. The first job checks whether this commit may be released. If it refuses, the last lines of its log say why, starting `REFUSED:`.
   `tools/check-release.ps1` is the full list of what it checks. Nothing has been built or published at this point.
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

## How someone checks a copy they downloaded

- `SHA256SUMS` beside the zip has the zip's hash, and `SHA256SUMS.txt` inside the zip has the hash of every file in it.
- `gh attestation verify <the zip> --repo Melanikus/AvP` asks GitHub whether that file really came out of this repository's workflow.
- `BUILD.txt` inside the zip names the commit.

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
