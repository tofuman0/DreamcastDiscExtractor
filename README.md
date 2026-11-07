# Dreamcast Disc Extractor

This standalone utility extracts files from user-supplied Dreamcast disc images.
It does not include game data. The app is in development and needs testing with
real images on Windows and Linux.

## Build

Use CMake 3.22 or newer and a C++20 compiler:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
cmake --install build --config Release --prefix dist
```

The first build fetches pinned SDL3, libchdr and stb source. SDL3, libchdr and the
Windows C++ runtime are linked statically. Linux system libraries remain
required. To build offline, set `SDL_SOURCE_DIR`, `CHDR_SOURCE_DIR` and `STB_SOURCE_DIR` to local
source checkouts of the pinned revisions.

## Formats and limitations

GDI, CUE/BIN, CHD and CDI have initial readers. The extracted filesystem must
be ISO 9660; unsupported sector modes and multi-extent files produce an error.
The current CUE reader supports binary track files. You can type a path and press Enter to inspect an image if the system file picker
is unavailable. Existing output files are not overwritten. Cancelling removes the partially written current file but keeps
completed files. Output folders must already exist.

Synthetic GDI, CUE, BIN, CHD and CDI examples have been extracted, including
2336-byte CDI sectors. One user-supplied GDI extracted all 144 files on Windows.
Other real disc variants, Linux operation, performance and the resized UI
still need verification.

## Third-party notices

The CDI parser is based on OpenReliant's `src/formats/cdimage/cdi.zig` under
[MPL 2.0](LICENSE-MPL-2.0.txt). CHD decoding uses
[libchdr](https://github.com/rtissera/libchdr) under its
[BSD license](LICENSE-libchdr.txt). The install step includes the SDL3 and decoder dependency notices.

The interface embeds Roboto under the [Open Font License](assets/OFL.txt) and
uses [stb](LICENSE-stb.txt) to rasterize it.

The application icon is an original AI-generated cartoon illustration of a
Dreamcast console, created for this utility.
