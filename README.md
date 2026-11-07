# AvP

Hosting AvP 2000 DirectX 9 renderer by Barry Duncan: http://homepage.eircom.net/~duncandsl/avp/, thanks Barry and Team for rescuing the game from Abandonware status.

I've updated a couple headers that changed over the years/versions of Platform/Windows SDK/DirectX to get the code to compile in VS2019 community edition. 

Right now to compile the code you need:

1 - Visual Studio 2026 Community, with the C++ tools and vcpkg - https://visualstudio.microsoft.com/vs/community/

2 - DirectX 9 

You no longer need the old DirectX SDK. The Windows SDK that comes with Visual Studio has DirectX 9 itself, and vcpkg fetches Microsoft's D3DX helper library for you as the dxsdk-d3dx package.

3 - OpenAL 

Barry and team implemented AvP's audio using OpenAL. vcpkg builds OpenAL Soft from source, so there's nothing to download: https://openal-soft.org/

4 - AV Playback libraries 

Note: Original AVP source had all in-game videos disabled as the code was proprietary from RAD which made the Bink and Smack formats to run intro and in-game videos back in the day. Thanks to Barry & Team this functionality was completely re-implemented with open-source libraries! To get those to work you'll need:

Playback

https://github.com/xiph/ogg
https://github.com/xiph/vorbis

Decoding

http://homepage.eircom.net/~duncandsl/avp/ libsmackerdec and libbinkdec 

You need Ogg for Vorbis and you need both for AvP. vcpkg builds them from vcpkg.json, and Barry's decoders are in dependencies/ as source, built with the game. No .libs or .dlls are kept in this repository.

To build it, from a Developer Command Prompt for Visual Studio:

```
git clone https://github.com/Melanikus/AvP.git
cd AvP
msbuild win32\VS2026\AvP.sln /p:Configuration=Release /p:Platform=Win32
```

The first build takes a while, because vcpkg builds the libraries from source. The game and its DLLs end up in win32\VS2026\Win32\Release.

## Pull requests

A pull request into `avpx_main` runs four checks from `.github/workflows`: Game build, Tests, Snyk (the security scans) and Take-outs. Take-outs breaks one thing at a time in a copy of the code and checks that the tests notice. If it fails, `tests/takeouts/README.md`, under "When the Take-outs check fails", says what to do.
