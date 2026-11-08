# Removed code

This file lists what we have taken out of this repository, why it went, and how to get it back.

The game has one build today: 32-bit Windows, Direct3D 9, Visual Studio 2026 (`win32/VS2026`). Code that build never
compiles gets removed, because nobody can test it and the tools that check our code can't always reach it. But some of it
is the only record of how something worked, so nothing goes without a row here.

**If you remove code, add a row.**

## If you are porting the game

Start here:

- **Audio on Linux, macOS or Android:** OpenAL Soft already supports them, and nothing needs bringing back. See
  [Third-party code](#third-party-code).
- **What a new platform has to replace:** the original Xbox port is the only second platform this code ever had. Its 8
  files in `xbox/src` show the pieces a port must supply: startup, input, audio, CD music and the Direct3D layer. See
  [Whole folders](#whole-folders-and-projects).
- **OpenGL:** there is no OpenGL renderer to bring back. `3dc/renderer.h` has `USE_OPENGL` and `USE_GLIDE` switches, but they
  include `ogl_func.h` and `glide_func.h`, which never existed. The project's old SVN repository
  (`https://subversion.assembla.com/svn/AvP`) has a branch called `sdl2_opengl`, but despite the name a search of it found no
  SDL or OpenGL calls, so don't count on it.
- **A Linux project file:** you will need a new one. The old Code::Blocks project was too far out of date to help.

## How to get code back

Everything under [Our code](#our-code) is in commit `e4394fc` on the `avpx_main` branch, the last commit before this cleanup.
Line numbers below are from that commit.

```
git show e4394fc:3dc/networking.cpp        # read one file
git ls-tree -r --name-only e4394fc xbox    # list a folder
git restore --source=e4394fc -- xbox       # put a whole folder back
```

Be careful with `git restore` on a file that still exists: it replaces the whole file with the old version, undoing every
later change to it. For part of a file, read it with `git show` and copy what you need.

The **Removed by** column gives the title of the commit that did it. Its message has the details, such as exact function
names. To find it:

```
git log --all --grep="Delete the unbuilt original-Xbox port"
```

We give titles rather than commit IDs because an ID can change before a branch merges. `e4394fc` can't change.

## Our code

### Whole folders and projects

| What it was | Where it was, and how to read it | Why it went | What a port would need it for | Removed by |
|---|---|---|---|---|
| The original Xbox port: its own project, 8 source files and an old copy of ENet (63 files) | `git ls-tree -r --name-only e4394fc xbox`, then for example `git show e4394fc:xbox/src/main_xbox.cpp` | A separate Visual Studio .NET 2003 project that no build used. `3dc/renderer.h` lost its `USE_D3D_XBOX` include with it | **The best map of what a new platform must replace.** Read `xbox/src` before starting any port | Delete the unbuilt original-Xbox port (xbox/) |
| ENet 1.3.12, the networking library, with its docs (195 files) | `git ls-tree -r --name-only e4394fc enet`, then for example `git show e4394fc:enet/host.c` | Out of date and not compiled. The game gets ENet 1.3.18 from vcpkg | Nothing. ENet itself supports Linux and macOS: use a current release, not this copy | Delete the stale ENet copy and the old VS2010 and VS2013 projects |
| The Visual Studio 2010 and 2013 projects | `git show e4394fc:win32/VS2010/AvP.vcxproj`, `git show e4394fc:win32/VS2013/AvP.vcxproj` | Replaced by `win32/VS2026`. They could no longer link | Nothing, apart from old build settings for reference | Delete the stale ENet copy and the old VS2010 and VS2013 projects |
| The Code::Blocks project, for the gcc compiler | `git show e4394fc:codeblocks/AvP.cbp` | One build system remains. 306 of its 490 files no longer existed, and 205 of the 234 source files we compile weren't in it | Very little. It shows that someone once tried gcc, but it needs replacing, not reviving | Delete the dead Code::Blocks project |
| The XAudio2 sound backend: `3dc/avp/win95/xa2_audio.cpp`, and what only it used in five other files | `git show 3364ee9:3dc/avp/win95/xa2_audio.cpp`. The other parts are behind `USE_XAUDIO2` in `git show 3364ee9:3dc/avp/win95/psndplat.h`, `3dc/AudioStreaming.h`, `3dc/VorbisPlayer.cpp` and `3dc/video`'s Bink, Smacker and Theora players. SVN still has it, in `branches/workbranch/trunk` | Nothing defines `USE_XAUDIO2`, so it compiled to nothing. Barry Duncan's OpenAL backend replaced it in March 2013 (SVN r572 and r573), and its last change was May 2013 (r609). It still had the hang on a damaged WAV that the OpenAL one no longer has, so turning it on would have brought that back. It wasn't the Xbox port's sound, which was `xbox/src/audio.cpp` | Nothing. OpenAL Soft is the game's sound on every platform it could go to. It does show how the game's sound was once done through Microsoft's own API | Delete the unused XAudio2 sound backend |
| The installer's uninstall entry for `enet.dll` | `git show e4394fc:tools/installer/avpx.iss`, line 66 | ENet is now built into `AvPx.exe`, so there is no such file | Nothing | Drop enet.dll from the installer's uninstall list |

### Parts of files

| What it was | Where it was, and how to read it | Why it went | What a port would need it for | Removed by |
|---|---|---|---|---|
| The level-writing half of the chunk library: the code that builds `.rif` chunks from memory, as a level editor does | `git show e4394fc:3dc/win95/chunk_shape.cpp` holds the most. Also the other `3dc/win95/chunk_*.cpp` files, `3dc/win95/mishchnk.cpp`, `3dc/win95/bitmap_names.cpp`, `3dc/win95/animation_objects.cpp`, `3dc/win95/enum_chunk.cpp`, `3dc/win95/environment_chunk.cpp`, `3dc/avp/win95/avp_chunk.cpp` and `3dc/avp/win95/hierplace.cpp` | The game only reads levels, so nothing called this | Not for a port. **A level editor or converter would want it** | Delete the unused RIF-authoring half of the chunk library; Delete the RIF-authoring constructors left without a caller; Delete unused RIF-authoring code in the game chunk classes |
| The way of making a `Placed_Hierarchy_Data_Chunk` in code, from a name and numbers, and not from a level file | `git show 3d2531e:3dc/avp/win95/hierplace.cpp`, lines 134-157 | Nothing called it, and only a level editor would. It went when the name copy next to it was fixed | A level editor | Copy a placed hierarchy's name by its measured length, and give every test chunk an owner |
| Locking chunks inside a level file on disk: the other side of `DisableLock`, `update_chunk_in_file`, `check_file`, `update_chunks_from_file`, every `file_equals` and `get_head_id`, `list_chunks_in_file` and the `obinfile` struct | `git show 3d2531e:3dc/win95/mishchnk.cpp`, and `chunk.cpp`, `chunk_object.cpp`, `chunk_shape.cpp`, `chunk_gsprites.cpp`, `enum_chunk.cpp`, `environment_chunk.cpp` | `DisableLock` was always 1, so none of it could run. It was for several people editing one level at once, in the tools the game was made with | A level editor shared by several people, and it would need the reads below fixed | Read a level file's names only as far as the end of their chunk, and take out the file locking |
| A shape's polygons and points sorted into a grid of cubes, read from a `SHPZSPDT` chunk: `zsp.cpp` and `zsp.hpp` | `git show 3d2531e:3dc/win95/zsp.cpp`, and `zsp.hpp` | All of `zsp.cpp` was switched off with `#if 0`, marked not used, and nothing else included `zsp.hpp`. A level's `SHPZSPDT` chunk is kept as plain bytes, as it already was | Nothing: the game doesn't read these grids | Read a level file's names only as far as the end of their chunk, and take out the file locking |
| 18 maths helpers in Microsoft assembly language | `git show e4394fc:3dc/win95/inline.h`, lines 57-571 | Switched off for every compiler by `#if defined(_MSC_VER) && 0`, so never built in this port | Nothing. They only suit 32-bit x86, and the plain C versions are what the game runs | Delete inline.h's assembly versions, which were never compiled |
| 17 maths helpers in gcc assembly language | `git show e4394fc:3dc/mathline.cpp`, under `ASM386` and `USE_ASM` | Neither switch was ever on | Nothing, for the same reason | Delete mathline.cpp's assembly versions, which were never compiled |
| The whole body of `chunk_textures.cpp` | `git show e4394fc:3dc/win95/chunk_textures.cpp`, lines 2-925 | All inside `#if 0`. The file stays, holding only its change notice | Nothing, unless a level tool wants it | Delete disabled (#if 0) code in chunk_object.cpp and chunk_textures.cpp |
| `Adjacent_Modules_Chunk`, a chunk type | `git show e4394fc:3dc/win95/chunk_object.cpp`, lines 758-825 | Inside `#if 0`, and its class no longer exists | Nothing | Delete disabled (#if 0) code in chunk_object.cpp and chunk_textures.cpp |
| The start of a network path for damage to track objects | `git show 8cd8770:3dc/avp/bh_track.cpp`, the top of `TrackObjectIsDamaged` | Inside `#if 0`. The real path now does this in the mode that plays as single player does | Nothing | In the co-op mode, a track object can be broken, for everyone |
| The body of the `connect` console command: a test of connecting straight to an IP address | `git show e4394fc:3dc/networking.cpp`, lines 523-652 | Inside `#if 0`. `Net_ConnectToAddress` and the command remain, and do nothing | Not for a port. Useful when multiplayer is redone | Delete the disabled body of the "connect" console command |
| The `LISTMOD` console command, which listed the level's modules | `git show e4394fc:3dc/avp/win95/module_commands.cpp`, from line 51. Also `3dc/avp/win95/module_commands.hpp` line 27 and `3dc/avp/davehook.cpp` line 211 | Nothing could reach it: its only entry was inside `#if 0` | Could help when debugging level loading | Delete the module list command, which nothing can reach (S8) |
| `KeyBinding::AttemptToBind` and `AttemptToUnbind` | `git show e4394fc:3dc/avp/support/console_bind.cpp`, lines 223-304 | Inside `#if 0` | Nothing | Delete disabled (#if 0) code in three console / font / report files |
| The "PF" font load and unload code | `git show e4394fc:3dc/avp/support/index_font.cpp`, lines 982-1081 | Inside `#if 0` | Nothing | Delete disabled (#if 0) code in three console / font / report files |
| Two on-screen test messages in the text report | `git show e4394fc:3dc/avp/win95/gadgets/text_report_gadget.cpp`, lines 189-207 and 465-484 | Inside `#if 0` | Nothing | Delete disabled (#if 0) code in three console / font / report files |
| Debug logs for enemy generators and far-away positions, written to `D:/PATRICK/...` | `git show e4394fc:3dc/avp/bh_generator.cpp` (`logGenData`), `git show e4394fc:3dc/avp/pfarlocs.cpp` (`logFarLocPositions`) | Switched off in every build | Nothing | Delete the disabled debug logging in bh_generator.cpp and pfarlocs.cpp |
| "NO AMMO FOR" and "NOT AVAILABLE IN DEMO" weapon messages, and one unused `#else` | `git show e4394fc:3dc/avp/weapons.cpp`, around lines 1785, 1840, 2071 and 2119 | Never compiled, and they couldn't be: `tempstring` isn't declared there | Nothing | Delete the disabled (#if 0) weapon messages in weapons.cpp; Let code scanners parse weapons.cpp (delete a never-compiled #else); Remove two empty else-ifs and an unused variable (N3, N5) |
| An unfinished loop in `Bink_Open`, in the Bink video decoder | `git show e4394fc:dependencies/libbinkdec/src/BinkDecoder.cpp`, lines 108-117 | Inside `#if 0`, and not valid C++ | Nothing | Delete BinkDecoder.cpp's unfinished disabled block |
| Debug code that kept a record of every reference-counted object, switched on by `TrackReferenceCounted` | `git show e4394fc:3dc/avp/support/refobj.cpp`. Also `3dc/avp/support/refobj.hpp`, `3dc/avp/support/scstring.cpp`, `3dc/avp/support/scstring.hpp` and `3dc/avp/davehook.cpp` | Always off, and it could not have been switched on without further work, as it wasn't valid C++ | Not for a port. The idea could help when hunting memory leaks | Delete the reference-tracking debug code, which was never compiled |
| Older versions of some AI tests, kept beside the live ones | `git show e4394fc:3dc/avp/bh_marine.cpp` (7 places), `3dc/avp/bh_weapon.cpp`, `3dc/avp/bh_xeno.cpp`, `3dc/avp/bh_near.cpp` | Switched off. They also stopped code scanners reading the files | Not for a port. They show how the behaviour was tuned | Let code scanners parse bh_marine.cpp (delete never-compiled if-heads); Let code scanners read bh_weapon, bh_xeno, bh_near (delete dead #if 0s) |
| Shapes stored in a level as a raw copy of memory: the `SHPPRPRO` chunk, `Shape_Preprocessed_Data_Chunk` and `copy_preprocessed_to_shapeheader` | `git show e4394fc:3dc/win95/chunk_shape.cpp`, from line 3097, and `git show e4394fc:3dc/win95/chunk_load.cpp`, lines 2743-2780. Also `3dc/win95/chunk_shape.hpp` and `3dc/win95/chunk_load.hpp` | A level set the shape's counts and addresses itself, so none of the checks on shapes applied, and a crafted level could choose where the game wrote. None of the 86 levels that ship with the game holds one, and nothing in the game made one | Nothing. It kept addresses in 32 bits, so it couldn't work in a 64-bit build either. Do not bring it back: see below | Stop reading shapes stored as raw copies of memory |
| A chunk for a collection of sounds to choose from (`SOUNDCOL`, `Sound_Collection_Chunk`, `ChunkSoundWeighting`), and `CHUNK_EXTRACT_ARRAY`, the macro that read its list | `git show 7bc26b3:3dc/win95/chunk_sound.cpp`, at the end of the file, and `chunk_sound.hpp`, `chunk.hpp` | All of the chunk was inside a comment, so it never ran, and the macro was only used there. The macro made a list as long as a count in the file said, with no check | A level format with sound collections. It would need its list measured with `ReadListCount` | Stop a level file's chunks reading past their stated size |
| `spare1` and `spare2` on `Module_Waypoint_Chunk`, and the two reads that filled them | `git show 7bc26b3:3dc/win95/chunk_waypoint.cpp`, at the end of the constructor that reads a level, and `chunk_waypoint.hpp` | The code that writes a waypoint chunk never wrote them, so every waypoint chunk in the game's levels had the reads run 8 bytes past its end. Nothing used what they found | Nothing | Stop a level file's chunks reading past their stated size |

Some code changed shape without anything being removed, so a compiler sees the same thing. If you compare an old copy, these
will look different: `3dc/frustum.cpp` (the clipping macros), `3dc/win95/hash_tem.hpp` (the `HashTable` classes),
`3dc/maths.cpp`, `3dc/avp/hmodel.cpp` and `3dc/avp/bh_waypoint.cpp`.

## Third-party code

These libraries come from vcpkg, and the removed files were never in this repository. We build from a pinned source download,
and `vcpkg-overlay-ports/<library>/portfile.cmake` deletes the listed files from that download before building. Nothing is
lost: it's all in the library's own release, and the portfile names the version.

**To bring something back:** remove it from the `file(REMOVE ...)` lists in that portfile. Where a row names a setting, change
that too.

### OpenAL Soft (sound)

| What was removed | Where it applies | To bring it back | What a port would need it for |
|---|---|---|---|
| The sound output code for other systems, in `alc/backends/`: alsa, coreaudio, jack, oboe, opensl, oss, pipewire, portaudio, pulseaudio, sdl2, sdl3, sndio, solaris, winmm | **x86 Windows only** | **Nothing to bring back on another platform: the portfile only deletes these on x86 Windows.** Linux gets ALSA by itself, macOS gets CoreAudio, Android gets OpenSL. PipeWire and PulseAudio are vcpkg features: ask for `openal-soft[pipewire,pulseaudio]`. The rest are set off in the portfile: change `-DALSOFT_BACKEND_JACK=OFF` and the like to `ON` | **Any Linux, macOS or Android port needs one of these for sound** |
| `core/rtkit.cpp`, `core/rtkit.h` | x86 Windows only | Change `-DALSOFT_RTKIT=OFF` to `ON` | Lets sound run at high priority on Linux |
| `core/mixer/mixer_neon.cpp` | x86 Windows only | Nothing: the portfile turns it on for 64-bit ARM | Sound mixing on ARM processors |
| `common/ghc_filesystem.h` | x86 Windows only | Nothing to bring back elsewhere | A stand-in for compilers without `std::filesystem` |
| The code in `common/almalloc.cpp` | x86 Windows only: the file stays, empty | Nothing to bring back elsewhere | Memory functions for macOS older than 10.13 |
| `modules/`, `alsoft-modules/` and the other `.cppm` files | All platforms | Remove them from the list, and change `-DALSOFT_ENABLE_MODULES=OFF` to `ON` | Nothing: C++ modules, not needed to build |
| `router/` | All platforms | Remove it from the list, and change `-DALSOFT_BUILD_ROUTER=OFF` to `ON` | Nothing: a Windows-only way to choose between OpenAL drivers |
| `utils/` | All platforms | Remove it from the list, and change `-DALSOFT_UTILS=OFF` to `ON` | `openal-info` lists the sound devices, which helps when getting sound working on a new platform |
| `examples/` | All platforms | Remove it from the list, and change `-DALSOFT_EXAMPLES=OFF` to `ON` | Sample programs, handy for testing a sound setup |
| `tests/` | All platforms | Remove it from the list, and change `-DALSOFT_TESTS=OFF` to `ON` | Nothing |
| `registry/`, `common/win_main_utf8.h` | All platforms | Remove them from the list | Nothing. `registry/` makes headers by hand, and the header served the examples and tools |
| `fmt-11.2.0/` | All platforms | Do not bring it back: it's an old copy of fmt, and the port uses the fmt library instead | Nothing |

### fmt (text formatting, used by OpenAL Soft)

| What was removed | To bring it back | What a port would need it for |
|---|---|---|
| `test/` | Remove it from the list, and change `-DFMT_TEST=OFF` to `ON` | Nothing |
| `support/python/` | Remove it from the list, and change `-DFMT_DOC=OFF` to `ON` | Nothing: it builds fmt's documentation |
| `src/fmt.cc` | Remove it from the list, and change `-DFMT_MODULE=OFF` to `ON` | Nothing: the C++ module form of the library |

### libvorbis (music and sound decoding)

| What was removed | To bring it back | What a port would need it for |
|---|---|---|
| `vq/`, `examples/`, `test/`, `contrib/`, `lib/psytune.c`, `lib/misc.c` | Remove them from the list. They aren't in the library's own build, so you would build them yourself | Nothing: tools for designing and tuning the encoder, sample programs, upstream's tests and fuzz setup, and a debug memory tracker |

### pthreads4w (threads on Windows)

| What was removed | To bring it back | What a port would need it for |
|---|---|---|
| `tests/` and `signal.c` | Remove them from the list in the portfile, and remove the two `vcpkg_replace_string` lines under it, which take `tests/` out of the Makefile's `clean` targets | Nothing. Other systems have threads built in, so a port drops this library altogether |

## Do not bring back

| What it was | Where it was | Why not |
|---|---|---|
| Prebuilt ENet libraries: `enet/enet.lib`, `enet/enet64.lib`, `enet/lib/enet.dll`, `enet/lib/enet.exp`, `enet/lib/enet.lib` | Commit `e4394fc` | They can't be matched to any source code, are unsigned, and carry no version. This project keeps no precompiled code from open-source libraries. Build ENet from source |
| Reading a shape from a raw copy of memory in a level (`SHPPRPRO`) | Commit `e4394fc`, `3dc/win95/chunk_shape.cpp` from line 3097 | A level is a file from outside the game, and this let one choose the addresses the game writes to. Any faster way of loading shapes has to pass the same checks as every other shape |
| Reading a strategy for a lift in another level file (`AVPEXSTR`, `AVP_External_Strategy_Chunk`) | Commit `e4394fc`, `3dc/avp/win95/strategy_chunk.cpp` and `.hpp`. The code that would have used it, `SetupExternalLift` in `3dc/avp/win95/object_setup.cpp`, was already `#if 0` | It read its numbers with no look at the chunk's size, so a short one read past its own chunk, into the next chunk or the 64 KB the game keeps after every loaded file, and one of an unknown kind asked for a negative amount of memory, which stopped the game. No stock level has one and nothing used it. A level with one now keeps it as plain bytes. If external lifts ever come back, read them as `AVP_Strategy_Chunk` is read: from a copy, and exactly the size of the chunk |
| Comparing a chunk with the level file on disk by reading its name into `char name[50]` (`file_equals`, `check_file`, `unlock_chunk`, `update_chunks_from_file`) | Commit `3d2531e`, `3dc/win95/chunk_object.cpp` line 142 and `3dc/win95/mishchnk.cpp` lines 155, 794 and 1368 | A level is a file from outside the game. These read up to its next zero byte with no limit, onto the stack |
| Reading a shape's grid of cubes by counts from the level file, without looking at the chunk's size (`ZSP_Data`) | Commit `3d2531e`, `3dc/win95/zsp.cpp` line 19 | A level is a file from outside the game. The counts choose how far it reads and how much memory it asks for, and nothing checks them against the chunk |
| Shapes made for the PlayStation and Saturn (`CONSHAPE`, `CONSTYPE`, `Console_Shape_Chunk`, `Console_Type_Chunk`) | Commit `e4394fc`, `3dc/win95/chunk_shape.cpp` and `.hpp` | Nothing in the game made or read a console shape. `CONSHAPE` was never registered, so a level's one was already plain bytes. `CONSTYPE` was registered for any parent and read 4 bytes with no look at its size, so a short one read past its own chunk, into the next chunk or the 64 KB the game keeps after every loaded file. No stock level has either. Its constructor also has a brace in the wrong place, so a loop that never ends would be waiting for anyone who registered it without moving the brace |
| Two checks that stopped a list when its room ran out: a shape set's list and a waypoint's links | Commit `0e8793c`, `3dc/win95/hierarchy_chunk.cpp` line 304 and `3dc/win95/chunk_waypoint.cpp` line 108 | The reads after them came to check their own room, so no file could be refused by these two alone, and no test could show they were needed. A list that runs out is still refused, by those reads, and its count is still measured against the chunk, so it can't go on for long. |

## Planned

| What it is | Where it is | Status |
|---|---|---|
| A debug memory tracker, with branches for PlayStation and Saturn | `3dc/mem3dc.c` | Still in the tree. Its row moves up when it goes |
