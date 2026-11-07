# Dreamcast Disc Extractor

A standalone Windows and Linux utility for extracting files from your own
Dreamcast disc images. Choose an image and an output folder, inspect its details,
and follow extraction progress. No game data is included.

## Features

- Reads GDI, CUE/BIN, standalone data-track BIN, CHD and CDI images.
- Shows the title, region, game ID, date, publisher, track count and file count
  when the disc provides them.
- Extracts the ISO 9660 filesystem with current-file and total progress,
  transfer speed, elapsed time and an estimate of the remaining time.
- Runs through a resizable desktop interface or from the command line.
- Refuses to overwrite existing files. The GUI Abort control removes the unfinished
  file and leaves completed files in place.
- Bundles SDL3, the CHD decoder, its font and the application icon; users do not
  need Python, an emulator or an external extraction utility.

## Graphical interface

Launch **Dreamcast Disc Extractor** without arguments. Browse to the image and
an existing output folder, check the disc details, then select **Extract**.

Paths can also be typed into the fields. Press Enter after typing the image
path to inspect it. This is useful on Linux if a desktop file picker is not
available.

## Command line

Extract into an existing folder:

```powershell
& '.\Dreamcast Disc Extractor.exe' --extract 'D:\Games\Game.gdi' --output 'D:\Games\data' | Out-Host
```

```sh
'./Dreamcast Disc Extractor' --extract '/games/Game.gdi' --output '/games/data'
```

Inspect an image without extracting it, or show the command reference:

```sh
'./Dreamcast Disc Extractor' --info '/games/Game.gdi'
'./Dreamcast Disc Extractor' --help
```

| Option | Purpose |
| --- | --- |
| `--extract <image>` | Inspect and extract a GDI, CUE, BIN, CHD or CDI image. |
| `--output <folder>` | Set the existing destination folder for extraction. |
| `--info <image>` | Print available disc details without extracting. |
| `--help` | Print the command reference. |

The command returns `0` on success, `1` on an image or extraction error and
`2` for invalid arguments. Quote paths containing spaces. The Windows example
pipes output to `Out-Host` so PowerShell waits for the graphical-subsystem
executable to finish. Use the same pipe with `--info` and `--help`.

## Build

Use CMake 3.22 or newer and a C++20 compiler:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
cmake --install build --config Release --prefix dist
```

The first build fetches pinned SDL3, libchdr and stb source. SDL3, libchdr and
the Windows C++ runtime are linked statically. Linux system libraries remain
required. To build offline, set `SDL_SOURCE_DIR`, `CHDR_SOURCE_DIR` and
`STB_SOURCE_DIR` to local checkouts of the pinned revisions.

## Format and verification notes

The image must contain an ISO 9660 filesystem. Unsupported sector modes and
multi-extent files produce an error. The CUE reader supports binary track files.
Support for all disc variants is not yet verified.

Windows and Linux builds pass. Synthetic images have been extracted in each
supported format, including 2336-byte CDI sectors. One user-supplied GDI
extracted all 144 files on Windows. Other real images, Linux runtime,
performance and the redesigned interface still need testing.

## Third-party notices

The CDI parser is based on OpenReliant's `src/formats/cdimage/cdi.zig` under
[MPL 2.0](LICENSE-MPL-2.0.txt). CHD decoding uses
[libchdr](https://github.com/rtissera/libchdr) under its
[BSD license](LICENSE-libchdr.txt). The install step includes the SDL3 and
decoder dependency notices.

The interface embeds Roboto under the Open Font License (`assets/OFL.txt` in
the source and `LICENSE-Roboto.txt` in installed packages). It uses
[stb](LICENSE-stb.txt) to rasterize it. The application icon is an
AI-generated cartoon illustration created for this utility.
