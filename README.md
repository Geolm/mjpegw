# mjpegw

![C Standard](https://img.shields.io/badge/C-C99-F4A261)
[![Build Status](https://github.com/Geolm/mjpegw/actions/workflows/c-compilation.yml/badge.svg)](https://github.com/geolm/mjpegw/actions)

Tiny C99 library for creating MJPEG AVI files. No external dependencies.

## Simple Interface

Use `mjpegw_open` to create the AVI file, call `mjpegw_add_frame` for each frame, then finalize with `mjpegw_close`.  

AVI files produced have been tested on ffmpeg, VLC and ffplay. You can convert the video with ffmpeg to your favorite format.


## MJPEG

MJPEG is essentially an AVI container filled with standalone JPEG images.  
Each frame is an independent JPEG—no inter-frame prediction.

* Very simple, low CPU load during decoding  
* Very good quality retention (no inter-frame artefacts)  
* Larger file sizes compared to H.264 or other modern codecs

**Note:** Not related to MPEG-1.

## TinyJPEG

**Huge** thanks to [TinyJPEG](https://github.com/serge-rgb/TinyJPEG) library.

## Building and Running the Test

From the project root, open a terminal and type:
```
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build .
./test
```
