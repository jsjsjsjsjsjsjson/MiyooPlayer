# MiyooPlayer

MiyooPlayer is a small local media player for MiyooCFW-style framebuffer/fbcon systems. It is a real native player: it opens files with FFmpeg/libav, decodes audio and video in-process, renders with SDL, and uses SDL audio output. It is not an `ffplay` wrapper.

The default build targets SDL 1.2 because that is usually the most suitable backend for MiyooCFW fbcon devices. SDL2 can be enabled with `-DMIYOO_USE_SDL2=ON` if your image has a working SDL2 framebuffer setup.

## Features

- File browser starting at `/mnt/videos`, `MIYOO_PLAYER_DIR`, or a command-line directory.
- MP4/MKV/AVI/MOV/MPEG and MP3/OGG/WAV/FLAC/AAC/M4A/Opus files, depending on the FFmpeg codecs in your sysroot.
- FFmpeg demuxing and decoding through libavformat/libavcodec.
- Video scaling/conversion through libswscale to RGB565 for 320x240 output.
- Audio resampling through libswresample to configurable stereo signed 16-bit PCM output.
- SDL framebuffer/fbcon rendering with `SDL_NOMOUSE=1` and dummy mouse driver variables set at startup.
- SDL audio callback with a bounded low-memory queue.
- Basic A/V sync: audio continuity is prioritized, and late video frames may be dropped.
- Pause/resume, seek, volume, progress, duration, status OSD.
- Browser menu with sort by name/type/time, hidden file toggle, refresh, and playback order selection.
- Home screen with playlist cards, generated covers, quick browse access, and a small action menu.
- `.m3u8` playlists stored under `<media-root>/.miyoo-player/playlists`.
- Create empty playlists, add selected files manually, or recursively scan a folder into a playlist.
- Temporary playback queues for marked files and one-off folder opens; these do not create saved playlists.
- FFmpeg-based conversion panel with target format, sample rate, bit depth, and bitrate choices.
- Separate settings page for output sample rate, SDL audio buffer, PCM queue size, UI refresh interval, seek step, OSD timeout, hidden files, default playback order, and theme.
- Settings are stored in `<media-root>/.miyoo-player/config.ini`.
- Playback info panel showing current file, time, volume, audio queue, video size, and playback modes.
- Playback order modes: sequential, list loop, repeat one, and shuffle.
- Playback OSD uses compact icons for play/pause, volume, and order mode, refreshes during playback, and auto-hides after inactivity.
- Release builds use `-Ofast`, `-fomit-frame-pointer`, and `-fno-rtti`; app-level timing uses `float` instead of `double`.

## Controls

The exact Miyoo button mapping depends on the SDL keymap in the firmware. The player handles common mappings:

| Action | Keys |
| --- | --- |
| Move selection | D-pad Up/Down |
| Page in browser | L2/R2 |
| Open/play | A, START, Y, D-pad Right |
| Back/stop | B, R1 |
| Menu | SELECT, L1 |
| Pause/resume during playback | START, A, Y |
| Mark file in browser | X |
| Cycle playback order during playback | X |
| Seek -10s/+10s | D-pad Left/Right, L2/R2 |
| Previous/next track during playback | D-pad Up/Down |
| Quit | RESET |

Detected SDL mapping for this target:

| Button | SDL name |
| --- | --- |
| D-pad | Up/Down/Left/Right |
| SELECT | Escape |
| START | Return |
| RESET | Right Ctrl |
| A/B/X/Y | Left Alt / Left Ctrl / Left Shift / Space |
| L1/L2/R1/R2 | Tab / PageUp / Backspace / PageDown |

Home starts on playlist cards. Left/Right selects a playlist, A/START opens that playlist, B opens the file browser, X creates a new empty playlist, SELECT opens the home menu, and R2 scans the media root into a playlist. The home menu can create playlists, scan the root, browse files, open settings, and delete the selected playlist with a confirmation press.

In a playlist, Up/Down selects a file, A/START plays from that file, B returns home, X removes the selected item, and SELECT opens the playlist menu. The playlist menu can play, remove an item, or delete the whole playlist with a confirmation press.

In the browser, X marks/unmarks media files, B/Left goes up one folder and returns home when already at the media root. The browser menu also has a `HOME` row for an explicit return path. In the browser menu, Up/Down selects a row and A/START/Left/Right changes or activates it. You can open marked files as a temporary queue, open the selected directory as a temporary queue, add the selected file to the current playlist, scan the current folder into a new playlist, open the converter page, create an empty playlist, refresh, sort, and change playback order. `ORDER` cycles through `SEQ`, `LOOP`, `ONE`, and `RAND`. During playback, X cycles the order without opening the menu, while Up/Down switches previous/next track.

The converter is a separate page opened from the browser menu with `CONVERT`. Its rows are:

- `FMT`: `MKV COPY`, `MP4 COPY`, `WAV PCM`, `OGG VORB`, `MP3`, `AAC`.
- `RATE`: 22050, 32000, 44100, or 48000 Hz.
- `DEPTH`: 16, 24, or 32-bit PCM for WAV output.
- `BR`: bitrate for compressed audio targets.
- `START`: run the current conversion profile for the selected file.

