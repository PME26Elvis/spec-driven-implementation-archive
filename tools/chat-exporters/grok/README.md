# Grok Web chat exporters

Utilities used to archive Grok Web implementation conversations and derive a plain-text model-output corpus.

## Files

- `grok-console-export-v1.4.js` — browser DevTools exporter for the full Grok Markdown conversation.
- `extract-grok-chat-output.py` — post-processor for v1.4 `GROK_EXPORT` provenance markers.

## Corpus policy

The TXT corpus is intended to measure model-authored output while repository source/document content is measured separately (for example with Repomix), so repository payload must not be double-counted.

Included:

- visible Grok reasoning/thinking prose;
- Grok-authored Bash/tool command and control text;
- final Grok responses.

Excluded:

- user messages;
- terminal stdout/stderr and other tool results;
- Read-file/tool-return contents;
- Write/Edit-file bodies;
- repository-file bodies embedded in shell heredocs or `apply_patch`;
- large multiline repository literals embedded in repo-mutating Python heredocs;
- UI/status chrome.

Runtime/ad-hoc test payloads written under paths such as `/tmp/` are retained, because they are model-authored output rather than archived repository contents.

The extractor does not inject category labels or replacement prose into the TXT. If a repository payload is removed from a command, only the original model-authored command/control scaffolding remains.

## Basic usage

```bash
python tools/chat-exporters/grok/extract-grok-chat-output.py --self-test
```

Then process an exported Markdown file:

```bash
python tools/chat-exporters/grok/extract-grok-chat-output.py --strict path/to/conversation.md
```

The extractor writes a sibling `*.grok-output.txt` and `*.grok-output.meta.json`. The TXT is intended for downstream tokenizer/statistics work; tokenization itself is deliberately a separate step.

`--strict` succeeds only for clean v1.4 provenance-marker parsing with no warnings or ambiguity.
