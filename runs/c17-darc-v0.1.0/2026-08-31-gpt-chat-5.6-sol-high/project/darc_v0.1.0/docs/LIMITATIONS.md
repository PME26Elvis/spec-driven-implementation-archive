# v0.1.0 Limitations and Non-Goals

The following are intentionally outside the v0.1.0 mandatory product and are not claimed as implemented features:

- remote/network repositories, synchronization, authentication, encryption, signing, or key management;
- a GUI or display-server integration;
- distributed/multi-host concurrent writers;
- Windows-native filesystem semantics;
- ACL, xattr, SELinux label, Linux capability, filesystem compression-flag, sparse-extent-layout or reflink preservation;
- special-device/FIFO/socket archival as ordinary payload objects (policy is error or skip);
- following symlink targets into the archive by default;
- arbitrary/general-purpose YAML beyond the documented configuration subset;
- external archive interoperability such as tar/zip;
- deletion of individual files from an immutable snapshot (create a new snapshot instead);
- recovery from more than one unavailable data member in the same XOR parity stripe.

DARC preserves the task-required regular-file bytes, directory structure, mode bits, nanosecond mtimes where the target filesystem supports them, symlink target bytes, and hardlink topology for full restores. Partial restore may intentionally degrade a selected hardlink to a regular file if its primary lies outside the selected subtree; the CLI reports that condition.
