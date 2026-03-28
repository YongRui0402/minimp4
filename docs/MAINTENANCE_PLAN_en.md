# minimp4 Maintenance Takeover Plan

## Background

minimp4 is a minimal, header-only MP4 mux/demux library in C. The original project [lieff/minimp4](https://github.com/lieff/minimp4) is no longer actively maintained. This plan aims to take over maintenance through four phases, building solid infrastructure and ultimately becoming the actively maintained version.

### Core Philosophy

All changes must follow these principles:

- **Minimal** — no unnecessary features
- **Zero dependencies** — only C standard library
- **Embeddable** — must compile on embedded targets (ARM, etc.)
- **Header-only** — single file include

### Initial Assessment

| Item | Status |
|------|--------|
| Code | `minimp4.h` (3502 lines) + `minimp4_test.c` (367 lines) |
| Build system | Shell scripts (`scripts/build_x86.sh`, `build_arm.sh`), no CMake/Makefile |
| Tests | Binary comparison tests (`scripts/test.sh` + `vectors/` reference files), no unit tests |
| CI/CD | Travis CI only (`.travis.yml`), no GitHub Actions |
| Docs | Minimal README (48 lines), no contributing guide |
| Versioning | No tags, no releases, single master branch |
| License | CC0 1.0 Universal (public domain) — now changed to MIT |
| Known issues | 6 TODO/FIXMEs, missing bounds checks, incomplete HEVC support |

---

## Phase 1: Automated Testing Infrastructure

> Goal: Build a comprehensive testing framework to ensure all features are verifiable and regression-safe.

### 1.1 CMake Build System

Keep existing `scripts/` for backward compatibility. Add CMake as the primary build system.

**New files:**

```
CMakeLists.txt          # Root, defines INTERFACE library target
tests/CMakeLists.txt    # Test target definitions
```

**Design:**

- `minimp4` defined as `INTERFACE` target (header-only library)
- Build options:
  - `MINIMP4_BUILD_TESTS` — build tests (default ON)
  - `MINIMP4_ENABLE_SANITIZERS` — enable ASan/UBSan
  - `MINIMP4_ENABLE_COVERAGE` — enable gcov coverage
  - `MINIMP4_ENABLE_FUZZING` — build libFuzzer fuzz targets
- Also builds legacy `minimp4_test.c` as integration test

**Basic usage:**

```bash
cmake -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

### 1.2 Test Framework

Custom minimal test harness — no external test framework dependencies, consistent with zero-dependency philosophy.

`tests/test_harness.h` provides:

- `ASSERT_EQ(a, b)`, `ASSERT_NE(a, b)` — equality assertions
- `ASSERT_NULL(x)` / `ASSERT_NOT_NULL(x)` — null pointer assertions
- `ASSERT_MEM_EQ(a, b, n)` — memory comparison
- `ASSERT_TRUE(x)` / `ASSERT_FALSE(x)` — boolean assertions
- `RUN_TEST(fn)` — test execution and reporting
- `TEST_SUMMARY()` — test summary output

### 1.3 Test Directory Structure

```
tests/
├── test_harness.h          # Minimal assertion/runner macros
├── test_mux_api.c          # MP4E_* API unit tests
├── test_demux_api.c        # MP4D_* API unit tests
├── test_h26x_writer.c      # mp4_h26x_write_* convenience API tests
├── test_nal_parsing.c      # NAL unit finding, escape code handling
├── test_vector_util.c      # minimp4_vector_* internal data structure
├── test_bit_reader.c       # bit_reader_t and bs_t tests
├── test_edge_cases.c       # NULL pointers, zero-length, overflow, etc.
├── test_integration.c      # Round-trip mux→demux fidelity tests
├── test_hevc.c             # HEVC-specific mux/demux paths
└── fuzz/
    ├── fuzz_demux.c        # libFuzzer: MP4D_open fuzz target
    └── fuzz_mux_nal.c      # libFuzzer: mp4_h26x_write_nal fuzz target
```

### 1.4 Test Coverage by Module

| Test file | Coverage | Priority |
|-----------|----------|----------|
| `test_mux_api.c` | `MP4E_open/add_track/put_sample/set_dsi/sps/pps/close`, sequential/fragmented modes, multi-track | P0 |
| `test_demux_api.c` | `MP4D_open/frame_offset/read_sps/read_pps/close`, track enumeration, fMP4 demux | P0 |
| `test_integration.c` | H.264 mux→demux round-trip, three modes, slices (replaces shell `cmp` tests) | P0 |
| `test_h26x_writer.c` | `mp4_h26x_write_init/nal/close`, H.264 and H.265 NAL writing | P1 |
| `test_nal_parsing.c` | `find_nal_unit` (00 00 01 / 00 00 00 01 start codes), `remove_nal_escapes` round-trip, edge cases | P1 |
| `test_bit_reader.c` | `init_bits/get_bits/show_bits`, Exp-Golomb codec, position tracking | P1 |
| `test_edge_cases.c` | NULL pointers to all public APIs, zero-length data, invalid track ID, callback failures, integer overflow | P1 |
| `test_vector_util.c` | `minimp4_vector_init/grow/alloc_tail/put/reset` | P2 |
| `test_hevc.c` | HEVC NAL mux/demux, hvcC box verification | P2 |
| `fuzz/fuzz_demux.c` | Arbitrary bytes fed to `MP4D_open`, highest-value fuzz target | P2 |
| `fuzz/fuzz_mux_nal.c` | Arbitrary bytes fed to `mp4_h26x_write_nal` | P2 |

### 1.5 CI/CD: Travis CI → GitHub Actions

New file: `/.github/workflows/ci.yml`

| Job | Description |
|-----|-------------|
| `build-and-test` | Build matrix: ubuntu/macos/windows x gcc/clang, with ASan/UBSan |
| `coverage` | gcov + lcov → Codecov, target **80%+** line coverage |
| `legacy-tests` | Run original `scripts/build_x86.sh` + `scripts/test.sh` |
| `fuzz` | clang + libFuzzer, 60-second quick fuzz run |

### 1.6 Execution Order

```
Step 1  ─── CMakeLists.txt (root + tests/), with legacy test target
Step 2  ─── tests/test_harness.h
Step 3  ─── .github/workflows/ci.yml (legacy tests + CMake build)
Step 4  ─── test_mux_api.c + test_demux_api.c (highest value)
Step 5  ─── test_integration.c (replaces shell cmp tests)
Step 6  ─── test_nal_parsing.c + test_bit_reader.c + test_h26x_writer.c
Step 7  ─── test_edge_cases.c + test_vector_util.c
Step 8  ─── Coverage reporting
Step 9  ─── Fuzz targets
Step 10 ─── HEVC test vectors + test_hevc.c
Step 11 ─── Remove .travis.yml
```

---

## Phase 2: AI Agent Semi-automated Management

> Goal: Use AI tools to assist daily maintenance and reduce manual effort.

### 2.1 CLAUDE.md Project Guide

Create `/CLAUDE.md` at repo root with project context for Claude Code:
- Project overview and architecture
- Build and test commands
- Core philosophy
- Code style conventions (C99, 4-space indent, snake_case, MP4E_/MP4D_/mp4_h26x_ prefixes)
- Known issues and TODO list
- PR review guidelines

### 2.2 AI-Assisted PR Review

New file: `/.github/workflows/ai-review.yml`

- **Trigger:** `pull_request` events
- **Review focus:** (a) philosophy compliance (b) C code quality (c) test coverage (d) no new dependencies
- **Output:** PR comment (advisory, non-blocking)

### 2.3 AI-Assisted Issue Triage

- Triggered on new issues
- Auto-label: `bug` / `enhancement` / `question` + topic labels
- Suggest priority (P0-P3)
- **Never auto-close** — label and comment only

### 2.4 AI-Assisted Code Quality

- Pre-commit hook script (`scripts/pre-commit`)
- Checks: build with `-Wall -Wextra -Werror`, run tests, warn about malloc without NULL check

### 2.5 AI-Assisted Release Management

- Tag push triggers changelog generation
- Draft release notes from commit history
- Create GitHub Release via `gh release create`

### 2.6 Execution Order

```
Step 1  ─── Create CLAUDE.md
Step 2  ─── Configure .claude/settings.local.json
Step 3  ─── AI PR review GitHub Action
Step 4  ─── AI Issue triage action
Step 5  ─── Pre-commit hooks
Step 6  ─── Release automation
```

---

## Phase 3: Upstream Issue & PR Review

> Goal: Filter valuable contributions from lieff/minimp4 and integrate them.

### 3.1 Review Process

1. Add upstream remote: `git remote add upstream https://github.com/lieff/minimp4.git`
2. Fetch and catalog all open/closed issues and PRs
3. Create tracking document: `docs/upstream-review.md`

### 3.2 Acceptance/Rejection Criteria

**Accept (all must be met):**
- Fixes a real bug (crash, data corruption, spec non-compliance) or improves robustness
- Does not add external dependencies
- Does not significantly increase code size (>100 lines needs strong justification)
- Maintains header-only, embeddable nature
- Tested or we can add tests for it

**Reject (any one triggers):**
- Adds external library dependencies
- Goes beyond core mux/demux scope
- Breaks backward API compatibility without strong justification
- Cosmetic-only with no functional improvement

**Defer:**
- Large changes that need tests before merging
- Touches HEVC code we plan to rewrite

### 3.3 Git Workflow

- Cherry-pick (not merge) to keep history clean
- Each cherry-pick gets its own branch and PR with tests
- Commit messages reference original issue/PR: `Cherry-pick from lieff/minimp4#XX`

### 3.4 Execution Order

```
Step 1  ─── Add upstream remote and fetch
Step 2  ─── Catalog all issues/PRs into tracking document
Step 3  ─── Review each item against criteria
Step 4  ─── Cherry-pick accepted changes, add tests, merge via PR
Step 5  ─── Fix accepted upstream bugs by priority
```

---

## Phase 4: Project Infrastructure

> Goal: Establish professional open-source project presence.

### 4.1 README Overhaul

Expand README with: badges, fork notice, feature list, quick start, API reference, build instructions, muxing mode diagrams, contributing link, license.

### 4.2 CONTRIBUTING.md

Create `/CONTRIBUTING.md` covering: bug reporting, code submission workflow, code style guide, testing requirements, project philosophy statement, review process.

### 4.3 GitHub Issue/PR Templates

- `/.github/ISSUE_TEMPLATE/bug_report.md` — structured bug report
- `/.github/ISSUE_TEMPLATE/feature_request.md` — feature proposal
- `/.github/PULL_REQUEST_TEMPLATE.md` — PR checklist

### 4.4 Versioning Strategy

| Version | Milestone |
|---------|-----------|
| `v0.1.0` | Code baseline (inherited from lieff/minimp4) |
| `v0.2.0` | Testing infrastructure complete |
| `v0.3.0` | Upstream review integration complete |
| `v1.0.0` | Stable release (all known bugs fixed + HEVC improvements) |

Semantic Versioning after v1.0.0: PATCH for bug fixes, MINOR for new features, MAJOR for breaking API changes.

### 4.5 Community Outreach (Deferred)

> Deferred until the project is more complete. Includes: announcing on lieff/minimp4, commenting on upstream issues, contacting dependent projects, social media promotion.

### 4.6 Execution Order

```
Step 1  ─── Update README.md
Step 2  ─── Create CONTRIBUTING.md
Step 3  ─── Create issue/PR templates
Step 4  ─── Set GitHub topics and description
Step 5  ─── Create initial release tag v0.1.0
(Deferred)
Step 6  ─── Announce on lieff/minimp4
Step 7  ─── Comment on upstream issues
Step 8  ─── Contact dependent project maintainers
Step 9  ─── Community promotion
```

---

## Verification

### Phase 1

```bash
cmake -B build -DMINIMP4_ENABLE_SANITIZERS=ON
cmake --build build
ctest --test-dir build --output-on-failure
# Target: 80%+ line coverage
```

- GitHub Actions CI all green
- Codecov shows 80%+ coverage
- Legacy tests still pass

### Phase 2

- Test PR triggers AI review bot comment
- New issues get auto-labeled
- Tag push generates release notes

### Phase 3

- `docs/upstream-review.md` fully reviewed
- Each accepted change has corresponding tests
- Cherry-picked PRs pass CI

### Phase 4

- README contains all planned sections
- Issue/PR templates work correctly
- `v0.1.0` release created
- GitHub topics set
