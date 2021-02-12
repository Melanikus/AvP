# AvP

Hosting AvP 2000 DirectX 9 renderer by Barry Duncan: http://homepage.eircom.net/~duncandsl/avp/ in Github, thanks Barry and Team for rescuing the game from Abandonware status.

I've updated a couple headers that changed over the years/versions of Platform/Windows SDK/DirectX to get the code to compile in VS2019 community edition. 

Right now to compile the code you need:

Visual Studio 2019 Community - https://visualstudio.microsoft.com/vs/community/

DirectX 9 SDK - Can be found around the web, you want the June 2010 version, it'll likely throw S1023 error because you have a newer redistributable for some of the packages it's installing, if you get that refer to https://docs.microsoft.com/pt-br/troubleshoot/windows/win32/s1023-error-when-you-install-directx-sdk (run both commands and try again)

Original AVP source had all in-game videos disabled as the code was proprietary from RAD which made the Bink and Smack formats to run intro and in-game videos back in the day. Thanks to Barry & Team this functionality was completely re-implemented with open-source libraries! To get those to work you'll need:

For playback

https://github.com/xiph/ogg
https://github.com/xiph/vorbis
https://github.com/xiph/theora

For decoding 

http://homepage.eircom.net/~duncandsl/avp/ libsmackerdec and libbinkdec 

You need Ogg/Vorbis for Theora and you need Ogg for Vorbis and you need all of the for AvP, Barry's code has the .libs and .dlls but you'll need to compile and link your own playback libraries, remember to set all of them as multi-threaded DLLs and to copy the DLLs to your output directory (game) when testing your exe. The process is pretty smooth as long as you know how to reference include and lib output directories in the projects it should run smoothly. 
