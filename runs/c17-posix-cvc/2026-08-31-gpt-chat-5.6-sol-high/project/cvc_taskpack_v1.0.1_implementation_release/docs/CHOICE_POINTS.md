# Permitted Implementation Choice Points

The CVC v1 specification fixes external behavior and canonical committed bytes. This implementation uses the following choices only where the specification leaves implementation freedom.

## JSON representation

The handwritten JSON layer builds a small generic value tree sufficient for the RFC-8259 syntax required by the task, then applies a separate strict CVC configuration-schema validator. The spec permits a generic DOM but does not require one.

## Working snapshot representation

Working/commit snapshots are represented internally as sorted leaf entries containing canonical repository-relative path, entry kind, and object ID. Directories are inferred structural prefixes. When storing a snapshot, this flat view is recursively serialized into the fixed canonical tree format. This is an in-memory algorithmic choice only; it does not alter repository bytes.

## Directory traversal

Recursive traversal is handwritten using `opendir`/`readdir` plus `lstat`. It deliberately does not use `ftw`, `nftw`, `fts`, `glob`, or `fnmatch`. No include-pattern directory-pruning optimization is used; all safe in-scope directories are enumerated and filtering is applied deterministically.

## Diff presentation

Ordinary regular-file edit computation uses the handwritten Myers shortest-edit algorithm. Human-readable output is CVC-specific rather than byte-for-byte GNU/unified diff formatting, as the specification permits decoration to vary. Line identity is byte-exact and preserves final-newline/CRLF distinctions.

## Three-way text merge construction

Three-way text merge derives base-coordinate edit ranges from the same handwritten Myers machinery. Identical edits are coalesced; disjoint edits are applied in deterministic base order; overlapping incompatible edits become conflicts. Visible textual conflicts use stable CVC marker output. This is an implementation choice for realizing the required three-way semantics, not an always-pick-one-side shortcut.

## Merge-base tie handling

The commit graph is traversed directly from canonical commit parent IDs. For multiple best common ancestors, the implementation applies the specification-required deterministic lexicographically smallest raw object-ID tie-break. A dedicated graph unit test covers this case.

## Branch-list decoration

Branch names are emitted in unsigned UTF-8 byte order. Every row begins with a fixed marker column: `*` for the current branch and `-` for other branches. The exact prose/decorative marker is not prescribed by the spec; explicit non-current markers make parsing unambiguous.

## Unreachable objects

Objects made unreachable by branch deletion or by a failed operation are retained. The specification explicitly permits unreachable loose objects to remain and forbids requiring garbage collection.

## Rename detection

No rename detector is implemented. A disappearance plus identical content at a different path is reported as delete + add, which the specification permits.

## Empty directories

Empty working-tree directories are not versioned. Structural directories that become empty after tracked descendants are removed may disappear. Unrelated untracked descendants of a structural directory are preserved during materialization.

## Restored regular-file permissions

File mode bits are not part of the CVC v1 object model. Restored regular files are created with the implementation's ordinary creation mode (`0666`, filtered by process `umask`). This does not encode unsupported permission metadata into history.

## Object installation strategy

The specification requires immutable/atomic safe installation but does not prescribe one syscall sequence. This implementation writes a temporary file in the object fan-out directory, fsyncs it, and uses a same-filesystem hard-link install to avoid overwriting an existing object; an existing target is independently verified. Metadata refs/HEAD use temporary-file + fsync + rename + parent-directory fsync.

## Working-tree rollback strategy

For detected failures after multi-path materialization begins, this implementation uses backup-renames under `.cvc/state/` as its rollback journal. A preflight collision/symlink safety pass occurs first, then affected tracked entries are moved to backup, replacements are installed as new directory entries, and controlling HEAD/ref/history metadata is updated only at the specified safe point. Other equivalent journal strategies were permitted by the spec.

## Symlink display

Symlink contents are stored exactly, but diagnostic/diff rendering may escape bytes that should not be emitted raw. This is presentation only; object identity and restoration use exact target bytes.
