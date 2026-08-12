#!/bin/bash
# Maps major TEST_CATALOG areas to automated checks
set -e
D=${1:-./bin/darc}
cp "$D" /tmp/darc_cat && chmod +x /tmp/darc_cat
D=/tmp/darc_cat
PASS=0; FAIL=0
ok() { echo "PASS $1"; PASS=$((PASS+1)); }
bad() { echo "FAIL $1"; FAIL=$((FAIL+1)); }

# ALG-SHA256
$D --version >/dev/null && ok ALG-CLI-VERSION || bad ALG-CLI-VERSION

# Build algorithm tests
gcc -std=c17 -O2 -Iinclude -o /tmp/ta tests/test_algorithms.c src/sha256.c src/crc32c.c src/buzhash.c src/lzh1.c 2>/dev/null
/tmp/ta >/dev/null && ok ALG-GOLDEN || bad ALG-GOLDEN

# CONFIG
$D config validate examples/config.json >/dev/null && ok CFG-JSON || bad CFG-JSON
$D config validate examples/config.yaml >/dev/null && ok CFG-YAML || bad CFG-YAML
$D config validate /etc/hosts 2>/dev/null && bad CFG-EXT || ok CFG-EXT-REJECT

# REPO-INIT
rm -rf /tmp/cat_repo /tmp/cat_src
mkdir -p /tmp/cat_src
echo -n abc > /tmp/cat_src/a.txt
ln /tmp/cat_src/a.txt /tmp/cat_src/b.txt
$D init /tmp/cat_repo && ok REPO-INIT || bad REPO-INIT
test -f /tmp/cat_repo/FORMAT && ok REPO-FORMAT || bad REPO-FORMAT

# SNAP
$D --repo /tmp/cat_repo snapshot create /tmp/cat_src --name t --timestamp 0 >/dev/null && ok SNAP-CREATE || bad SNAP-CREATE
$D --repo /tmp/cat_repo snapshot list | grep -q SNAPSHOT && ok SNAP-LIST || bad SNAP-LIST
ID=$($D --repo /tmp/cat_repo snapshot list | awk 'NR==2{print $1}')
$D --repo /tmp/cat_repo snapshot show $ID | grep -q Files && ok SNAP-SHOW || bad SNAP-SHOW

# VERIFY
$D --repo /tmp/cat_repo verify --level scrub | grep -q OK && ok VER-SCRUB || bad VER-SCRUB

# RESTORE + HARDLINK
rm -rf /tmp/cat_out
$D --repo /tmp/cat_repo restore $ID --to /tmp/cat_out >/dev/null && ok REST-FULL || bad REST-FULL
i1=$(stat -c %i /tmp/cat_out/cat_src/a.txt)
i2=$(stat -c %i /tmp/cat_out/cat_src/b.txt)
test "$i1" = "$i2" && ok REST-HARDLINK || bad REST-HARDLINK

# DIFF
echo -n abcd > /tmp/cat_src/a.txt
$D --repo /tmp/cat_repo snapshot create /tmp/cat_src --name t2 --timestamp 1 --parent $ID >/dev/null
ID2=$($D --repo /tmp/cat_repo snapshot list | awk 'NR==2{print $1}')
$D --repo /tmp/cat_repo snapshot diff $ID $ID2 --format json | grep -q modified && ok DIFF-JSON || bad DIFF-JSON

# STATS formats
$D --repo /tmp/cat_repo stats --format json | grep -q snapshots && ok STATS-JSON || bad STATS-JSON
$D --repo /tmp/cat_repo stats --format ndjson | head -1 | grep -q type && ok STATS-NDJSON || bad STATS-NDJSON
$D --repo /tmp/cat_repo stats --format svg | grep -q '<svg' && ok STATS-SVG || bad STATS-SVG

# GC
$D --repo /tmp/cat_repo snapshot delete $ID --yes >/dev/null
$D --repo /tmp/cat_repo gc --dry-run >/dev/null && ok GC-DRY || bad GC-DRY

# INDEX
$D --repo /tmp/cat_repo index rebuild | grep -q rebuilt && ok IDX-REBUILD || bad IDX-REBUILD

# PARITY REPAIR
rm -rf /tmp/cat_par /tmp/cat_ps
mkdir -p /tmp/cat_ps
for i in 1 2 3 4 5 6 7 8; do python3 -c "open('/tmp/cat_ps/f$i','wb').write(bytes([$i])*40000)"; done
$D init /tmp/cat_par
$D --repo /tmp/cat_par snapshot create /tmp/cat_ps --timestamp 0 >/dev/null
MEM=$(awk 'NR==1{print $2}' /tmp/cat_par/parity/CATALOG 2>/dev/null)
if [ -n "$MEM" ] && [ ${#MEM} -eq 64 ]; then
  OP="/tmp/cat_par/objects/sha256/${MEM:0:2}/${MEM:2}"
  rm -f "$OP"
  $D --repo /tmp/cat_par verify --level full --repair >/dev/null
  test -f "$OP" && ok PARITY-REPAIR || bad PARITY-REPAIR
else
  bad PARITY-REPAIR-NOCATALOG
fi

# JOURNAL
echo "op=test" > /tmp/cat_repo/journal/current
$D --repo /tmp/cat_repo snapshot create /tmp/cat_src --timestamp 2 >/dev/null
test ! -f /tmp/cat_repo/journal/current && ok JOURNAL-RECOVER || bad JOURNAL-RECOVER

# CORRUPT DETECT
CHUNK=$(find /tmp/cat_repo/objects -type f | head -1)
cp "$CHUNK" /tmp/c.bak
python3 -c "p=open('$CHUNK','r+b'); p.seek(35); p.write(b'\\xff'); p.close()"
set +e
$D --repo /tmp/cat_repo verify --level full >/dev/null
EC=$?
set -e
cp /tmp/c.bak "$CHUNK"
test $EC -eq 6 && ok CORRUPT-DETECT || bad CORRUPT-DETECT

echo "----"
echo "PASSED=$PASS FAILED=$FAIL"
test $FAIL -eq 0
