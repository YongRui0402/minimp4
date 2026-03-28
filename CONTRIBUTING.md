# Contributing to minimp4

Thank you for your interest in contributing! This document explains how to contribute effectively.

## Project Philosophy

minimp4 is a **minimal, zero-dependency, header-only** C library. All contributions must respect these principles:

- **No external dependencies** — only C standard library
- **Keep it minimal** — don't add features beyond core MP4 mux/demux
- **Header-only** — everything stays in `minimp4.h`
- **Embeddable** — must compile on embedded targets (ARM, etc.)

PRs that violate these principles will be rejected.

## Reporting Bugs

1. Use the [bug report template](.github/ISSUE_TEMPLATE/bug_report.md)
2. Include: commit hash, OS, compiler, reproduction steps
3. If possible, attach a minimal test file that triggers the bug

## Submitting Code

### Workflow

1. Fork the repository
2. Create a feature branch from `master`
3. Make your changes
4. Add tests for new functionality or bug fixes
5. Ensure all tests pass (including with sanitizers)
6. Submit a pull request

### Before Submitting

```bash
# Build and test
cmake -B build -DMINIMP4_ENABLE_SANITIZERS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

### Code Style

- **C99/C11**, K&R-style braces
- **4-space indentation** (no tabs)
- **snake_case** for functions and variables
- **UPPER_CASE** for macros and constants
- Public API prefixes: `MP4E_`, `MP4D_`, `mp4_h26x_`
- Keep functions short and focused

### Testing Requirements

- All PRs must include corresponding tests
- Tests must pass with ASan/UBSan enabled (`-DMINIMP4_ENABLE_SANITIZERS=ON`)
- Test harness is in `tests/test_harness.h` (zero-dependency, custom)
- Place tests in `tests/test_*.c`

### PR Checklist

- [ ] Tests added for the change
- [ ] All tests pass (`ctest --test-dir build --output-on-failure`)
- [ ] Tests pass with sanitizers enabled
- [ ] No new external dependencies introduced
- [ ] Code follows the style guide above
- [ ] Commit messages are clear and descriptive

## Review Process

PRs are reviewed with a combination of:
1. **Automated checks** — CI runs build matrix, sanitizers, and coverage
2. **AI-assisted review** — automated checklist for philosophy compliance
3. **Human review** — maintainer reviews code quality and design

## License

By contributing, you agree that your contributions will be released under the [MIT License](LICENSE).
