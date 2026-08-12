#!/bin/bash
set -e
D=${1:-./bin/darc}
cp "$D" /tmp/darc_st && chmod +x /tmp/darc_st
D=/tmp/darc_st
R=/tmp/darc_stress_repo
S=/tmp/darc_stress_src
rm -rf "$R" "$S"
mkdir -p "$S"
# randomized small files
python3 - <<'PY'
import os, random
random.seed(42)
os.makedirs("/tmp/darc_stress_src", exist_ok=True)
for i in range(30):
    n = random.randint(0, 8000)
    data = bytes(random.getrandbits(8) for _ in range(n))
    open(f"/tmp/darc_stress_src/f{i}.bin","wb").write(data)
PY
$D init "$R"
$D --repo "$R" snapshot create "$S" --timestamp 0
$D --repo "$R" verify --level full
$D --repo "$R" snapshot create "$S" --timestamp 1
$D --repo "$R" verify --level scrub
echo STRESS_OK
