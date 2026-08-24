#!/bin/bash
set -e
OUT=build/test-output
mkdir -p "$OUT"
{
  echo "# Evidence summary"
  date -u
  make test
  ls -la build/bin/
} > "$OUT/evidence.txt" 2>&1
echo '{"status":"partial"}' > "$OUT/summary.json"
echo wrote "$OUT"
