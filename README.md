<h1 align="center">Pawn TTS</h1>

<p align="center">
  Real-time clientless text-to-speech and 3D spatial voice streaming engine for SA-MP and open.mp servers.
</p>

<p align="center">
  <a href="#compatibility"><img alt="SA-MP" src="https://img.shields.io/badge/SA--MP-0.3.7-2f6feb?style=flat-square"></a>
  <a href="#compatibility"><img alt="open.mp" src="https://img.shields.io/badge/open.mp-supported-00a86b?style=flat-square"></a>
  <a href="#build-from-source"><img alt="C++17" src="https://img.shields.io/badge/C++-17-00599c?style=flat-square"></a>
  <a href="#license"><img alt="License" src="https://img.shields.io/badge/License-MIT-yellow?style=flat-square"></a>
  <a href="#release-files"><img alt="Platforms" src="https://img.shields.io/badge/Linux%20%7C%20Windows-x86__64%20%7C%20x86-111827?style=flat-square"></a>
</p>

<p align="center">
  <a href="#installation">Installation</a>
  ·
  <a href="#quick-start">Quick Start</a>
  ·
  <a href="#native-api">Native API</a>
  ·
  <a href="#configuration">Configuration</a>
  ·
  <a href="#build-from-source">Build</a>
</p>

---

Pawn TTS brings real-time, clientless text-to-speech (TTS) and spatial audio streaming to SA-MP and open.mp Pawn servers. Players on standard PC (GTA: San Andreas) and Android mobile launchers do not require any client modifications, ASI plugins, or external software. Audio is streamed natively through the GTA audio engine via `PlayAudioStreamForPlayer` with full 3D spatial sound (distance attenuation and stereo panning).

## Overview

| Item | Detail |
| --- | --- |
| Plugin name | `pawn_tts` |
| Target | SA-MP (0.3.7) and open.mp |
| Plugin type | Legacy SA-MP plugin API & Native open.mp Component |
| Language | C++17 |
| Audio streaming | Native HTTP chunked/range audio stream (`audio/mpeg`) |
| Default HTTP port | `7788` |
| Default TTS voice | Indonesian (`id`), English (`en`), 100+ languages |
| Default provider | Google Translate TTS (free cloud, zero config required) |
| Offline provider | Piper TTS or custom HTTP endpoint |
| Caching engine | RAM LRU cache + Disk caching with SHA-256 deduplication |
| Included platforms | Linux x86_64, Linux x86 (32-bit), Windows x86 (32-bit) |
| Pawn include | `pawno/include/pawn_tts.inc` |

## Features

| Feature | Native / Stock | Notes |
| --- | --- | --- |
| Check engine status | `TTS_IsReady` | Returns true if the HTTP stream server and audio engine are running. |
| Get streaming URL | `TTS_GetBaseURL` | Retrieves the active base stream URL (e.g. `http://ip:7788/audio/`). |
| Set public URL | `TTS_SetPublicURL` | Overrides the public URL (useful behind reverse proxies, NAT, or domain names). |
| Private speech | `TTS_Speak` | Streams voice privately to a single player in 2D stereo. |
| Broadcast speech | `TTS_SpeakToAll` | Streams voice to all connected players simultaneously. |
| 3D spatial speech | `TTS_SpeakAtPos` | Plays 3D positioned voice at world coordinates (X, Y, Z) with falloff distance. |
| 3D speech for player | `TTS_SpeakAtPosForPlayer` | Plays positioned 3D voice audible only to a targeted player. |
| Player body audio | `TTS_SpeakFromPlayer` | Plays 3D voice originating directly from a player's physical coordinate. |
| Stop player audio | `TTS_Stop` | Immediately stops any active speech stream for a player. |
| Stop all audio | `TTS_StopAll` | Immediately stops active speech streams for all players. |
| Pre-cache audio | `TTS_Precache` | Asynchronously generates and caches frequent phrases for instant 0 ms playback. |
| Check cached status | `TTS_IsCached` | Checks if a phrase has already been synthesized and cached. |
| Cache statistics | `TTS_GetStats` | Queries memory cache count, disk cache count, and total cache size in KB. |
| Purge cache | `TTS_ClearCache` | Clears memory and optionally disk audio cache. |
| Synthesis callback | `OnTTSGenerated` | Called when a speech synthesis request finishes generating. |
| Playback callback | `OnTTSReady` | Dispatches audio stream to the client via `PlayAudioStreamForPlayer`. |

