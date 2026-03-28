# Upstream Review: lieff/minimp4

Tracking document for reviewing issues and PRs from the original [lieff/minimp4](https://github.com/lieff/minimp4) repository.

**Review criteria** — see `docs/MAINTENANCE_PLAN.md` Phase 3.2 for full details.

## Pull Requests

| # | Title | State | Decision | Rationale | Our Action |
|---|-------|-------|----------|-----------|------------|
| 45 | Fixed memory leak (missing free for vvps) | OPEN | already-fixed | We fixed this independently in Phase 1 Step 4 | - |
| 46 | Clean up compiler warnings except fallthrough | OPEN | review | Addresses #31, compiler warnings cleanup | evaluate |
| 36 | Add an SLConfigDescriptor to the ES_Descriptor | OPEN | defer | Adds ~30 lines for AAC spec compliance, needs testing | evaluate |
| 35 | Add methods for manual flushing and changing write offset | OPEN | reject | Adds non-minimal API surface, breaks header-only simplicity | - |
| 34 | Add function to find sync frames and store avc1 codec info | OPEN | defer | Large addition (~100+ lines), extends API beyond core mux/demux | evaluate |
| 37 | integrated with mayhem | CLOSED | reject | CI integration for external fuzzing service, not relevant | - |
| 29 | Fix compilation under Visual Studio | MERGED | already-included | Already in our codebase | - |
| 28 | fix access unit delimiter handling | MERGED | already-included | Already in our codebase | - |
| 23 | Add tag to mp4_h26x_writer_t | MERGED | already-included | Already in our codebase | - |
| 22 | add optional TFDT support | MERGED | already-included | Already in our codebase | - |
| 19 | Replace search for zero byte with memchr() | MERGED | already-included | Already in our codebase | - |
| 17 | fix mac build | CLOSED | skip | Closed without merge | - |
| 7 | fix build in windows | MERGED | already-included | Already in our codebase | - |
| 5 | t1 :test 1 | CLOSED | skip | Test PR, no content | - |

## Issues

| # | Title | State | Decision | Rationale | Our Action |
|---|-------|-------|----------|-----------|------------|
| 50 | Heap buffer overflow via integer overflow in stsz/stts malloc | OPEN | accept | Security bug (CWE-190/CWE-122), aligns with known bug #3 | fix |
| 49 | AV1 Support | OPEN | reject | Beyond core scope (H.264/H.265 mux/demux) | - |
| 48 | minimp4 parses samplerate as 0 (32-bit audio format) | OPEN | accept | Real bug in audio track parsing | fix |
| 47 | 100 individual single-frame H264 files to fmp4 cannot play | OPEN | defer | Needs reproduction, unclear if library bug or usage issue | investigate |
| 44 | build status is not available | OPEN | already-fixed | We replaced Travis CI with GitHub Actions | - |
| 43 | Question about minimp4_test.c | OPEN | skip | Usage question, not a bug | - |
| 42 | Illegal memory access after resetting to sample zero | OPEN | accept | Memory safety bug, bounds checking issue | fix |
| 41 | Wrong track->handler_type for video track | OPEN | accept | Demuxer bug, incorrect handler type parsing | fix |
| 40 | Consulting work | OPEN | skip | Not a bug/feature request | - |
| 39 | can this lib use for fmp4 push to browser MSE? | CLOSED | skip | Usage question | - |
| 38 | H265 to mp4 half video is gray image | OPEN | defer | May be related to our HEVC fixes, needs investigation | investigate |
| 33 | Crash when parsing malformed 264 files | OPEN | accept | Robustness issue, fuzzer likely catches this | fix |
| 32 | Cannot demux MP4 files in H265 format | OPEN | already-fixed | We added hvcC demux handler in Phase 1 Step 10 | - |
| 31 | Compilation Warnings with -Wall -Wextra | OPEN | accept | Code quality, related to PR #46 | fix |
| 30 | Cmd line bug, typos | CLOSED | already-included | Already in our codebase (commit 4575afb) | - |
| 27 | Access Unit Delimiter (AUD) handling | CLOSED | already-included | Fixed in commit 2666e56 | - |
| 26 | License / Patent ? | OPEN | skip | License question (CC0), not actionable | - |
| 25 | Twitter rejects minimp4 files, possible fix | OPEN | accept | Compatibility fix, likely small change | evaluate |
| 24 | The program exits abnormally | CLOSED | skip | Resolved upstream | - |
| 21 | Adjustable Memory Allocation Functions | OPEN | defer | Enhancement, would add API complexity | evaluate |
| 20 | Question: support of MOV container | OPEN | skip | Feature request beyond scope | - |
| 18 | a/v sync issue with fragmentation mode | OPEN | accept | Real bug in fMP4 mode | fix |
| 16 | all modes failed on certain bitstream | CLOSED | skip | Resolved upstream | - |
| 15 | fragmentation mode + slices doesn't work | OPEN | accept | Real bug | fix |
| 14 | sequential mode not seekable on windows | OPEN | accept | Platform compatibility bug | fix |
| 13 | fragmentation mode doesn't work with audio | CLOSED | skip | Resolved upstream | - |
| 12 | add mp3/aac samples | OPEN | defer | Enhancement, needs audio test infrastructure | evaluate |
| 11 | Can you provide the pcm file for test code? | CLOSED | skip | Question | - |
| 10 | Crash with repeated SPS/PPS per IDR frame | CLOSED | skip | Resolved upstream | - |
| 9 | Add const qualifier for data in MP4E_put_sample | CLOSED | already-included | Already in our codebase | - |
| 8 | Compiling as c++ | CLOSED | skip | Resolved upstream | - |
| 6 | mux h265 stream cannot play | CLOSED | already-fixed | Our HEVC fixes address this | - |
| 4 | x265 with aac to mp4? | CLOSED | skip | Question | - |
| 3 | is this lib supporting fMP4? | CLOSED | skip | Question (yes, it does) | - |
| 2 | Assumes correct SPS/PPS | CLOSED | skip | Resolved upstream | - |

## Summary

| Decision | Count |
|----------|-------|
| accept (fix) | 9 |
| already-included/fixed | 11 |
| defer (evaluate) | 6 |
| reject | 3 |
| skip | 16 |

### Priority fixes (accept)

1. **#50** — Heap buffer overflow in stsz/stts (security, P0)
2. **#42** — Illegal memory access after sample reset (P0)
3. **#33** — Crash parsing malformed 264 files (P1)
4. **#41** — Wrong handler_type for video track (P1)
5. **#48** — Audio samplerate parsed as 0 (P1)
6. **#31** — Compiler warnings + PR #46 (P2)
7. **#25** — Twitter compatibility fix (P2)
8. **#18** — A/V sync in fragmentation mode (P2)
9. **#15** — Fragmentation + slices bug (P2)
10. **#14** — Sequential mode not seekable on Windows (P2)
