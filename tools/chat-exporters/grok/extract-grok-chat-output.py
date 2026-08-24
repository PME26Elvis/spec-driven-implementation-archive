#!/usr/bin/env python3
"""Extract a plain-text corpus of Grok model-authored output from Grok Markdown exports.

Recommended input: Grok Console Export v1.4 Markdown. v1.4 embeds invisible
``GROK_EXPORT`` provenance comments derived from the live Grok DOM.

Corpus policy
=============

This corpus is intended to estimate *model output* while repository source/docs
are measured separately (for example with Repomix).

INCLUDE
  * visible Grok reasoning/thinking prose
  * Grok-authored Bash/tool command and control text
  * Grok final responses

EXCLUDE
  * user messages
  * terminal stdout/stderr/tool results
  * Read-file/tool-return contents
  * Write/Edit-file bodies
  * repository-file payloads embedded in Bash heredocs/apply_patch
  * large multiline repository literals embedded in repo-mutating Python heredocs
  * UI/activity labels and metadata

The generated TXT contains no category labels or replacement prose injected by
this script. When repository payload is stripped from a command, only the
model-authored command/control scaffolding remains.

A sibling ``*.grok-output.meta.json`` records provenance, category character
counts, exclusions, warnings, and ambiguity diagnostics. Tokenization is
deliberately a separate stage.
"""
from __future__ import annotations

import argparse
import hashlib
import io
import json
import re
import sys
import tokenize
from dataclasses import dataclass, field
from pathlib import Path
from typing import Iterable, List, Optional, Tuple

VERSION = "1.2.0"

MARKER_RE = re.compile(
    r"^\s*<!--\s*GROK_EXPORT:(BEGIN|END)\s+kind=([a-zA-Z0-9_-]+)"
    r"(?:\s+subtype=([a-zA-Z0-9_-]+))?\s*-->\s*$"
)
TURN_RE = re.compile(r"^##\s+(Human|Grok)(?:\s+\[[^\]]+\])?\s*$", re.M)
FENCE_OPEN_RE = re.compile(r"^\s*(`{3,}|~{3,})([^`]*)$")
HEREDOC_RE = re.compile(r"<<-?\s*(['\"]?)([A-Za-z_][A-Za-z0-9_]*)\1")

INCLUDE_KINDS = {"model_reasoning", "assistant_final"}
EXCLUDE_KINDS = {"tool_result", "ui_event"}
WRITE_BODY_SUBTYPES = {
    "write_file",
    "write-file",
    "edit_file",
    "edit-file",
    "edited_file",
    "edited_files",
    "apply_patch",
}

TEMP_PATH_PREFIXES = (
    "/tmp/",
    "/var/tmp/",
    "$TMPDIR/",
    "${TMPDIR}/",
    "tmp/",
    "./tmp/",
)

REPO_PATH_HINT_RE = re.compile(
    r"(?:^|[^A-Za-z0-9_])(?:src|include|tests?|docs?|tools?|fixtures?|scripts?|examples?|"
    r"bench(?:marks?)?|cmake|assets?|resources?)(?:[/\\]|$)"
    r"|(?:^|[/\\])(?:Makefile|CMakeLists\.txt|README(?:\.[A-Za-z0-9_-]+)?|"
    r"\.github)(?:$|[/\\])"
    r"|\.(?:c|h|cc|cpp|hpp|rs|go|java|cs|py|js|ts|tsx|jsx|sh|md|rst|txt|"
    r"json|ya?ml|toml|ini|cfg|cmake)$",
    re.I,
)

PYTHON_MUTATION_RE = re.compile(
    r"(?:\.write_text\s*\(|\.write_bytes\s*\(|"
    r"\bopen\s*\([^,\n]+,\s*['\"][^'\"]*[wax+][^'\"]*['\"]|"
    r"\b(?:write|writelines)\s*\()",
    re.I,
)


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
    """Remove one Markdown fence pair inserted around a DOM <pre> tool input."""
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
    for idx in range(len(stack) - 1, -1, -1):
        fr = stack[idx]
        if fr.kind == kind and (not subtype or fr.subtype == subtype):
            popped = fr
            del stack[idx:]
            return popped
    return None


def _unquote_shell_word(word: str) -> str:
    word = word.strip()
    if len(word) >= 2 and word[0] == word[-1] and word[0] in "'\"":
        return word[1:-1]
    return word