## Release Files

Prebuilt release packages are available in `release/`.

| File | Format | Contents |
| --- | --- | --- |
| `release/pawn-tts-v0.1.0-windows-x86.zip` | ZIP | Windows 32-bit release archive (legacy plugin & native component). |
| `release/pawn-tts-v0.1.0-linux-x86.zip` | ZIP | Linux 32-bit release archive (legacy plugin & native component). |
| `release/pawn-tts-v0.1.0-linux-x86_64.tar.gz` | tar.gz | Linux 64-bit release archive (legacy plugin & native component). |
| `release/pawn-tts-v0.1.0.zip` | ZIP | Universal bundle containing all platforms and examples. |

Package layout:

```text
pawn-tts-v0.1.0/
  plugins/
    pawn_tts.so
    pawn_tts.dll
  components/
    pawn_tts.so
    pawn_tts.dll
  pawno/include/
    pawn_tts.inc
  examples/
    example.pwn
    commands.pwn
  pawn_tts.json
  README.md
  LICENSE
```

## Compatibility

| Runtime | Status | Notes |
| --- | --- | --- |
| SA-MP Windows server | Supported | Use `plugins/pawn_tts.dll` (32-bit for classic SA-MP Windows servers). |
| SA-MP Linux server | Supported | Use `plugins/pawn_tts.so` (32-bit x86 for classic SA-MP Linux servers). |
| open.mp Windows server | Supported (Component & Plugin) | Native component: `components/pawn_tts.dll`. Legacy plugin: `plugins/pawn_tts.dll`. |
| open.mp Linux server | Supported (Component & Plugin) | Native component: `components/pawn_tts.so`. Legacy plugin: `plugins/pawn_tts.so`. |
| Android SA-MP clients | Supported | Fully clientless; streams through native GTA audio stream engine. |
| PC SA-MP clients | Supported | 100% stock GTA San Andreas client, no ASI plugins required. |

Pawn TTS does not hook RakNet, patch memory, or depend on internal server offsets. It registers standard Pawn natives and runs a high-performance HTTP stream listener to deliver audio directly to GTA game clients.

## Installation

### 1. Install Files

#### For SA-MP (Legacy Plugin)

Copy the files into your server directory:

```text
plugins/pawn_tts.so        -> Linux server plugins folder (use x86 32-bit for classic SA-MP)
plugins/pawn_tts.dll       -> Windows server plugins folder
pawno/include/pawn_tts.inc -> Pawn compiler include folder
pawn_tts.json              -> Server root folder
```

#### For open.mp (Native Component - Recommended)

Copy the files into your server directory:

```text
components/pawn_tts.so     -> Linux server components folder
components/pawn_tts.dll    -> Windows server components folder
pawno/include/pawn_tts.inc -> Pawn compiler include folder
pawn_tts.json              -> Server root folder
```

For Qawno or custom build systems, place `pawn_tts.inc` in the include path used by your compiler.

### 2. Load The Plugin / Component

#### For SA-MP `server.cfg`:

```text
plugins pawn_tts
```

On Linux setups where explicit extensions are required:

```text
plugins pawn_tts.so
```

#### For open.mp `config.json` (Native Component Mode - Recommended):

```json
{
  "components": [
    "pawn_tts"
  ]
}
```

#### For open.mp `config.json` (Legacy Plugin Mode):

```json
{
  "pawn": {
    "legacy_plugins": ["pawn_tts"]
  }
}
```

### 3. Include In Pawn

```pawn
#include <pawn_tts>
```

## Quick Start

