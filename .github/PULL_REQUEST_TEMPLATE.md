## Description

<!-- What does this PR do? Link to related issues with #number. -->

## Checklist

- [ ] Tests added for the change
- [ ] All tests pass (`ctest --test-dir build --output-on-failure`)
- [ ] Tests pass with ASan/UBSan (`cmake -B build -DMINIMP4_ENABLE_SANITIZERS=ON`)
- [ ] No new external dependencies introduced
- [ ] Code follows project style (C99, 4-space indent, snake_case)
- [ ] Header-only constraint maintained (changes only in `minimp4.h`)