`MKV COPY` and `MP4 COPY` remux streams without re-encoding. `WAV PCM` decodes audio and writes PCM using the selected sample rate and bit depth. `OGG VORB`, `MP3`, and `AAC` use FFmpeg encoders if those encoders are present in the target firmware build.

The settings page is opened from the home menu. Use Up/Down to select a row and A/Left/Right to change it. START saves immediately from any row; the `SAVE` row also writes `<media-root>/.miyoo-player/config.ini`. Theme, hidden-file display, seek step, OSD timeout, UI refresh interval, and default order are applied immediately for future screens/playback. Output sample rate, SDL audio buffer, and queue size are loaded before SDL audio opens, so restart the player after saving those audio-device settings.

## Build

The expected toolchain root is `/opt/miyoo`, with pkg-config files in the sysroot for FFmpeg and SDL.
`Release` builds are optimized with `-Ofast`; use `-DCMAKE_BUILD_TYPE=Debug` if you need a debuggable binary.

```sh
cmake -S . -B build-miyoo \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=cmake/miyoo-toolchain.cmake

cmake --build build-miyoo -j
```

This builds two binaries:

- `build-miyoo/miyoo-player` - the media player.
- `build-miyoo/miyoo-keytest` - SDL keycode tester for mapping Miyoo buttons.

If your toolchain is not in `/opt/miyoo`:

```sh
cmake -S . -B build-miyoo \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=cmake/miyoo-toolchain.cmake \
  -DMIYOO_ROOT=/path/to/miyoo/toolchain
```

SDL2 build, only if SDL2 fbcon/framebuffer works on your image:

```sh
cmake -S . -B build-miyoo-sdl2 \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=cmake/miyoo-toolchain.cmake \
  -DMIYOO_USE_SDL2=ON
cmake --build build-miyoo-sdl2 -j
```

## Install To Device

Copy the binary to the SD card or device:

```sh
mkdir -p /media/$USER/MIYOO/App/MiyooPlayer
cp build-miyoo/miyoo-player /media/$USER/MIYOO/App/MiyooPlayer/
cp build-miyoo/miyoo-keytest /media/$USER/MIYOO/App/MiyooPlayer/
```

Put media under `/mnt/videos`, or pass a directory in the launcher command.
Playlists are written under `/mnt/videos/.miyoo-player/playlists` when `/mnt/videos` is the active media root.
Exports are written under `/mnt/videos/.miyoo-player/exports`.
Settings are written under `/mnt/videos/.miyoo-player/config.ini`.

Example gmenu2x launcher: `gmenu2x/miyoo-player.lnk`

```ini
title=Miyoo Player
description=Local video and audio player
exec=/mnt/App/MiyooPlayer/miyoo-player
params=/mnt/videos
selector=false
clock=720
```

Adjust paths for your SD card layout.

## Key Test

Run this on the device to discover SDL key mappings:

```sh
/mnt/App/MiyooPlayer/miyoo-keytest
```

Press each physical button and write down the `KEY` number and `NAME` shown on screen. The same lines are also printed to stderr if you launch it from a shell. Exit with RESET, or `Q` if you run it from a keyboard.

Example gmenu2x launcher: `gmenu2x/miyoo-keytest.lnk`

## Performance Notes

The f1c100s target is very constrained: ARMv5TE, 720 MHz, 32 MB RAM plus swap. Recommended files:

- 320x240 or lower video.
- H.264 Baseline, MPEG-4 ASP, or MPEG-1/2 at modest bitrates.
- 24 or 30 fps; lower is better.
- Stereo audio at 44.1 kHz or 48 kHz. If high-bitrate audio stutters, try 32000 or 22050 Hz output in Settings, a 2048 SDL audio buffer, and a 128 KB or 192 KB PCM queue.
- Avoid high-profile H.264, B-frame-heavy encodes, 720p/1080p files, high-bitrate FLAC plus video, and large subtitles.

The first version intentionally avoids complex buffering and worker threads. That keeps memory use low and behavior predictable, but it also means slow codecs or oversized files will drop video frames.

## Project Layout

- `src/main.cpp` - app loop, browser/player mode switching.
- `src/sdl_platform.*` - SDL init, framebuffer setup, audio device, timing.
- `src/file_browser.*` - local media directory scanning and navigation.
- `src/playlist_manager.*` - `.m3u8` playlist persistence and recursive folder import.
- `src/settings.*` - local config file persistence and setting value cycles.
- `src/media_tools.*` - FFmpeg remux/export helpers.
- `src/input.*` - SDL key mapping to player actions.
- `src/ui.*` - built-in bitmap text UI, browser menu, and playback OSD.
- `src/player.*` - FFmpeg demux/decode loop, pause, seek, sync.
- `src/audio_queue.*` - bounded PCM queue consumed by the SDL audio callback.
- `src/video_renderer.*` - libswscale RGB565 conversion and SDL blit.
- `src/ffmpeg_compat.*` - small compatibility helpers for FFmpeg/libav versions.

## Future Work

- Firmware-specific keymap profiles.
- Resume position files.
- Recent folders.
- Playlist import/export.
- Optional image/subtitle display.
- Better dropped-frame accounting and on-screen diagnostics.
- Device-specific packaging scripts.