```pawn
#include <a_samp>
#include <pawn_tts>

public OnGameModeInit()
{
    // Pre-cache frequent phrases for instant 0 ms playback
    TTS_Precache("Selamat datang di server!", "id");
    TTS_Precache("Waktunya Payday! Silakan ambil gaji Anda di bank.", "id");
    return 1;
}

public OnPlayerConnect(playerid)
{
    // Private 2D stereo welcome greeting
    TTS_Speak(playerid, "Selamat datang di server!", "id");
    return 1;
}

// 3D Proximity Voice Chat: speech originates from the player character in world space
public OnPlayerText(playerid, text[])
{
    TTS_SpeakFromPlayer(playerid, text, "id", 25.0);
    return 1;
}
```

## Native API

| Native | Signature | Return | Description |
| --- | --- | ---: | --- |
| `TTS_IsReady` | none | `bool` | Checks whether the HTTP server and audio engine are initialized. |
| `TTS_GetBaseURL` | `output[], maxlen = sizeof(output)` | `1` | Copies the current base streaming URL into the output string. |
| `TTS_SetPublicURL` | `const url[]` | `1` | Dynamically overrides the public streaming URL. |
| `TTS_Precache` | `const text[], const voice[] = "id", Float:speed = 1.0` | `bool` | Synthesizes and caches an audio phrase in the background without playing it. |
| `TTS_IsCached` | `const text[], const voice[] = "id", Float:speed = 1.0` | `bool` | Checks whether a phrase is already available in memory or disk cache. |
| `TTS_ClearCache` | `bool:clear_disk = false` | `1` | Clears memory cache (and optionally disk cache). |
| `TTS_GetStats` | `&cached_mem, &cached_disk, &total_bytes_kb` | `1` | Retrieves current cache counts and total memory footprint in KB. |
| `TTS_Speak` | `playerid, const text[], const voice[] = "id", Float:speed = 1.0` | `bool` | Plays private 2D stereo audio directly to a player. |
| `TTS_SpeakToAll` | `const text[], const voice[] = "id", Float:speed = 1.0` | `bool` | Broadcasts audio to all connected players on the server. |
| `TTS_SpeakAtPos` | `const text[], const voice[] = "id", Float:x, Float:y, Float:z, Float:distance = 30.0, Float:speed = 1.0` | `bool` | Plays 3D spatial audio at specific world coordinates with distance falloff. |
| `TTS_SpeakAtPosForPlayer` | `playerid, const text[], const voice[] = "id", Float:x, Float:y, Float:z, Float:distance = 30.0, Float:speed = 1.0` | `bool` | Plays 3D spatial audio at specific coordinates audible only to a targeted player. |
| `TTS_SpeakFromPlayer` | `speakerid, const text[], const voice[] = "id", Float:distance = 25.0, Float:speed = 1.0` | `bool` | Plays 3D spatial audio emanating from a player's character body. |
| `TTS_Stop` | `playerid` | `1` | Stops any active audio stream for a specific player. |
| `TTS_StopAll` | none | `1` | Stops active audio streams for all connected players. |

## Callbacks

```pawn
forward OnTTSGenerated(const hash[], const text[], const voice[], bool:success);
forward OnTTSReady(playerid, const hash[], const url[], Float:x, Float:y, Float:z, Float:distance, usepos);
```

### OnTTSGenerated

Called asynchronously when a speech synthesis request finishes:

```pawn
public OnTTSGenerated(const hash[], const text[], const voice[], bool:success)
{
    if (success) {
        printf("[TTS] Audio generated: hash=%s voice=%s", hash, voice);
    }
    return 1;
}
```

### OnTTSReady

Dispatched automatically by the include hook to play audio via `PlayAudioStreamForPlayer`. If you define `TTS_OnTTSReady`, you can customize or filter stream playback before dispatch.

## Configuration

Configuration is managed via `pawn_tts.json` in the server root folder:

```json
{
  "port": 7788,
  "bind_ip": "0.0.0.0",
  "public_url": "",
  "default_voice": "id",
  "cache_dir": "cache/tts",
  "cache_memory_limit": 256,
  "provider": "google",
  "custom_http_url": "http://127.0.0.1:5000/tts"
}
```

