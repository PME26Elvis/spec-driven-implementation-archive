#!/usr/bin/env python3
"""Extract a plain-text corpus of visible Grok model-authored output from Grok Markdown exports.

Recommended input: Grok Console Export v1.4 Markdown. v1.4 embeds invisible
``GROK_EXPORT`` HTML comments derived from the live Grok DOM, allowing this
script to distinguish model-authored reasoning/tool-call input/final responses
from terminal/tool results without relying on grep/find/command keyword lists.

Output corpus policy (compute-oriented, not billing-oriented):
  INCLUDE
    * visible Grok reasoning/thinking prose
    * Grok-authored Bash command strings / tool-call arguments
    * Grok-authored Write-file bodies exposed by the UI
    * Grok final responses
  EXCLUDE
    * user messages
    * terminal stdout/stderr/exit-code text
    * Read-file/tool-return contents
    * UI/activity labels and metadata

The .txt corpus contains no category labels added by this script. A sibling
.meta.json contains provenance, category character counts, exclusions, warnings,
and ambiguity diagnostics. This script deliberately does NOT tokenize; feed the
.txt to the tokenizer of your choice as a separate pipeline stage.

Legacy v1.2/v1.3 Markdown without provenance markers is accepted in best-effort
mode, but exact tool-input vs tool-result separation cannot be guaranteed after
DOM semantics have already been flattened. Re-exporting with v1.4 is preferred.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import re
import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import Iterable, List, Optional, Tuple

VERSION = "1.0.0"
MARKER_RE = re.compile(
    r"^\s*<!--\s*GROK_EXPORT:(BEGIN|END)\s+kind=([a-zA-Z0-9_-]+)"
    r"(?:\s+subtype=([a-zA-Z0-9_-]+))?\s*-->\s*$"
)
TURN_RE = re.compile(r"^##\s+(Human|Grok)(?:\s+\[[^\]]+\])?\s*$", re.M)
FENCE_OPEN_RE = re.compile(r"^\s*(`{3,}|~{3,})([^`]*)$")

INCLUDE_KINDS = {"model_reasoning", "model_tool_input", "assistant_final"}
EXCLUDE_KINDS = {"tool_result", "ui_event"}


@dataclass
class Fragment:
    kind: str
    subtype: str = ""
    text: str = ""
    source: str = "marker"


@dataclass
class Frame:
    kind: str
    subtype: str
    lines: List[str] = field(default_factory=list)


@dataclass
class ExtractionResult:
    corpus: str
    included: List[Fragment]
    excluded: List[Fragment]
    ambiguous: List[Fragment]
    parser_mode: str
    warnings: List[str]


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def clean_fragment(text: str) -> str:
    text = text.replace("\r\n", "\n").replace("\r", "\n")
    text = re.sub(r"[ \t]+\n", "\n", text)
    return text.strip("\n")


def strip_one_outer_code_fence(text: str) -> str:
    """Remove one Markdown fence pair inserted by the DOM->Markdown exporter.

    Tool-call inputs are rendered inside <pre>, so the exporter necessarily
    adds a fence even though that fence is not part of the Bash/write-file
    argument itself. Final-response/reasoning Markdown is left untouched.
    """
    lines = clean_fragment(text).splitlines()
    while lines and not lines[0].strip():
        lines.pop(0)
    while lines and not lines[-1].strip():
        lines.pop()
    if len(lines) < 2:
        return "\n".join(lines)

    m = FENCE_OPEN_RE.match(lines[0])
    if not m:
        return "\n".join(lines)
    fence_char = m.group(1)[0]
    min_len = len(m.group(1))
    if re.fullmatch(rf"\s*{re.escape(fence_char)}{{{min_len},}}\s*", lines[-1]):
        return "\n".join(lines[1:-1]).strip("\n")
    return "\n".join(lines)


def _pop_matching(stack: List[Frame], kind: str, subtype: str) -> Optional[Frame]:
    # v1.4 emits properly nested markers. Be defensive if a file was edited.
    for idx in range(len(stack) - 1, -1, -1):
        fr = stack[idx]
        if fr.kind == kind and (not subtype or fr.subtype == subtype):
            popped = fr
            del stack[idx:]
            return popped
    return None


def parse_marker_export(markdown: str) -> ExtractionResult:
    included: List[Fragment] = []
    excluded: List[Fragment] = []
    ambiguous: List[Fragment] = []
    warnings: List[str] = []
    stack: List[Frame] = []
    unmarked_thinking: List[str] = []

    def flush_unmarked() -> None:
        nonlocal unmarked_thinking
        txt = clean_fragment("\n".join(unmarked_thinking))
        unmarked_thinking = []
        if txt:
            included.append(Fragment("model_reasoning", "unmarked_thinking", txt, "v1.4-default"))

    lines = markdown.splitlines()
    marker_count = 0
    malformed_markers = 0

    for line in lines:
        m = MARKER_RE.match(line)
        if m:
            marker_count += 1
            phase, kind, subtype = m.group(1), m.group(2), m.group(3) or ""
            flush_unmarked()
            if phase == "BEGIN":
                stack.append(Frame(kind, subtype))
            else:
                frame = _pop_matching(stack, kind, subtype)
                if frame is None:
                    malformed_markers += 1
                    continue
                text = clean_fragment("\n".join(frame.lines))
                if not text:
                    continue
                frag = Fragment(frame.kind, frame.subtype, text, "v1.4-marker")
                if frame.kind in INCLUDE_KINDS:
                    if frame.kind == "model_tool_input":
                        frag.text = strip_one_outer_code_fence(frag.text)
                    included.append(frag)
                elif frame.kind in EXCLUDE_KINDS:
                    excluded.append(frag)
                # assistant_thinking is an envelope; its classified/unmarked children
                # are handled separately to avoid double counting.
            continue

        # Append to every active frame. The outer assistant_thinking envelope is not
        # emitted as a fragment, but this allows assistant_final/tool frames to close.
        for frame in stack:
            frame.lines.append(line)

        active_kinds = [fr.kind for fr in stack]
        if "assistant_thinking" in active_kinds and not any(
            k in INCLUDE_KINDS or k in EXCLUDE_KINDS for k in active_kinds
        ):
            unmarked_thinking.append(line)
        else:
            flush_unmarked()

    flush_unmarked()

    if stack:
        warnings.append(f"{len(stack)} unclosed GROK_EXPORT marker frame(s)")
        malformed_markers += len(stack)
    if malformed_markers:
        warnings.append(f"{malformed_markers} malformed/unmatched marker(s)")
    if marker_count == 0:
        warnings.append("No GROK_EXPORT markers found")

    # Preserve occurrence order. Do NOT globally deduplicate identical text: a model
    # can legitimately generate the same command/reply multiple times and each is output.
    corpus_parts = [clean_fragment(f.text) for f in included if clean_fragment(f.text)]
    corpus = "\n\n".join(corpus_parts).strip() + ("\n" if corpus_parts else "")

    return ExtractionResult(corpus, included, excluded, ambiguous, "v1.4-markers", warnings)


def split_turns(markdown: str) -> List[Tuple[str, str]]:
    matches = list(TURN_RE.finditer(markdown))
    turns: List[Tuple[str, str]] = []
    for i, m in enumerate(matches):
        end = matches[i + 1].start() if i + 1 < len(matches) else len(markdown)
        body = markdown[m.end():end]
        # Strip the export separator only; preserve model-authored Markdown.
        body = re.sub(r"\n---\s*$", "", body.rstrip())
        turns.append((m.group(1), body.strip()))
    return turns


def extract_fenced_blocks(text: str) -> Tuple[str, List[str]]:
    """Return non-fenced text and fenced blocks for legacy analysis."""
    lines = text.splitlines()
    prose: List[str] = []
    blocks: List[str] = []
    i = 0
    while i < len(lines):
        m = FENCE_OPEN_RE.match(lines[i])
        if not m:
            prose.append(lines[i])
            i += 1
            continue
        fence_char = m.group(1)[0]
        min_len = len(m.group(1))
        j = i + 1
        body: List[str] = []
        while j < len(lines):
            if re.fullmatch(rf"\s*{re.escape(fence_char)}{{{min_len},}}\s*", lines[j]):
                break
            body.append(lines[j])
            j += 1
        if j >= len(lines):
            prose.extend(lines[i:])
            break
        blocks.append("\n".join(body).strip("\n"))
        i = j + 1
    return clean_fragment("\n".join(prose)), blocks


# These features are ONLY for legacy v1.2/v1.3 fallback. The v1.4 path never
# classifies by command keywords; it uses provenance markers derived from DOM semantics.
SHELLISH_RE = re.compile(
    r"(?:^|\n)\s*(?:cd\b|ls\b|cat\b|head\b|tail\b|grep\b|find\b|sed\b|awk\b|wc\b|"
    r"gcc\b|clang\b|make\b|cmake\b|python(?:3)?\b|perl\b|bash\b|sh\b|zip\b|unzip\b|"
    r"mkdir\b|cp\b|mv\b|rm\b|chmod\b|printf\b|echo\b|git\b|timeout\b|test\b|for\s+\w+\s+in\b|"
    r"[A-Z_][A-Z0-9_]*=|#\s*!)",
    re.M,
)
RESULTISH_RE = re.compile(
    r"(?:Exit code\s+\d+|^make(?:\[\d+\])?:|^gcc\s+-|^clang\s+-|^total\s+\d+|"
    r"^drwx|^-rw|^PASS\b|^FAIL\b|^warning:|^error:|^Archive:|^inflating:)",
    re.M | re.I,
)


def parse_legacy_export(markdown: str) -> ExtractionResult:
    """Best-effort fallback for v1.2/v1.3 Markdown without provenance comments.

    It intentionally reports ambiguity because DOM role information has already been
    flattened. It is useful for old archives, not a replacement for v1.4.
    """
    included: List[Fragment] = []
    excluded: List[Fragment] = []
    ambiguous: List[Fragment] = []
    warnings = [
        "Legacy Markdown has no DOM provenance markers; tool-input vs tool-result separation is heuristic.",
        "For strict statistics, re-export the live conversation with v1.4.",
    ]

    for speaker, body in split_turns(markdown):
        if speaker == "Human":
            excluded.append(Fragment("user_message", "", clean_fragment(body), "legacy-heading"))
            continue

        if "### Thinking" in body:
            _, after = body.split("### Thinking", 1)
            if "### Response" in after:
                thinking, response = after.split("### Response", 1)
            else:
                thinking, response = after, ""
        else:
            thinking, response = "", body

        if response.strip():
            included.append(Fragment("assistant_final", "", clean_fragment(response), "legacy-heading"))

        if thinking.strip():
            prose, blocks = extract_fenced_blocks(thinking)
            # UI status crumbs flattened by v1.2 are hard to distinguish from reasoning;
            # include prose as visible Grok thinking, but report the legacy uncertainty.
            if prose:
                included.append(Fragment("model_reasoning", "legacy-prose", prose, "legacy-heuristic"))

            # Conservative pairing: a shell-like block immediately followed by a
            # non-shell/result-like block is treated as command then tool result.
            i = 0
            while i < len(blocks):
                block = blocks[i]
                shellish = bool(SHELLISH_RE.search(block))
                if shellish:
                    included.append(Fragment("model_tool_input", "legacy-shell", block, "legacy-heuristic"))
                    if i + 1 < len(blocks):
                        nxt = blocks[i + 1]
                        if RESULTISH_RE.search(nxt) or not SHELLISH_RE.search(nxt):
                            excluded.append(Fragment("tool_result", "legacy-paired", nxt, "legacy-heuristic"))
                            i += 2
                            continue
                    i += 1
                else:
                    ambiguous.append(Fragment("ambiguous_code_block", "", block, "legacy-heuristic"))
                    i += 1

    corpus_parts = [clean_fragment(f.text) for f in included if clean_fragment(f.text)]
    corpus = "\n\n".join(corpus_parts).strip() + ("\n" if corpus_parts else "")
    return ExtractionResult(corpus, included, excluded, ambiguous, "legacy-heuristic", warnings)


def extract(markdown: str) -> ExtractionResult:
    if "<!-- GROK_EXPORT:BEGIN" in markdown:
        return parse_marker_export(markdown)
    return parse_legacy_export(markdown)


def count_chars(fragments: Iterable[Fragment], kind: Optional[str] = None) -> int:
    return sum(len(f.text) for f in fragments if kind is None or f.kind == kind)


def count_fragments(fragments: Iterable[Fragment], kind: Optional[str] = None) -> int:
    return sum(1 for f in fragments if kind is None or f.kind == kind)


def category_summary(fragments: List[Fragment]) -> dict:
    kinds = sorted({f.kind for f in fragments})
    return {
        kind: {
            "fragments": count_fragments(fragments, kind),
            "chars": count_chars(fragments, kind),
            "subtypes": sorted({f.subtype for f in fragments if f.kind == kind and f.subtype}),
        }
        for kind in kinds
    }


def user_chars_from_turns(markdown: str) -> int:
    return sum(len(clean_fragment(body)) for speaker, body in split_turns(markdown) if speaker == "Human")


def build_meta(source: Path, source_bytes: bytes, result: ExtractionResult, corpus_bytes: bytes) -> dict:
    return {
        "schema": "grok-model-output-corpus-meta-v1",
        "extractor": f"extract_grok_model_output.py/{VERSION}",
        "source": str(source),
        "source_sha256": sha256_bytes(source_bytes),
        "parser_mode": result.parser_mode,
        "corpus_sha256": sha256_bytes(corpus_bytes),
        "corpus_chars": len(result.corpus),
        "policy": {
            "purpose": "compute-oriented visible model-authored output proxy; not billing-token accounting",
            "included": ["visible reasoning/thinking", "model-authored tool-call input", "final Grok responses"],
            "excluded": ["user messages", "terminal/tool results", "read-file results", "UI/status chrome"],
            "tokenizer": "not run; tokenize the generated .txt separately",
        },
        "included": category_summary(result.included),
        "excluded": category_summary(result.excluded),
        "ambiguous": category_summary(result.ambiguous),
        "ambiguous_fragments": len(result.ambiguous),
        "warnings": result.warnings,
    }


def output_paths(source: Path, output_dir: Optional[Path]) -> Tuple[Path, Path]:
    base_dir = output_dir or source.parent
    base_dir.mkdir(parents=True, exist_ok=True)
    stem = source.name[:-3] if source.name.lower().endswith(".md") else source.name
    txt = base_dir / f"{stem}.grok-output.txt"
    meta = base_dir / f"{stem}.grok-output.meta.json"
    return txt, meta


def process_file(source: Path, output_dir: Optional[Path], strict: bool) -> int:
    source_bytes = source.read_bytes()
    markdown = source_bytes.decode("utf-8", errors="replace")
    result = extract(markdown)
    corpus_bytes = result.corpus.encode("utf-8")
    txt_path, meta_path = output_paths(source, output_dir)
    meta = build_meta(source, source_bytes, result, corpus_bytes)
    meta["source_user_chars_observed"] = user_chars_from_turns(markdown)

    txt_path.write_bytes(corpus_bytes)
    meta_path.write_text(json.dumps(meta, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")

    print(f"{source}: {result.parser_mode}")
    print(f"  -> {txt_path} ({len(result.corpus):,} chars)")
    print(f"  -> {meta_path}")
    for kind, info in meta["included"].items():
        print(f"     include {kind}: {info['fragments']} fragment(s), {info['chars']:,} chars")
    for kind, info in meta["excluded"].items():
        print(f"     exclude {kind}: {info['fragments']} fragment(s), {info['chars']:,} chars")
    if result.ambiguous:
        print(f"     AMBIGUOUS: {len(result.ambiguous)} fragment(s)")
    for warning in result.warnings:
        print(f"     warning: {warning}")

    if strict and (result.parser_mode != "v1.4-markers" or result.ambiguous or result.warnings):
        return 3
    return 0


def expand_inputs(raw: List[str], recursive: bool) -> List[Path]:
    files: List[Path] = []
    seen = set()
    for item in raw:
        p = Path(item)
        if p.is_dir():
            iterator = p.rglob("*.md") if recursive else p.glob("*.md")
            candidates = sorted(iterator)
        else:
            candidates = [p]
        for c in candidates:
            if not c.is_file():
                continue
            key = str(c.resolve())
            if key not in seen:
                seen.add(key)
                files.append(c)
    return files


def self_test() -> int:
    sample = r'''# Grok Conversation Export

## Human

DO NOT INCLUDE ME

---

## Grok [THINK]

### Thinking

<!-- GROK_EXPORT:BEGIN kind=assistant_thinking -->
<!-- GROK_EXPORT:BEGIN kind=model_reasoning -->
I should inspect the repository.
<!-- GROK_EXPORT:END kind=model_reasoning -->
<!-- GROK_EXPORT:BEGIN kind=ui_event subtype=bash_header -->
Executed command
<!-- GROK_EXPORT:END kind=ui_event subtype=bash_header -->
<!-- GROK_EXPORT:BEGIN kind=model_tool_input subtype=bash -->
```
grep -R "solver" src/
```
<!-- GROK_EXPORT:END kind=model_tool_input subtype=bash -->
<!-- GROK_EXPORT:BEGIN kind=tool_result subtype=bash -->
```
src/solver.c:1000 lines of returned text
Exit code 0
```
<!-- GROK_EXPORT:END kind=tool_result subtype=bash -->
Unmarked visible reasoning line.
<!-- GROK_EXPORT:END kind=assistant_thinking -->

### Response

<!-- GROK_EXPORT:BEGIN kind=assistant_final -->
Done **successfully**.
<!-- GROK_EXPORT:END kind=assistant_final -->
'''
    r = extract(sample)
    expected = [
        "I should inspect the repository.",
        'grep -R "solver" src/',
        "Unmarked visible reasoning line.",
        "Done **successfully**.",
    ]
    forbidden = ["DO NOT INCLUDE ME", "1000 lines of returned text", "Executed command", "Exit code 0"]
    for x in expected:
        assert x in r.corpus, x
    for x in forbidden:
        assert x not in r.corpus, x
    assert r.parser_mode == "v1.4-markers"
    assert count_fragments(r.included, "model_tool_input") == 1
    assert count_fragments(r.excluded, "tool_result") == 1
    print("SELF_TEST_PASS")
    return 0


def main(argv: Optional[List[str]] = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("inputs", nargs="*", help="Markdown export file(s) or directories")
    ap.add_argument("-o", "--output-dir", type=Path, help="write generated files here")
    ap.add_argument("--recursive", action="store_true", help="recurse into input directories")
    ap.add_argument("--strict", action="store_true", help="exit 3 unless v1.4 marker parsing is clean and unambiguous")
    ap.add_argument("--self-test", action="store_true", help="run embedded deterministic parser tests")
    args = ap.parse_args(argv)

    if args.self_test:
        return self_test()
    if not args.inputs:
        ap.error("provide at least one .md input or use --self-test")

    files = expand_inputs(args.inputs, args.recursive)
    if not files:
        print("No Markdown files found.", file=sys.stderr)
        return 2

    rc = 0
    for source in files:
        try:
            rc = max(rc, process_file(source, args.output_dir, args.strict))
        except Exception as exc:
            print(f"ERROR {source}: {exc}", file=sys.stderr)
            rc = max(rc, 2)
    return rc


if __name__ == "__main__":
    raise SystemExit(main())