def _redirect_target(opener: str) -> Optional[str]:
    redirs = re.findall(r"(?:^|[\s])(?:>>|>)\s*([^\s;&|]+)", opener)
    if redirs:
        return _unquote_shell_word(redirs[-1])

    m = re.search(r"(?:^|[|;&]\s*|\s)tee(?:\s+-[A-Za-z]+)*\s+([^\s;&|]+)", opener)
    if m:
        return _unquote_shell_word(m.group(1))
    return None


def _is_temp_path(path: str) -> bool:
    p = path.strip()
    return any(p.startswith(prefix) for prefix in TEMP_PATH_PREFIXES)


def _looks_repo_path(path: str) -> bool:
    p = path.strip()
    if not p or p in {"/dev/null", "/dev/stdout", "/dev/stderr"}:
        return False
    if _is_temp_path(p):
        return False
    if not p.startswith("/"):
        return True
    return bool(REPO_PATH_HINT_RE.search(p))


def _line_offsets(text: str) -> List[int]:
    offsets = [0]
    for m in re.finditer("\n", text):
        offsets.append(m.end())
    return offsets


def _pos_to_offset(offsets: List[int], pos: Tuple[int, int], text_len: int) -> int:
    line, col = pos
    if line <= 0:
        return 0
    idx = line - 1
    if idx >= len(offsets):
        return text_len
    return min(text_len, offsets[idx] + col)


def _empty_string_token(token_text: str) -> Optional[str]:
    m = re.match(r'(?is)^([rubf]*)(\"{3}|\'{3})', token_text)
    if not m:
        return None
    quote = m.group(2)
    if not token_text.endswith(quote):
        return None
    return f"{m.group(1)}{quote}{quote}"


def strip_large_python_repo_literals(
    body: str,
    source: str,
    min_chars: int = 200,
) -> Tuple[str, List[Fragment]]:
    """Strip large multiline literals from repo-mutating Python heredocs."""
    if not PYTHON_MUTATION_RE.search(body):
        return body, []
    if not REPO_PATH_HINT_RE.search(body):
        return body, []

    try:
        toks = list(tokenize.generate_tokens(io.StringIO(body).readline))
    except (tokenize.TokenError, IndentationError):
        return body, []

    offsets = _line_offsets(body)
    replacements: List[Tuple[int, int, str, str]] = []
    for tok in toks:
        if tok.type != tokenize.STRING or len(tok.string) < min_chars or "\n" not in tok.string:
            continue
        replacement = _empty_string_token(tok.string)
        if replacement is None:
            continue
        start = _pos_to_offset(offsets, tok.start, len(body))
        end = _pos_to_offset(offsets, tok.end, len(body))
        replacements.append((start, end, replacement, tok.string))

    if not replacements:
        return body, []

    out = body
    excluded: List[Fragment] = []
    for start, end, replacement, original in reversed(replacements):
        out = out[:start] + replacement + out[end:]
        excluded.append(Fragment("repository_payload", "python_multiline_literal", original, source))
    excluded.reverse()
    return out, excluded


def strip_bash_repository_payloads(text: str) -> Tuple[str, List[Fragment]]:
    """Remove repo file bodies while preserving model-authored shell control text."""
    lines = clean_fragment(text).splitlines()
    out: List[str] = []
    excluded: List[Fragment] = []
    i = 0

    while i < len(lines):
        line = lines[i]
        hm = HEREDOC_RE.search(line)
        if not hm:
            out.append(line)
            i += 1
            continue

        delimiter = hm.group(2)
        j = i + 1
        while j < len(lines):
            candidate = lines[j]
            if candidate == delimiter or candidate.lstrip("\t") == delimiter:
                break
            j += 1

        if j >= len(lines):
            out.append(line)
            i += 1
            continue

        body = "\n".join(lines[i + 1:j])
        target = _redirect_target(line)
        is_apply_patch = bool(re.search(r"(?:^|[\s;&|])apply_patch(?:\s|$)", line))
        repo_write = target is not None and _looks_repo_path(target)

        if is_apply_patch or repo_write:
            out.append(line)
            out.append(lines[j])
            if body:
                subtype = "apply_patch" if is_apply_patch else "shell_heredoc_file"
                excluded.append(Fragment("repository_payload", subtype, body, "bash-filter"))
            i = j + 1
            continue

        if re.search(r"(?:^|[\s;&|])python(?:3(?:\.\d+)?)?(?:\s|$)", line):
            filtered, py_excluded = strip_large_python_repo_literals(
                body, "bash-python-heredoc"
            )
            out.append(line)
            if filtered:
                out.extend(filtered.splitlines())
            out.append(lines[j])
            excluded.extend(py_excluded)
            i = j + 1
            continue

        out.append(line)
        out.extend(lines[i + 1:j])
        out.append(lines[j])
        i = j + 1

    return clean_fragment("\n".join(out)), excluded


