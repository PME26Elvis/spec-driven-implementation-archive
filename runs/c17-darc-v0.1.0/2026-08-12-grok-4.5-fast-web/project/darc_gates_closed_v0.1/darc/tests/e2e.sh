#!/bin/bash
set -e
D=${DARC:-./bin/darc}
cp "$D" /tmp/darc_e2e_bin && chmod +x /tmp/darc_e2e_bin
D=/tmp/darc_e2e_bin
R=/tmp/darc_e2e_repo2
S=/tmp/darc_e2e_src2
OUT=/tmp/darc_e2e_out2
rm -rf "$R" "$S" "$OUT"
mkdir -p "$S/nested"
echo -n abc > "$S/a.txt"
echo -n xyz > "$S/nested/c.txt"
ln "$S/a.txt" "$S/a2.txt"
ln -sf a.txt "$S/rel"
$D init "$R"
$D --repo "$R" snapshot create "$S" --name first --timestamp 0
$D --repo "$R" verify --level scrub
ID=$($D --repo "$R" snapshot list | awk 'NR==2{print $1}')
$D --repo "$R" restore "$ID" --to "$OUT"
test -f "$OUT/$(basename $S)/a.txt"
test "$(cat "$OUT/$(basename $S)/a.txt")" = "abc"
# hardlink same inode
i1=$(stat -c %i "$OUT/$(basename $S)/a.txt")
i2=$(stat -c %i "$OUT/$(basename $S)/a2.txt")
test "$i1" = "$i2"
echo -n abcd > "$S/a.txt"
$D --repo "$R" snapshot create "$S" --name second --timestamp 1 --parent "$ID"
S1=$($D --repo "$R" snapshot list | awk 'NR==3{print $1}')
S2=$($D --repo "$R" snapshot list | awk 'NR==2{print $1}')
$D --repo "$R" snapshot diff "$S1" "$S2" --format json | grep -q modified
$D --repo "$R" stats --format svg | grep -q '<svg'
$D --repo "$R" stats --format ndjson | head -1 | grep -q stats
$D config validate examples/config.json
$D config validate examples/config.yaml
$D --repo "$R" snapshot delete "$S1" --yes
$D --repo "$R" gc --dry-run
echo E2E_PASS
