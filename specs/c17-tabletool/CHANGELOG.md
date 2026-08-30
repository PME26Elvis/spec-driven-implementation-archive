# Task Pack Changelog

## v1.0.1

Specification-hardening release; no new product feature family was added.

Key repairs:

- syntax errors no longer open or truncate the requested report path;
- exact decoded runtime path-collision semantics are defined;
- binary-mode C I/O and the practical 8-bit ASCII-compatible C17 profile are explicit;
- value literals now have deterministic text-to-target-type conversion semantics, including oversized numeric-looking text assigned to STRING;
- DECIMAL precision/scale counting is mechanical and includes boundary examples;
- TSV NULL-token decode order is explicit, and exported NULL tokens pass through each format's normal escaping rules;
- quoted CSV CR/LF data now has lossless, byte-exact round-trip semantics; terminal CSV/TSV line endings no longer admit an extra-record interpretation;
- Markdown row framing/tokenization, blank-line definition, and bare-CR behavior are algorithmic rather than intuitive;
- Markdown output rejects semantic leading/trailing ASCII spaces it cannot round-trip;
- report strings have mandatory escaping and report counters have exact definitions;
- whole-script preflight is restricted to path-collision safety so later domain errors cannot reorder execution; report-unsafe collisions have deterministic precedence;
- URL repeated-slash + dot-segment behavior is fully specified;
- SVG root width/height/viewBox geometry and label baseline are deterministic;
- duplicate FIND columns, numeric-domain errors, protective limits, bare CR, and repeated-output failure semantics are classified;
- empty/comment-only scripts are explicitly invalid instead of becoming accidental zero-command successes;
- three compact acceptance groups were added for mutations, Markdown typed/NULL behavior, and parse/report safety.
