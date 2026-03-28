# minimp4

[![CI](https://github.com/YongRui0402/minimp4/actions/workflows/ci.yml/badge.svg)](https://github.com/YongRui0402/minimp4/actions/workflows/ci.yml)
[![License: CC0](https://img.shields.io/badge/License-CC0_1.0-lightgrey.svg)](https://creativecommons.org/publicdomain/zero/1.0/)

Minimal, embeddable MP4 mux/demux library in a single C header file.

This is an actively maintained fork of [lieff/minimp4](https://github.com/lieff/minimp4).

## Features

- **Header-only** — single file `minimp4.h`, just include and go
- **Zero dependencies** — only C standard library
- **H.264 (AVC)** and **H.265 (HEVC)** mux/demux support
- **AAC audio** support (with external codec)
- **Three muxing modes** — default, sequential (no backward seek), fragmented (fMP4/HLS)
- **Cross-platform** — Linux, macOS, Windows, ARM, tested via CI

## Quick Start

```c
#define MINIMP4_IMPLEMENTATION
#include "minimp4.h"

// Muxing: H.264 elementary stream to MP4
MP4E_mux_t *mux = MP4E_open(0 /*sequential*/, 0 /*fragmented*/, file, write_callback);
mp4_h26x_writer_t writer;
mp4_h26x_write_init(&writer, mux, width, height, 0 /*is_hevc*/);

// Feed NAL units
mp4_h26x_write_nal(&writer, nal_data, nal_size, duration);

// Finalize
MP4E_close(mux);
mp4_h26x_write_close(&writer);
```

See `minimp4_test.c` for a complete muxing/demuxing example.

## Muxing Modes

**Default** — one large mdat chunk (most efficient, requires backward seek):

![default](images/mux_mode_default.png?raw=true)

**Sequential** — no backward seek needed (useful for streaming/network):

![sequential](images/mux_mode_sequential.png?raw=true)

**Fragmented (fMP4)** — indexes spread across stream, decoding starts before full stream is available (used by browsers/HLS):

![fragmented](images/mux_mode_fragmented.png?raw=true)

## API Reference

### Muxing

| Function | Description |
|----------|-------------|
| `MP4E_open()` | Create multiplexer (default/sequential/fragmented mode) |
| `MP4E_add_track()` | Add video/audio/private track |
| `MP4E_put_sample()` | Write encoded sample (frame) to track |
| `MP4E_set_dsi()` | Set Decoder Specific Info (AAC config) |
| `MP4E_set_sps()` / `set_pps()` / `set_vps()` | Set H.264/H.265 parameter sets |
| `MP4E_set_text_comment()` | Add metadata comment |
| `MP4E_close()` | Finalize and close MP4 file |

### Demuxing

| Function | Description |
|----------|-------------|
| `MP4D_open()` | Parse MP4 file via read callback |
| `MP4D_frame_offset()` | Get sample position, size, timestamp, duration |
| `MP4D_read_sps()` / `read_pps()` | Extract H.264 parameter sets |
| `MP4D_close()` | Free demuxer resources |

### H.264/H.265 Helper

| Function | Description |
|----------|-------------|
| `mp4_h26x_write_init()` | Initialize H.26x writer (auto-detects SPS/PPS/IDR) |
| `mp4_h26x_write_nal()` | Feed a NAL unit (handles SPS/PPS/slice detection) |
| `mp4_h26x_write_close()` | Clean up writer resources |

## Building & Testing

```bash
# CMake (recommended)
cmake -B build
cmake --build build
ctest --test-dir build --output-on-failure

# With AddressSanitizer + UndefinedBehaviorSanitizer
cmake -B build -DMINIMP4_ENABLE_SANITIZERS=ON
cmake --build build
ctest --test-dir build --output-on-failure

# Legacy build scripts
scripts/build_x86.sh
scripts/test.sh
```

### CMake Options

| Option | Default | Description |
|--------|---------|-------------|
| `MINIMP4_BUILD_TESTS` | ON | Build unit tests |
| `MINIMP4_ENABLE_SANITIZERS` | OFF | Enable ASan/UBSan |
| `MINIMP4_ENABLE_COVERAGE` | OFF | Enable gcov coverage |
| `MINIMP4_ENABLE_FUZZING` | OFF | Build libFuzzer targets (requires clang) |

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines.

## Bindings

- https://github.com/darkskygit/minimp4.rs — Rust bindings

## Related Projects

- https://github.com/aspt/mp4
- https://github.com/ireader/media-server/tree/master/libmov
- https://github.com/MPEGGroup/isobmff

## References

- [ISO/IEC 14496-12](https://www.iso.org/standard/68960.html) — ISO Base Media File Format
- [Apple QuickTime File Format](https://developer.apple.com/library/archive/documentation/QuickTime/QTFF/QTFFPreface/qtffPreface.html)
- http://atomicparsley.sourceforge.net/mpeg-4files.html
- http://xhelmboyx.tripod.com/formats/mp4-layout.txt

## License

[CC0 1.0 Universal](LICENSE) — Public Domain
