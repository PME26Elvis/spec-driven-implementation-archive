# Test-only Material and License Note

No third-party test fixtures, corpora, source files, or vendored test libraries are included in this submission.

The files under `tests/` were written for this implementation. The Python acceptance scripts use only the Python standard library. `tests/faultio.c` is a test-only POSIX/Linux dynamic-interposition fault injector and links against the host system `libdl`; it is not linked into or required by the production `cvc` executable.

Accordingly, there is no additional third-party non-production material requiring a bundled license notice in this submission.
