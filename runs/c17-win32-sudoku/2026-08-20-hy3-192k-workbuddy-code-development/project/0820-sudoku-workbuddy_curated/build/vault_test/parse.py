import re, sys

# Extract PLAIN-DUMP hex from dump.txt
data = open(r'D:\0820-sudoku-workbuddy\build\vault_test\dump.txt', 'r', encoding='utf-8', errors='ignore').read()
m = re.search(r'PLAIN-DUMP:(.*?)aead encrypt', data, re.S)
if not m:
    # fallback: take everything after first PLAIN-DUMP:
    idx = data.find('PLAIN-DUMP:')
    hexblob = data[idx+len('PLAIN-DUMP:'):]
else:
    hexblob = m.group(1)
# gather all two-hex-digit tokens
tokens = re.findall(r'[0-9a-fA-F]{2}', hexblob)
b = bytes(int(t,16) for t in tokens)
print("total bytes:", len(b))

pos = 0
def u8():
    global pos
    v = b[pos]; pos += 1; return v
def u16():
    global pos
    v = b[pos] | (b[pos+1]<<8); pos += 2; return v
def u32():
    global pos
    v = b[pos] | (b[pos+1]<<8) | (b[pos+2]<<16) | (b[pos+3]<<24); pos += 4; return v
def u64():
    global pos
    v = 0
    for i in range(8):
        v |= b[pos+i] << (8*i)
    pos += 8
    return v
def raw(n):
    global pos
    v = b[pos:pos+n]; pos += n; return v

magic = raw(8); print("magic", magic)
pv = u16(); print("payload_version", pv)
drv = u16(); print("diff_rules_ver", drv)
gfv = u16(); print("gen_format_ver", gfv)
reserved = u16(); print("reserved", reserved)
slen = u32(); print("settings_len", slen)
setb = raw(slen); print("settings", list(setb))
gc = u32(); print("game_count", gc)
for gi in range(gc):
    print("=== GAME", gi, "at offset", pos, "===")
    rt = u16(); print("  record_type", rt)
    rv = u16(); print("  record_version", rv)
    rl = u32(); print("  record_len", rl)
    start = pos
    gid = raw(16); print("  id", list(gid))
    diff = u8(); print("  difficulty", diff)
    drfv = u16(); print("  diff_rules_ver", drfv)
    gfv2 = u16(); print("  gen_format_ver", gfv2)
    seed = u64(); print("  gen_seed", seed)
    orig = raw(81); print("  orig[0:18]", list(orig[:18]))
    cur = raw(81); print("  cur[0:18]", list(cur[:18]))
    notes = [u16() for _ in range(81)]
    origin = raw(81); print("  origin[0:18]", list(origin[:18]))
    aem = u64(); print("  active_elapsed_ms", aem)
    ce = u64(); print("  created_epoch_ms", ce)
    le = u64(); print("  last_played_epoch_ms", le)
    paused = u8(); print("  paused", paused)
    hv = u32(); print("  hints_viewed", hv)
    ha = u32(); print("  hints_applied", ha)
    hht = u8(); print("  highest_hint_tech", hht)
    uas = u8(); print("  used_auto_solve", uas)
    cg = u64(); print("  current_generation", cg)
    sg = u64(); print("  saved_generation", sg)
    uc = u32(); print("  undo_count", uc)
    for ui in range(uc):
        print("   -- undo tx", ui, "at", pos)
        ak = u8(); ar = u8(); cc = u16(); seq = u64()
        print("      ak", ak, "ar", ar, "change_count", cc, "seq", seq)
        for ci in range(cc):
            ci_idx = u8(); bv = u8(); av = u8(); bo = u8(); ao = u8(); bn = u16(); an = u16()
            print("      change", ci, "cell", ci_idx, "bv", bv, "av", av, "bo", bo, "ao", ao, "bn", bn, "an", an)
    rc = u32(); print("  redo_count", rc)
    for ri in range(rc):
        print("   -- redo tx", ri, "at", pos)
        ak = u8(); ar = u8(); cc = u16(); seq = u64()
        for ci in range(cc):
            u8(); u8(); u8(); u8(); u8(); u16(); u16()
    consumed = pos - start
    print("  CONSUMED", consumed, "expected rl", rl, "MATCH" if consumed==rl else "MISMATCH")
print("after games, pos", pos)
cc2 = u32(); print("completed_count", cc2)
for ci in range(cc2):
    print("=== COMPLETED", ci, "at", pos)
    rt = u16(); rv = u16(); rl = u32()
    start = pos
    raw(16); u8(); u16(); u16(); u64(); raw(81); raw(81); raw(81)
    u64(); u64(); u64(); u64(); u32(); u32(); u8(); u8(); u8(); u8()
    u32(); u32(); u32()
    consumed = pos - start
    print("  CONSUMED", consumed, "expected", rl, "MATCH" if consumed==rl else "MISMATCH")
crc = u32(); print("crc", hex(crc), "pos", pos, "len", len(b))
print("pos==len?", pos==len(b))