def filter_model_tool_input(frag: Fragment) -> Tuple[Optional[Fragment], List[Fragment]]:
    subtype = frag.subtype.lower().replace(" ", "_")
    text = strip_one_outer_code_fence(frag.text)

    if subtype in WRITE_BODY_SUBTYPES or (
        ("write" in subtype or "edit" in subtype) and "file" in subtype
    ):
        return None, [
            Fragment("repository_payload", subtype or "write_edit_file", text, frag.source)
        ]

    if subtype in {"bash", "shell", "command", "terminal"} or not subtype:
        filtered, removed = strip_bash_repository_payloads(text)
        if not filtered:
            return None, removed
        return Fragment(frag.kind, frag.subtype, filtered, frag.source), removed

    return Fragment(frag.kind, frag.subtype, text, frag.source), []


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
            included.append(
                Fragment("model_reasoning", "unmarked_thinking", txt, "v1.4-default")
            )

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
                    included.append(frag)
                elif frame.kind == "model_tool_input":
                    kept, removed = filter_model_tool_input(frag)
                    if kept is not None and clean_fragment(kept.text):
                        included.append(kept)
                    excluded.extend(removed)
                elif frame.kind in EXCLUDE_KINDS:
                    excluded.append(frag)
            continue

        for frame in stack:
            frame.lines.append(line)

        active_kinds = [fr.kind for fr in stack]
        if "assistant_thinking" in active_kinds and not any(
            k in INCLUDE_KINDS or k in EXCLUDE_KINDS or k == "model_tool_input"
            for k in active_kinds
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

    corpus_parts = [clean_fragment(f.text) for f in included if clean_fragment(f.text)]
    corpus = "\n\n".join(corpus_parts).strip() + ("\n" if corpus_parts else "")
    return ExtractionResult(
        corpus, included, excluded, ambiguous, "v1.4-markers", warnings
    )


def split_turns(markdown: str) -> List[Tuple[str, str]]:
    matches = list(TURN_RE.finditer(markdown))
    turns: List[Tuple[str, str]] = []
    for i, m in enumerate(matches):
        end = matches[i + 1].start() if i + 1 < len(matches) else len(markdown)
        body = markdown[m.end():end]
        body = re.sub(r"\n---\s*$", "", body.rstrip())
        turns.append((m.group(1), body.strip()))
    return turns


def extract_fenced_blocks(text: str) -> Tuple[str, List[str]]:
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


SHELLISH_RE = re.compile(
    r"(?:^|\n)\s*(?:cd\b|ls\b|cat\b|head\b|tail\b|grep\b|find\b|sed\b|awk\b|wc\b|"
    r"gcc\b|clang\b|make\b|cmake\b|python(?:3)?\b|perl\b|bash\b|sh\b|zip\b|unzip\b|"
    r"mkdir\b|cp\b|mv\b|rm\b|chmod\b|printf\b|echo\b|git\b|timeout\b|test\b|"
    r"for\s+\w+\s+in\b|[A-Z_][A-Z0-9_]*=|#\s*!)",
    re.M,
)
RESULTISH_RE = re.compile(
    r"(?:Exit code\s+\d+|^make(?:\[\d+\])?:|^gcc\s+-|^clang\s+-|^total\s+\d+|"
    r"^drwx|^-rw|^PASS\b|^FAIL\b|^warning:|^error:|^Archive:|^inflating:)",
    re.M | re.I,
)


def parse_legacy_export(markdown: str) -> ExtractionResult:
    included: List[Fragment] = []
    excluded: List[Fragment] = []
    ambiguous: List[Fragment] = []
    warnings = [
        "Legacy Markdown has no DOM provenance markers; tool-input vs tool-result separation is heuristic.",
        "For strict statistics, re-export the live conversation with v1.4.",
    ]

    for speaker, body in split_turns(markdown):
        if speaker == "Human":
            excluded.append(
                Fragment("user_message", "", clean_fragment(body), "legacy-heading")
            )
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
            included.append(
                Fragment("assistant_final", "", clean_fragment(response), "legacy-heading")
            )

        if thinking.strip():
            prose, blocks = extract_fenced_blocks(thinking)
            if prose:
                included.append(
                    Fragment(
                        "model_reasoning",
                        "legacy-prose",
                        prose,
                        "legacy-heuristic",
                    )
                )

            i = 0
            while i < len(blocks):
                block = blocks[i]
                shellish = bool(SHELLISH_RE.search(block))
                if shellish:
                    filtered, removed = strip_bash_repository_payloads(block)
                    if filtered:
                        included.append(
                            Fragment(
                                "model_tool_input",
                                "legacy-shell",
                                filtered,
                                "legacy-heuristic",
                            )
                        )
                    excluded.extend(removed)
                    if i + 1 < len(blocks):
                        nxt = blocks[i + 1]
                        if RESULTISH_RE.search(nxt) or not SHELLISH_RE.search(nxt):
                            excluded.append(
                                Fragment(
                                    "tool_result",
                                    "legacy-paired",
                                    nxt,
                                    "legacy-heuristic",
                                )
                            )
                            i += 2
                            continue
                    i += 1
                else:
                    ambiguous.append(
                        Fragment(
                            "ambiguous_code_block",
                            "",
                            block,
                            "legacy-heuristic",
                        )
                    )
                    i += 1

    corpus_parts = [clean_fragment(f.text) for f in included if clean_fragment(f.text)]
    corpus = "\n\n".join(corpus_parts).strip() + ("\n" if corpus_parts else "")
    return ExtractionResult(
        corpus, included, excluded, ambiguous, "legacy-heuristic", warnings
    )


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
            "subtypes": sorted(
                {f.subtype for f in fragments if f.kind == kind and f.subtype}
            ),
        }
        for kind in kinds
    }