| Key | Type | Default | Description |
| --- | --- | --- | --- |
| `port` | Integer | `7788` | Port for the embedded HTTP audio server. |
| `bind_ip` | String | `"0.0.0.0"` | Network interface to bind the HTTP server. |
| `public_url` | String | `""` | Public base URL sent to clients (e.g. `"http://play.example.com:7788"`). If empty, auto-detects from client connection. |
| `default_voice` | String | `"id"` | Default ISO 639-1 voice language code (`"id"`, `"en"`, `"es"`, etc.). |
| `cache_dir` | String | `"cache/tts"` | Directory path for persistent MP3 disk cache. |
| `cache_memory_limit` | Integer | `256` | Maximum memory cache size in megabytes (MB). |
| `provider` | String | `"google"` | TTS synthesis backend (`"google"`, `"piper"`, or `"custom"`). |
| `custom_http_url` | String | `""` | Endpoint URL when using custom or local HTTP TTS engines. |

## 3D Spatial Audio Mechanics

When using `TTS_SpeakAtPos` or `TTS_SpeakFromPlayer`, audio streams leverage GTA San Andreas's built-in BASS audio engine:

- **Distance Falloff**: Audio volume decreases smoothly from the center coordinate out to the configured radius (`distance`).
- **Stereo Panning**: Audio dynamically pans between the player's left and right audio channels based on their camera and character orientation relative to the sound source.
- **Client Compatibility**: Fully operational on standard PC GTA:SA clients and official Android mobile launchers without any custom `.asi` or APK modifications.

## Build From Source

This repository uses CMake (version 3.20+).

### Requirements

| Target | Requirement |
| --- | --- |
| Linux `.so` (x86_64) | CMake 3.20+, C++17 compiler (`g++` / `clang++`) |
| Linux `.so` (x86 32-bit) | CMake 3.20+, Multilib 32-bit C++ compiler (`g++ -m32`) |
| Windows `.dll` (x86 32-bit) | CMake 3.20+, MinGW toolchain (`i686-w64-mingw32-g++-posix`) or MSVC |

### Linux (Native x86_64)

```bash
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Output:
- `build/plugins/pawn_tts.so` (Legacy SA-MP plugin)
- `build/components/pawn_tts.so` (Native open.mp component)

### Linux (32-bit x86 for Classic SA-MP)

```bash
cmake -B build-linux-x86 -S . \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_FLAGS="-m32" \
  -DCMAKE_SHARED_LINKER_FLAGS="-m32"
cmake --build build-linux-x86 --config Release
```

Output:
- `build-linux-x86/plugins/pawn_tts.so` (Legacy SA-MP plugin)
- `build-linux-x86/components/pawn_tts.so` (Native open.mp component)

### Windows (Cross-compile with MinGW)

```bash
cmake -B build-win -S . \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_SYSTEM_NAME=Windows \
  -DCMAKE_C_COMPILER=i686-w64-mingw32-gcc-posix \
  -DCMAKE_CXX_COMPILER=i686-w64-mingw32-g++-posix
cmake --build build-win --config Release
```

Output:
- `build-win/plugins/pawn_tts.dll` (Legacy SA-MP plugin)
- `build-win/components/pawn_tts.dll` (Native open.mp component)

## Troubleshooting

| Problem | Fix |
| --- | --- |
| `Failed (plugins/pawn_tts.so: wrong ELF class: ELFCLASS64)` | Standard SA-MP Linux server is 32-bit. Use the 32-bit Linux binary from `build-linux-x86/plugins/pawn_tts.so` or `release/pawn-tts-v0.1.0-linux-x86.zip`. |
| `Failed (plugins/pawn_tts.so: cannot open shared object file)` | Verify that `pawn_tts.so` is inside your `plugins/` directory and referenced correctly in `server.cfg`. |
| Players cannot hear speech | Ensure TCP port `7788` is open in your firewall/security group, or configure `"public_url"` in `pawn_tts.json` with your server's reachable public IP or domain. |
| Audio stream cuts off or stutters | Increase `"cache_memory_limit"` in `pawn_tts.json` and use `TTS_Precache` during `OnGameModeInit` for frequently repeated server phrases. |
| `undefined symbol: TTS_Speak` while compiling Pawn | Ensure `pawn_tts.inc` is placed in `pawno/include/` and `#include <pawn_tts>` is present after `#include <a_samp>`. |

## License

Pawn TTS is released under the MIT License.
