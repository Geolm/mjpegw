# mjpegw
Tiny C99 library for creating MJPEG AVI files. No external dependencies.

## Simple Interface

Use `mjpegw_open` to create the AVI file, call `mjpegw_add_frame` for each frame, then finalize with `mjpegw_close`.  

AVI files produces have been tested on ffmpeg, VLC and ffview.

## MJPEG

MJPEG is essentially an AVI container filled with standalone JPEG images.  
Each frame is an independent JPEG—no inter-frame prediction.

* Very simple, low CPU load during decoding  
* Good quality retention (no inter-frame artefacts)  
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