def user_chars_from_turns(markdown: str) -> int:
    return sum(
        len(clean_fragment(body))
        for speaker, body in split_turns(markdown)
        if speaker == "Human"
    )


def build_meta(
    source: Path,
    source_bytes: bytes,
    result: ExtractionResult,
    corpus_bytes: bytes,
) -> dict:
    return {
        "schema": "grok-model-output-corpus-meta-v2",
        "extractor": f"extract-grok-chat-output.py/{VERSION}",
        "source": str(source),
        "source_sha256": sha256_bytes(source_bytes),
        "parser_mode": result.parser_mode,
        "corpus_sha256": sha256_bytes(corpus_bytes),
        "corpus_chars": len(result.corpus),
        "policy": {
            "purpose": (
                "compute-oriented visible model-output proxy with repository "
                "payload excluded to avoid double counting against repo statistics"
            ),
            "included": [
                "visible reasoning/thinking",
                "model-authored command/control text",
                "final Grok responses",
            ],
            "excluded": [
                "user messages",
                "terminal/tool results",
                "read-file results",
                "Write/Edit file bodies",
                "repository payloads in shell heredocs/apply_patch",
                "large repository literals in repo-mutating Python heredocs",
                "UI/status chrome",
            ],
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
    return (
        base_dir / f"{stem}.grok-output.txt",
        base_dir / f"{stem}.grok-output.meta.json",
    )


def process_file(source: Path, output_dir: Optional[Path], strict: bool) -> int:
    source_bytes = source.read_bytes()
    markdown = source_bytes.decode("utf-8", errors="replace")
    result = extract(markdown)
    corpus_bytes = result.corpus.encode("utf-8")
    txt_path, meta_path = output_paths(source, output_dir)
    meta = build_meta(source, source_bytes, result, corpus_bytes)
    meta["source_user_chars_observed"] = user_chars_from_turns(markdown)

    txt_path.write_bytes(corpus_bytes)
    meta_path.write_text(
        json.dumps(meta, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
    )

    print(f"{source}: {result.parser_mode}")
    print(f"  -> {txt_path} ({len(result.corpus):,} chars)")
    print(f"  -> {meta_path}")
    for kind, info in meta["included"].items():
        print(
            f"     include {kind}: {info['fragments']} fragment(s), "
            f"{info['chars']:,} chars"
        )
    for kind, info in meta["excluded"].items():
        print(
            f"     exclude {kind}: {info['fragments']} fragment(s), "
            f"{info['chars']:,} chars"
        )
    if result.ambiguous:
        print(f"     AMBIGUOUS: {len(result.ambiguous)} fragment(s)")
    for warning in result.warnings:
        print(f"     warning: {warning}")

    if strict and (
        result.parser_mode != "v1.4-markers"
        or result.ambiguous
        or result.warnings
    ):
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
    sample = r"""# Grok Conversation Export

## Human

DO NOT INCLUDE ME

---

## Grok [THINK]

### Thinking

<!-- GROK_EXPORT:BEGIN kind=assistant_thinking -->
<!-- GROK_EXPORT:BEGIN kind=model_reasoning -->
I should inspect and modify the repository.
<!-- GROK_EXPORT:END kind=model_reasoning -->

<!-- GROK_EXPORT:BEGIN kind=model_tool_input subtype=bash -->
```
grep -R "solver" src/
```
<!-- GROK_EXPORT:END kind=model_tool_input subtype=bash -->

<!-- GROK_EXPORT:BEGIN kind=tool_result subtype=bash -->
```
src/solver.c: returned terminal text
Exit code 0
```
<!-- GROK_EXPORT:END kind=tool_result subtype=bash -->

<!-- GROK_EXPORT:BEGIN kind=model_tool_input subtype=write_file -->
```
THIS WRITE FILE BODY MUST DISAPPEAR
```
<!-- GROK_EXPORT:END kind=model_tool_input subtype=write_file -->

<!-- GROK_EXPORT:BEGIN kind=model_tool_input subtype=bash -->
```
cat > src/generated.c <<'EOF'
REPOSITORY HEREDOC BODY MUST DISAPPEAR
EOF
cat > /tmp/query.sql <<'SQL'
SELECT 42;
SQL
apply_patch <<'PATCH'
*** Begin Patch
PATCH BODY MUST DISAPPEAR
*** End Patch
PATCH
```
<!-- GROK_EXPORT:END kind=model_tool_input subtype=bash -->

Unmarked visible reasoning line.
<!-- GROK_EXPORT:END kind=assistant_thinking -->

### Response

<!-- GROK_EXPORT:BEGIN kind=assistant_final -->
Done **successfully**.
<!-- GROK_EXPORT:END kind=assistant_final -->
"""

    r = extract(sample)

    expected = [
        "I should inspect and modify the repository.",
        'grep -R "solver" src/',
        "cat > src/generated.c <<'EOF'",
        "cat > /tmp/query.sql <<'SQL'",
        "SELECT 42;",
        "apply_patch <<'PATCH'",
        "Unmarked visible reasoning line.",
        "Done **successfully**.",
    ]
    forbidden = [
        "DO NOT INCLUDE ME",
        "returned terminal text",
        "Exit code 0",
        "THIS WRITE FILE BODY MUST DISAPPEAR",
        "REPOSITORY HEREDOC BODY MUST DISAPPEAR",
        "PATCH BODY MUST DISAPPEAR",
    ]
    for x in expected:
        assert x in r.corpus, x
    for x in forbidden:
        assert x not in r.corpus, x

    long_payload = "int generated_payload = 1;\n" * 40
    py_body = (
        'from pathlib import Path\n'
        'Path("src/from_python.c").write_text('
        + '"""'
        + long_payload
        + '"""'
        + ')\nprint("control survives")\n'
    )
    py_filtered, py_removed = strip_large_python_repo_literals(
        py_body, "self-test", min_chars=128
    )
    assert 'Path("src/from_python.c").write_text(' in py_filtered
    assert 'print("control survives")' in py_filtered
    assert "generated_payload" not in py_filtered
    assert py_removed

    assert r.parser_mode == "v1.4-markers"
    assert count_fragments(r.included, "model_tool_input") == 2
    assert count_fragments(r.excluded, "tool_result") == 1
    assert count_fragments(r.excluded, "repository_payload") >= 3
    assert not r.ambiguous
    assert not r.warnings

    print("SELF_TEST_PASS")
    return 0


def main(argv: Optional[List[str]] = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("inputs", nargs="*", help="Markdown export file(s) or directories")
    ap.add_argument("-o", "--output-dir", type=Path, help="write generated files here")
    ap.add_argument(
        "--recursive",
        action="store_true",
        help="recurse into input directories",
    )
    ap.add_argument(
        "--strict",
        action="store_true",
        help="exit 3 unless v1.4 marker parsing is clean and unambiguous",
    )
    ap.add_argument(
        "--self-test",
        action="store_true",
        help="run embedded deterministic parser tests",
    )
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
