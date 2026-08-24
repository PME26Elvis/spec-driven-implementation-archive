# Grok Web chat exporters

Utilities used to archive Grok Web implementation conversations and derive a plain-text model-output corpus.

## Files

- `grok-console-export-v1.4.js` — browser DevTools exporter for the full Grok Markdown conversation.
- `extract-grok-chat-output.py` — post-processor for v1.4 `GROK_EXPORT` provenance markers.

## Basic usage

```bash
python tools/chat-exporters/grok/extract-grok-chat-output.py --self-test
```

Then process an exported Markdown file:

```bash
python tools/chat-exporters/grok/extract-grok-chat-output.py --strict path/to/conversation.md
```

The extractor writes a sibling `*.grok-output.txt` and `*.grok-output.meta.json`. The TXT is intended for downstream tokenizer/statistics work; tokenization itself is deliberately a separate step.
