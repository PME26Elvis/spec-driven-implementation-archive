# Category C - Handwritten JSON Configuration Parser
from testlib import *

s = suite("C-JSONParser")

# Helpers: write config, run config validate, capture rc
def cfg(repo, text):
    repo.write(".cvc/config.json", text, mode="wb")

def cval(repo, expected_rc=0):
    p = repo.run("config", "validate")
    return p

# C01: valid minimal config
def case_c01():
    r = new_repo("c01"); r.init()
    cfg(r, b'{"format_version":1}')
    p = cval(r)
    assert p.returncode == 0, "valid minimal should pass: %s" % p.stderr.decode()
    assert b"valid" in p.stdout.lower()
s.test("C01 valid minimal config", case_c01)

# C02: valid full config
def case_c02():
    r = new_repo("c02"); r.init()
    cfg(r, b'{"format_version":1,"save":{"show_diffstat":true},'
          b'"tracking":{"include":["**"],"exclude":[".cvc/**"]},'
          b'"diffstat":{"include":["src/**"],"exclude":[]}}')
    p = cval(r)
    assert p.returncode == 0, "valid full config should pass: %s" % p.stderr.decode()
s.test("C02 valid full config", case_c02)

# C03: nested arrays/objects parse (schema-valid via valid keys)
def case_c03():
    r = new_repo("c03"); r.init()
    cfg(r, b'{"format_version":1,"tracking":{"include":["**","src/**/*.c","a/**/b"],'
          b'"exclude":["x","y/z"]},"diffstat":{"include":["p","q"],"exclude":["r"]}}')
    p = cval(r)
    assert p.returncode == 0, p.stderr.decode()
s.test("C03 nested arrays/objects", case_c03)

# C04: escaped string characters (quote, backslash, control escapes)
def case_c04():
    r = new_repo("c04"); r.init()
    cfg(r, b'{"format_version":1,"save":{"show_diffstat":true},"diffstat":'
          b'{"include":["a\\\"b","c\\\\d","e\\nf","g\\tb","h\\rb","i\\bb","j\\fb"],'
          b'"exclude":[]}}')
    p = cval(r)
    assert p.returncode == 0, p.stderr.decode()
s.test("C04 escaped string characters", case_c04)

# C05: BMP Unicode escape
def case_c05():
    r = new_repo("c05"); r.init()
    cfg(r, '{"format_version":1,"diffstat":{"include":["\\u4e2d\\u6587"],"exclude":[]}}'.encode("utf-8"))
    p = cval(r)
    assert p.returncode == 0, p.stderr.decode()
s.test("C05 BMP Unicode escape", case_c05)

# C06: surrogate pair to UTF-8 (emoji U+1F600 = D83D DE00)
def case_c06():
    r = new_repo("c06"); r.init()
    cfg(r, '{"format_version":1,"diffstat":{"include":["\\ud83d\\ude00"],"exclude":[]}}'.encode("utf-8"))
    p = cval(r)
    assert p.returncode == 0, "surrogate pair should parse: %s" % p.stderr.decode()
s.test("C06 surrogate pair to UTF-8", case_c06)

# C07: unpaired surrogate rejection
def case_c07():
    r = new_repo("c07"); r.init()
    for bad in ['{"format_version":1,"diffstat":{"include":["\\ud83d"],"exclude":[]}}',
                '{"format_version":1,"diffstat":{"include":["\\ude00"],"exclude":[]}}']:
        cfg(r, bad.encode("utf-8"))
        p = cval(r, 1)
        assert p.returncode != 0, "unpaired surrogate must be rejected: %r" % bad
s.test("C07 unpaired surrogate rejection", case_c07)

# C08: invalid UTF-8 in literal string rejected
def case_c08():
    r = new_repo("c08"); r.init()
    cfg(r, b'{"format_version":1,"diffstat":{"include":["a\xff\xfe"],"exclude":[]}}')
    p = cval(r, 1)
    assert p.returncode != 0, "invalid UTF-8 must be rejected"
s.test("C08 invalid UTF-8 rejection", case_c08)

# C09: duplicate key rejection incl escape-equivalent spellings
def case_c09():
    r = new_repo("c09"); r.init()
    for bad in [b'{"format_version":1,"format_version":1}',
                '{"format_version":1,"diffstat":{"include":["a"],"include":["b"]}}'.encode(),
                '{"format_version":1,"diffstat":{"\\u0069nclude":["a"],"include":["b"]}}'.encode(),
                b'{"format_version":1,"save":{"show_diffstat":true,"show_diffstat":false}}']:
        cfg(r, bad)
        p = cval(r, 1)
        assert p.returncode != 0, "duplicate key must be rejected: %r" % bad
s.test("C09 duplicate key rejection", case_c09)

# C10: trailing comma rejection
def case_c10():
    r = new_repo("c10"); r.init()
    for bad in [b'{"format_version":1,}', b'{"format_version":1,"save":{}},' ,
                b'{"format_version":1,"tracking":{"include":["**",]}}']:
        cfg(r, bad)
        p = cval(r, 1)
        assert p.returncode != 0, "trailing comma must be rejected: %r" % bad
s.test("C10 trailing comma rejection", case_c10)

# C11: comments rejection
def case_c11():
    r = new_repo("c11"); r.init()
    for bad in [b'// comment\n{"format_version":1}',
                b'{"format_version":1} /* c */',
                b'{"format_version":1,"save":{ /*c*/ }}',
                b'{"format_version":1,"tracking":{"include":["**"],# c\n"exclude":[]}}']:
        cfg(r, bad)
        p = cval(r, 1)
        assert p.returncode != 0, "comments must be rejected: %r" % bad
