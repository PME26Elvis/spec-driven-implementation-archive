#!/bin/bash
set -u
D=/mnt/data/darc_review_work/darc/bin/darc
B=$(mktemp -d /tmp/darc_int_XXXX)
S=$B/src; R=$B/repo; mkdir -p "$S"; printf old > "$S/f"; $D init "$R" >/dev/null
$D --repo "$R" snapshot create "$S" --timestamp 1 >/dev/null
printf new-content-that-is-different > "$S/f"; $D --repo "$R" snapshot create "$S" --timestamp 2 >/dev/null
LIST=$($D --repo "$R" snapshot list)
OLD=$(echo "$LIST"|awk 'NR==3{print $1}'); NEW=$(echo "$LIST"|awk 'NR==2{print $1}')
# delete old ref and run gc; should reclaim old-only objects while new ref remains
$D --repo "$R" snapshot delete "$OLD" --yes >/dev/null
GC=$($D --repo "$R" gc)
echo "GC_PARTIAL: $GC"
# delete a FILE object (type byte 2) from current graph. find first object with byte 8=2
FILEOBJ=$(python3 - "$R" <<'PY'
import os,sys
r=sys.argv[1]
for root,ds,fs in os.walk(r+'/objects/sha256'):
 for f in fs:
  p=os.path.join(root,f)
  try:
   with open(p,'rb') as h:b=h.read(9)
   if len(b)>=9 and b[:8]==b'DARCOBJ1' and b[8]==2:
    print(p); raise SystemExit
  except OSError:pass
PY
)
cp "$FILEOBJ" "$B/file.bak"; rm "$FILEOBJ"
set +e
VO=$($D --repo "$R" verify --level full 2>&1); VE=$?
set -e
echo "MISSING_FILE_VERIFY_EC=$VE OUT=$VO"
# restore should fail, proving missing object is actually needed somewhere if selected first file object happens live maybe could be old. restore current
rm -rf "$B/out"
set +e
RO=$($D --repo "$R" restore "$NEW" --to "$B/out" 2>&1); RE=$?
set -e
echo "MISSING_FILE_RESTORE_EC=$RE OUT=$RO"
# head deletion on separate repo
R2=$B/r2; S2=$B/s2; mkdir "$S2"; echo 1 > "$S2/x"; $D init "$R2" >/dev/null; $D --repo "$R2" snapshot create "$S2" --timestamp 1 >/dev/null; echo 2 > "$S2/x"; $D --repo "$R2" snapshot create "$S2" --timestamp 2 >/dev/null
HBEFORE=$(cat "$R2/HEAD"); $D --repo "$R2" snapshot delete "$HBEFORE" --yes >/dev/null; HAFTER=$(cat "$R2/HEAD"); REFS=$(find "$R2/refs/snapshots" -type f | wc -l)
echo "HEAD_DELETE before=$HBEFORE after=$HAFTER refs=$REFS"
