# FFmpegHapForgePlayer (WIP)

Very simple cross-platform (only tested on mac and windows) code to playback a HAP video file using FFMPEG for demuxing and HapDecode for decoding and The-Forge for rendering.
It handles Hap, Hap Alpha, HapQ & HapQ+Alpha.

The idea in this project is to demonstrate how to:
- demux a HAP video file using FFmpeg / libavformat
- decompress the snappy compression using HapDecode
- send the compressed DXT buffer to a The-Forge texture (or two if HapQ+Alpha but this format is not yet supported in FFMPEG...)
- render the texture using The-Forge (with a specific shader in case of HapQ)

It uses The-Forge for rendering and Native code for opening windows (as this is not a supported feature of The-Forge (At least not when using it purely as a library)).

Any suggestion is welcome.

# Linux 

# FIXME
Currently only supports macos
Refactor main run loop for a smoother experience

Only clang can compile the binary. Install clang is mandatory (apt-cache search clang to choose the one you need)
+ don't forget to install libsnappy, libblocksruntime  and libdispatch

To make the build you'll have to hack the Makefile :

1. replace gcc and g++ with clang
2. add -fblocks in CFLAGS and CXXFLAGS

e.g. on LinuxMint : 
sudo apt-get install libdispatch0 libdispatch-dev libsnappy-dev libsnappy1v5 libblocksruntime-dev libblocksruntime0


