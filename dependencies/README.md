# Third-party dependencies

The libraries in this folder are **not** covered by the repository's `License.txt` (Rebellion's AvP source licence
and Barry Duncan's 2010 renderer licence). Each one keeps its own licence, in its own `COPYING` file.

## libbinkdec and libsmackerdec
Bink and Smacker video decoders written by **Barry Duncan** (2011) for this port, so the game plays its FMVs without
RAD Game Tools' proprietary `binkw32.dll` / `smackw32.dll`.

- **Licence:** GNU Lesser General Public License, version 2.1 or (at your option) any later version. See
  `libbinkdec/COPYING` and `libsmackerdec/COPYING`; every source file carries the LGPL header.
- **Based on FFmpeg** (https://ffmpeg.org/): the Bink decoder by Kostya Shishkov, the Smacker decoder by Konstantin
  Shishkov, and FFmpeg's FFT / DCT / MDCT / RDFT code. The FFT / DCT files differ from FFmpeg n0.9 only by
  portability edits (allocation, disabled SIMD init, include paths, float literals), as their own headers note.
- **Origin:** Barry Duncan's AvP page, http://homepage.eircom.net/~duncandsl/avp/ (now reached through the Wayback
  Machine). Archives downloaded 2026-11-03:
  - `libbinkdec.7z` sha256 `CC5C2476C264B92E961338CF0BC7166A853A7DD91808F543CF0432173A236E48`
  - `libsmackerdec.7z` sha256 `D4609BE8367DE174FCC59AA35F53EEFAF80E6B2235B846B71AF99D674EAA801D`
- **What is here:** the source, headers, `.def` export lists, the VS2010 projects and `COPYING`, unchanged from the
  archives except where a change notice says otherwise. The prebuilt 2012 DLLs / import libraries from the archives
  are deliberately **not** included: build from source.
- **Changes:** every file changed after import carries a notice naming the change and its date, as LGPL 2.1 section 2
  requires. The commit history is the full record.
- **Building:** Release with the Runtime Library set to Multi-threaded DLL (`/MD`), as Barry's instructions say. The
  game links the import libraries and loads `libbinkdec.dll` / `libsmackerdec.dll` from its folder. Because they are
  separate DLLs, users can replace them, which the LGPL requires for a non-LGPL program.
