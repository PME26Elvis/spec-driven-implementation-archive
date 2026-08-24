CC=gcc
CFLAGS=-std=c17 -Wall -Wextra -Wno-unused-function -Wno-sign-compare -D_POSIX_C_SOURCE=200809L -D_DEFAULT_SOURCE
INCLUDES=-Iinclude
LDFLAGS=-lX11 -lm -lpthread
BUILD=build/bin

CRYPTO=$(wildcard src/crypto/*.c)
CORE=$(CRYPTO) src/pager/pager.c src/btree/btree.c src/btree/composite_key.c \
     src/sql/lexer.c src/sql/parser.c src/schema/catalog.c src/api/edb_api.c src/wal/wal.c src/mvcc/mvcc.c src/pager/freelist.c src/record/overflow.c
CLI=src/cli/edb_main.c
CHECK=src/tools/edb_check_main.c
GUI=src/gui/gui_main.c

.PHONY: all clean test check

all: $(BUILD)/edb $(BUILD)/edb-check $(BUILD)/edb-gui $(BUILD)/edb-fixture $(BUILD)/edb-crashrun $(BUILD)/locscan

$(BUILD):
	mkdir -p $@

$(BUILD)/edb: $(CLI) $(CORE) | $(BUILD)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $(CLI) $(CORE)

$(BUILD)/edb-check: $(CHECK) $(CORE) | $(BUILD)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $(CHECK) $(CORE)

$(BUILD)/edb-gui: $(GUI) $(CORE) | $(BUILD)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $(GUI) $(CORE) $(LDFLAGS)

$(BUILD)/locscan: tools/locscan.c | $(BUILD)
	$(CC) $(CFLAGS) -o $@ tools/locscan.c

$(BUILD)/edb-fixture: src/tools/edb_fixture_main.c $(CORE) | $(BUILD)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ src/tools/edb_fixture_main.c $(CORE)

$(BUILD)/edb-crashrun: src/tools/edb_crashrun_main.c $(CORE) | $(BUILD)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ src/tools/edb_crashrun_main.c $(CORE)

$(BUILD)/edb-crashrun-stub-unused: | $(BUILD)
	printf 'int main(int c,char**v){(void)c;(void)v;return 0;}\n' | $(CC) $(CFLAGS) -x c - -o $@

clean:
	rm -rf build

test: all
	@mkdir -p build/test-output
	@echo "=== UT pager ===" && $(CC) $(CFLAGS) $(INCLUDES) -o /tmp/t_phdr tests/unit/test_pager_header.c src/pager/pager.c src/pager/freelist.c src/record/overflow.c $(CRYPTO) && /tmp/t_phdr
	@echo "=== UT btree del ===" && $(CC) $(CFLAGS) $(INCLUDES) -o /tmp/t_bdel tests/unit/test_btree_delete.c src/btree/btree.c src/pager/pager.c src/pager/freelist.c src/record/overflow.c $(CRYPTO) && /tmp/t_bdel
	@echo "=== UT aead ===" && $(CC) $(CFLAGS) $(INCLUDES) -o /tmp/t_aead tests/unit/test_crypto_aead.c $(CRYPTO) && /tmp/t_aead
	@echo "=== UT freelist ===" && $(CC) $(CFLAGS) $(INCLUDES) -o /tmp/t_fl tests/unit/test_freelist.c src/pager/freelist.c src/pager/pager.c src/record/overflow.c $(CRYPTO) && /tmp/t_fl
	@echo "=== UT cidx null ===" && $(CC) $(CFLAGS) $(INCLUDES) -o /tmp/t_cidxn tests/unit/test_cidx_null_unique.c src/btree/composite_key.c && /tmp/t_cidxn
	@echo "=== UT wal ===" && $(CC) $(CFLAGS) $(INCLUDES) -o /tmp/t_wal tests/unit/test_wal_roundtrip.c src/wal/wal.c src/pager/pager.c src/pager/freelist.c src/record/overflow.c $(CRYPTO) && /tmp/t_wal
	@echo "=== UT utf8 ===" && $(CC) $(CFLAGS) $(INCLUDES) -o /tmp/t_utf8 tests/unit/test_utf8_chinese.c src/api/edb_api.c src/schema/catalog.c src/sql/lexer.c src/sql/parser.c src/btree/btree.c src/btree/composite_key.c src/pager/pager.c src/pager/freelist.c src/record/overflow.c src/wal/wal.c src/mvcc/mvcc.c $(CRYPTO) && /tmp/t_utf8
	@echo "=== UT sha256 ===" && $(CC) $(CFLAGS) $(INCLUDES) -o /tmp/t_sha tests/unit/test_sha256.c src/crypto/sha256.c && /tmp/t_sha
	@echo "=== UT overflow ===" && $(CC) $(CFLAGS) $(INCLUDES) -o /tmp/t_ovf tests/unit/test_overflow.c src/record/overflow.c src/pager/pager.c src/pager/freelist.c $(CRYPTO) && /tmp/t_ovf
	@echo "=== UT mvcc ===" && $(CC) $(CFLAGS) $(INCLUDES) -o /tmp/t_mvcc tests/unit/test_mvcc.c src/mvcc/mvcc.c && /tmp/t_mvcc
	@echo "=== UT composite ===" && $(CC) $(CFLAGS) $(INCLUDES) -o /tmp/t_ck tests/unit/test_composite_key.c src/btree/composite_key.c && /tmp/t_ck
	@echo "=== UT btree orders ===" && $(CC) $(CFLAGS) $(INCLUDES) -o /tmp/t_ord tests/unit/test_btree_orders.c src/btree/btree.c src/pager/pager.c src/pager/freelist.c src/record/overflow.c $(CRYPTO) && /tmp/t_ord
	@echo "=== UT btree height-3 ===" && $(CC) $(CFLAGS) $(INCLUDES) -o /tmp/t_ht tests/unit/test_btree_height.c src/btree/btree.c src/pager/pager.c src/pager/freelist.c src/record/overflow.c $(CRYPTO) && /tmp/t_ht
	@echo "=== IT mvcc 8-readers ===" && $(CC) $(CFLAGS) $(INCLUDES) -o /tmp/t_rdr tests/integration/test_mvcc_readers.c src/mvcc/mvcc.c && /tmp/t_rdr
	@echo "=== IT crash/corruption ===" && $(CC) $(CFLAGS) $(INCLUDES) -o /tmp/t_ct tests/integration/test_crash_corruption.c 	  src/api/edb_api.c src/schema/catalog.c src/sql/lexer.c src/sql/parser.c src/btree/btree.c src/btree/composite_key.c 	  src/pager/pager.c src/pager/freelist.c src/record/overflow.c src/wal/wal.c src/mvcc/mvcc.c $(CRYPTO) && /tmp/t_ct
	@echo "=== IT id batch100 ===" && $(CC) $(CFLAGS) $(INCLUDES) -o /tmp/t_b100 tests/integration/test_id_batch100.c src/api/edb_api.c src/schema/catalog.c src/sql/lexer.c src/sql/parser.c src/btree/btree.c src/btree/composite_key.c src/pager/pager.c src/pager/freelist.c src/record/overflow.c src/wal/wal.c src/mvcc/mvcc.c $(CRYPTO) && /tmp/t_b100
	@echo "=== IT sql surface ===" && $(CC) $(CFLAGS) $(INCLUDES) -o /tmp/t_sqls tests/integration/test_sql_surface.c src/api/edb_api.c src/schema/catalog.c src/sql/lexer.c src/sql/parser.c src/btree/btree.c src/btree/composite_key.c src/pager/pager.c src/pager/freelist.c src/record/overflow.c src/wal/wal.c src/mvcc/mvcc.c $(CRYPTO) && /tmp/t_sqls
	@echo "=== UT pbkdf2 ===" && $(CC) $(CFLAGS) $(INCLUDES) -o /tmp/t_pbkdf2 tests/unit/test_pbkdf2.c src/crypto/pbkdf2.c src/crypto/hmac_sha256.c src/crypto/sha256.c && /tmp/t_pbkdf2
	@echo "=== IT e2e cli+check ===" && \
	  cp $(BUILD)/edb /tmp/edb && chmod +x /tmp/edb && \
	  cp $(BUILD)/edb-check /tmp/edb-check && chmod +x /tmp/edb-check && \
	  rm -f /tmp/t_e2e.edb && \
	  /tmp/edb -c /tmp/t_e2e.edb -e "CREATE TABLE t (id INTEGER PRIMARY KEY, v TEXT);" && \
	  /tmp/edb /tmp/t_e2e.edb -e "INSERT INTO t VALUES (1, 'ok');" && \
	  /tmp/edb /tmp/t_e2e.edb -e "INSERT INTO t VALUES (2, '中文');" && \
	  /tmp/edb /tmp/t_e2e.edb -e "UPDATE t SET v = 'upd' WHERE id = 1;" && \
	  /tmp/edb /tmp/t_e2e.edb -e "SELECT * FROM t WHERE id = 2;" && \
	  /tmp/edb /tmp/t_e2e.edb -e "SELECT * FROM t ORDER BY id DESC;" && \
	  /tmp/edb /tmp/t_e2e.edb -e "SELECT COUNT(*) FROM t;" && \
	  /tmp/edb /tmp/t_e2e.edb -e "VACUUM;" && \
	  /tmp/edb-check /tmp/t_e2e.edb && \
	  rm -f /tmp/t_enc.edb && \
	  /tmp/edb -c -p secret /tmp/t_enc.edb -e "CREATE TABLE e (id INTEGER PRIMARY KEY);" && \
	  /tmp/edb -p secret /tmp/t_enc.edb -e "INSERT INTO e VALUES (1);" && \
	  if /tmp/edb -p wrong /tmp/t_enc.edb -e "SELECT * FROM e;" ; then echo "wrong password should fail"; exit 1; else echo "wrong password rejected OK"; fi
	@echo '{"status":"incomplete","note":"full mandatory suites still pending"}' > build/test-output/summary.json
	@echo "partial make test OK — full DoD NOT met"

check: test
	@echo "=== evidence matrix ===" && EDB=$(BUILD)/edb CHECK=$(BUILD)/edb-check bash tests/evidence/full_matrix.sh
	@echo "=== RG matrix present ===" && test -f docs/evidence/RG_MATRIX.md
	@echo "make check OK (partial gates — see RG_MATRIX.md)"
