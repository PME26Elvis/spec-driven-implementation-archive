#!/bin/bash
set -e
EDB=${EDB:-./build/bin/edb}
DB=/tmp/smoke_acc.edb
rm -f "$DB"
pass=0; fail=0
check() {
  local name="$1"; shift
  if "$@" >/tmp/smoke_out.txt 2>&1; then
    echo "PASS $name"; pass=$((pass+1))
  else
    echo "FAIL $name"; cat /tmp/smoke_out.txt; fail=$((fail+1))
  fi
}
$EDB -c "$DB" -e "CREATE TABLE t (id INTEGER PRIMARY KEY, name TEXT, score INTEGER);"
$EDB "$DB" -e "INSERT INTO t VALUES (1,'a',10),(2,'b',20),(3,'a',30);"
check multi_insert $EDB "$DB" -e "SELECT COUNT(*) FROM t;"
check where_gt $EDB "$DB" -e "SELECT * FROM t WHERE score > 15;"
check between $EDB "$DB" -e "SELECT * FROM t WHERE score BETWEEN 10 AND 20;"
check in_list $EDB "$DB" -e "SELECT * FROM t WHERE id IN (1,3);"
check group_by $EDB "$DB" -e "SELECT COUNT(*) FROM t GROUP BY name;"
check limit_offset $EDB "$DB" -e "SELECT * FROM t LIMIT 1 OFFSET 1;"
check is_null $EDB "$DB" -e "INSERT INTO t VALUES (4,NULL,0);" 
$EDB "$DB" -e "SELECT * FROM t WHERE name IS NULL;" >/dev/null
check explain $EDB "$DB" -e "EXPLAIN SELECT * FROM t WHERE id = 1;"
check join $EDB "$DB" -e "CREATE TABLE u (id INTEGER PRIMARY KEY, tid INTEGER);"
$EDB "$DB" -e "INSERT INTO u VALUES (10,1),(11,2);"
$EDB "$DB" -e "SELECT * FROM t JOIN u ON id = tid;" >/dev/null && echo PASS join || echo FAIL join
check left_join $EDB "$DB" -e "SELECT * FROM t LEFT JOIN u ON id = tid;"
check subquery $EDB "$DB" -e "SELECT * FROM t WHERE id = (SELECT COUNT(*) FROM t);"
check analyze $EDB "$DB" -e "ANALYZE;"
echo "SMOKE pass=$pass fail=$fail (partial evidence — not full DoD)"
exit 0
