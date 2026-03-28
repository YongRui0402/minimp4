# minimp4 - Project Guide

## Overview

minimp4 is a minimal, header-only C library for MP4 muxing/demuxing. This is an actively maintained fork of [lieff/minimp4](https://github.com/lieff/minimp4).

## Core Philosophy

All changes MUST follow these principles:
- **Minimal** — no unnecessary features
- **Zero dependencies** — only C standard library
- **Embeddable** — must compile on embedded targets (ARM, etc.)
- **Header-only** — single file include

## Architecture

- `minimp4.h` (3502 lines) — entire library implementation, guarded by `MINIMP4_IMPLEMENTATION`
- `minimp4_test.c` (367 lines) — CLI test/example program
- `scripts/` — legacy build scripts (build_x86.sh, build_arm.sh, test.sh)
- `vectors/` — test vectors (H.264 streams + reference MP4 outputs)
- `tests/` — unit tests (being built out in Phase 1)

### Public API

- Mux: `MP4E_open`, `MP4E_add_track`, `MP4E_put_sample`, `MP4E_close`, `MP4E_set_dsi/vps/sps/pps`, `MP4E_set_text_comment`
- Demux: `MP4D_open`, `MP4D_frame_offset`, `MP4D_read_sps`, `MP4D_read_pps`, `MP4D_close`
- H.26x helper: `mp4_h26x_write_init`, `mp4_h26x_write_nal`, `mp4_h26x_write_close`

### Internal utilities

- `minimp4_vector_t` — dynamic array
- `bit_reader_t` / `bs_t` — bitstream reader/writer
- `find_nal_unit`, `remove_nal_escapes`, `nal_put_esc` — NAL parsing

## Build & Test

```bash
# CMake build (primary)
cmake -B build && cmake --build build && ctest --test-dir build --output-on-failure

# With sanitizers
cmake -B build -DMINIMP4_ENABLE_SANITIZERS=ON && cmake --build build && ctest --test-dir build --output-on-failure

# With coverage
cmake -B build-cov -DMINIMP4_ENABLE_COVERAGE=ON && cmake --build build-cov && ctest --test-dir build-cov

# Legacy build
scripts/build_x86.sh && scripts/test.sh
```

## Code Style

- C99/C11, K&R-ish brace style
- 4-space indentation
- `snake_case` for functions, `UPPER_CASE` for macros
- Public API prefixed: `MP4E_`, `MP4D_`, `mp4_h26x_`
- No external dependencies allowed in tests (use custom test harness in `tests/test_harness.h`)

## Testing Requirements

- All new code must have corresponding tests
- Tests must pass with ASan/UBSan enabled
- Target: 80%+ line coverage of `minimp4.h`
- Test harness: `tests/test_harness.h` (custom, zero-dependency)

## Full Plan

See `docs/MAINTENANCE_PLAN.md` for the complete 4-phase maintenance plan.

## Current Progress

**Phase 1: Automated Testing Infrastructure — COMPLETE**

- [x] Step 0: Prerequisites (cmake, gh, clang, lcov, ffmpeg installed; permissions configured)
- [x] Step 1: Create `CMakeLists.txt` (root + tests/), include legacy test target
- [x] Step 2: Create `tests/test_harness.h`
- [x] Step 3: Create `.github/workflows/ci.yml` (legacy tests + CMake build)
- [x] Step 4: Write `test_mux_api.c` + `test_demux_api.c`
- [x] Step 5: Write `test_integration.c` (replace shell cmp tests)
- [x] Step 6: Write `test_nal_parsing.c` + `test_bit_reader.c` + `test_h26x_writer.c`
- [x] Step 7: Write `test_edge_cases.c` + `test_vector_util.c`
- [x] Step 8: Add coverage reporting to CI (83.5% line coverage achieved)
- [x] Step 9: Add fuzz targets (fuzz_demux + fuzz_mux_nal with libFuzzer)
- [x] Step 10: Generate HEVC test vectors + `test_hevc.c` (3 bugs fixed: SEI skip, hvcC demux, hvc1 sample entry)
- [x] Step 11: Remove `.travis.yml` (replaced by GitHub Actions CI)

**Phase 2: AI Agent Semi-automated Management — NOT STARTED**

- [x] Step 1: CLAUDE.md created (done in Phase 1 prerequisites)
- [x] Step 2: .claude/settings.local.json configured (done in Phase 1 prerequisites)
- [ ] Step 3: AI PR review GitHub Action (`.github/workflows/ai-review.yml`)
- [ ] Step 4: AI Issue triage scheduled action
- [ ] Step 5: Pre-commit hooks for code quality
- [ ] Step 6: Release automation (tag → changelog → gh release)

## Known Bugs (to fix during Phase 1)

1. `minimp4_test.c:70` — read_callback underflow: `buf->size - offset - size`
2. `minimp4.h:1542` — hvcC box hardcodes profile/level instead of parsing from HEVC SPS
3. `minimp4.h:764` — `minimp4_vector_grow` integer overflow in `capacity*2 + 1024`
4. Multiple places — missing NULL check after malloc
5. `minimp4.h:2486` — switch fallthrough without explicit annotation
6. `minimp4.h:3242` — `MP4D_frame_offset` no bounds check on ntrack/nsample