s.test("C11 comments rejection", case_c11)

# C12: trailing garbage rejection
def case_c12():
    r = new_repo("c12"); r.init()
    for bad in [b'{"format_version":1}x', b'{"format_version":1}  true',
                b'{"format_version":1}{"a":1}', b'{"format_version":1}"str"']:
        cfg(r, bad)
        p = cval(r, 1)
        assert p.returncode != 0, "trailing garbage must be rejected: %r" % bad
s.test("C12 trailing garbage rejection", case_c12)

# C13: numeric overflow rejection (huge integer literal)
def case_c13():
    r = new_repo("c13"); r.init()
    cfg(r, b'{"format_version":99999999999999999999999999999999999999}')
    p = cval(r, 1)
    assert p.returncode != 0, "overflowing number must be rejected"
s.test("C13 numeric overflow rejection", case_c13)

# C14: unknown schema key rejection at top level and nested levels
def case_c14():
    r = new_repo("c14"); r.init()
    for bad in [b'{"format_version":1,"foo":1}',
                b'{"format_version":1,"save":{"foo":1}}',
                b'{"format_version":1,"tracking":{"foo":1}}',
                b'{"format_version":1,"diffstat":{"foo":1}}',
                b'{"format_version":1,"version":2}']:
        cfg(r, bad)
        p = cval(r, 1)
        assert p.returncode != 0, "unknown schema key must be rejected: %r" % bad
s.test("C14 unknown schema key rejection", case_c14)

# C15: wrong schema type rejection
def case_c15():
    r = new_repo("c15"); r.init()
    for bad in [b'{"format_version":"1"}',
                b'{"format_version":1.5}',
                b'{"format_version":true}',
                b'{"format_version":[1]}',
                b'{"format_version":1,"save":"x"}',
                b'{"format_version":1,"save":5}',
                b'{"format_version":1,"tracking":{"include":"**"}}',
                b'{"format_version":1,"tracking":{"include":[1]}}',
                b'{"format_version":1,"diffstat":{"exclude":[{}]}}']:
        cfg(r, bad)
        p = cval(r, 1)
        assert p.returncode != 0, "wrong type must be rejected: %r" % bad
s.test("C15 wrong schema type rejection", case_c15)

# C16: UTF-8 BOM rejection
def case_c16():
    r = new_repo("c16"); r.init()
    cfg(r, b'\xef\xbb\xbf{"format_version":1}')
    p = cval(r, 1)
    assert p.returncode != 0, "UTF-8 BOM must be rejected"
s.test("C16 UTF-8 BOM rejection", case_c16)

# C17: \u0000 parsed length-safely then rejected semantically in a pattern/key context
def case_c17():
    r = new_repo("c17"); r.init()
    cfg(r, b'{"format_version":1,"diffstat":{"include":["a\\u0000b"],"exclude":[]}}')
    p = cval(r, 1)
    assert p.returncode != 0, "\\u0000 in a pattern must be rejected semantically, not truncated"
    # also as a key: must not be silently accepted
    cfg(r, b'{"format_version":1,"diffstat":{"in\\u0000clude":["a"],"exclude":[]}}')
    p = cval(r, 1)
    assert p.returncode != 0, "\\u0000 in a key must be rejected"
s.test("C17 \\u0000 parsed length-safely", case_c17)

# C18: format_version 1.0 and 1e0 rejected semantically
def case_c18():
    r = new_repo("c18"); r.init()
    for bad in [b'{"format_version":1.0}', b'{"format_version":1e0}',
                b'{"format_version":1E0}', b'{"format_version":1.0e0}']:
        cfg(r, bad)
        p = cval(r, 1)
        assert p.returncode != 0, "format_version as %r must be rejected semantically" % bad
s.test("C18 format_version 1.0/1e0 rejected", case_c18)

# C19: config show/validate operate on real parser; malformed config fails existing-repo
#      commands while init creates initial config without requiring one
def case_c19():
    # init creates a valid initial config
    r = new_repo("c19"); r.init()
    assert r.exists(".cvc/config.json"), "init must create initial config"
    p = r.run("config", "show")
    assert p.returncode == 0, p.stderr.decode()
    assert b"format_version" in p.stdout
    # malformed config fails an existing-repo command (status)
    cfg(r, b'{"format_version":1,}')
    p = r.run("status")
    assert p.returncode != 0, "status must fail on malformed config"
    assert b"config" in (p.stderr or b"").lower() or b"json" in (p.stderr or b"").lower(), \
        "status error should mention config: %r" % p.stderr.decode()
s.test("C19 config show/validate + init", case_c19)

# C20: malformed JSON-number spellings rejected lexically, not partially consumed
def case_c20():
    r = new_repo("c20"); r.init()
    for bad in [b'{"format_version":01}',
                b'{"format_version":1.}',
                b'{"format_version":+1}',
                b'{"format_version":1e}',
                b'{"format_version":.5}',
                b'{"format_version":-}',
                b'{"format_version":1..2}',
                b'{"format_version":--1}',
                b'{"format_version":NaN}',
                b'{"format_version":Infinity}',
                b'{"format_version":-Infinity}']:
        cfg(r, bad)
        p = cval(r, 1)
        assert p.returncode != 0, "malformed number %r must be rejected lexically" % bad
s.test("C20 malformed number spellings rejected", case_c20)
