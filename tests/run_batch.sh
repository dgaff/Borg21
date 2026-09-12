#!/bin/sh
#=============================================================================
# End-to-end batch test.
#
# Runs the real 1995 SAMPLE.B through `borg batch` and checks the artifacts it
# produces, because the unit test in batch_format.cpp only pins the row
# formatter -- it never parses a .b file or finishes a simulation.
#
# SAMPLE.B is the genuine article, copied unmodified from BORG/Source: CRLF line
# endings, a 265-character comment line, and one spec line of 42 columns that
# runs 2 agents on the standard field with seed 100.
#
# Usage: run_batch.sh <path to borg> <path to sample.b> <scratch dir>
#=============================================================================

set -e

BORG="$1"
FIXTURE="$2"
WORK="$3"

if [ -z "$BORG" ] || [ -z "$FIXTURE" ] || [ -z "$WORK" ]; then
  echo "usage: run_batch.sh <borg> <sample.b> <workdir>" >&2
  exit 2
fi

fail() { echo "FAIL  $1" >&2; exit 1; }
pass() { echo "PASS  $1"; }

echo
echo "Borg batch end-to-end"
echo "====================="
echo

rm -rf "$WORK"
mkdir -p "$WORK"
cp "$FIXTURE" "$WORK/sample.b"

# -C makes the .sim files and the .O land in the scratch directory rather than
# wherever ctest happens to be standing.
"$BORG" batch "$WORK/sample.b" -C "$WORK"

OUT="$WORK/sample.O"

#-----------------------------------------------------------------------------
[ -f "$OUT" ] || fail "no .O file was written"
pass ".O file written"

# SAMPLE.B holds one simulation starting at number 1, so exactly 1.sim.
[ -f "$WORK/1.sim" ] || fail "1.sim was not written"
pass "1.sim written"
[ -f "$WORK/2.sim" ] && fail "2.sim written, but SAMPLE.B declares only one simulation"
pass "no extra .sim files"

#-----------------------------------------------------------------------------
# Header: 2 caption lines + 1 blank.  Then one data row per agent, and SAMPLE.B
# asks for 2 agents.
#-----------------------------------------------------------------------------
TOTAL=$(wc -l < "$OUT" | tr -d ' ')
[ "$TOTAL" = "5" ] || fail ".O has $TOTAL lines, expected 5 (3 header + 2 agents)"
pass ".O has 3 header lines and 2 data rows"

sed -n '2p' "$OUT" | grep -q '^Sim #  Agent  Seed  Iterations' \
  || fail ".O caption line does not match the 1995 text"
pass ".O caption line matches 1995"

#-----------------------------------------------------------------------------
# Check the fixed columns by byte offset, which is how the MATLAB and Excel
# post-processing in Research/ reads this table.
#
#   bytes  1-4   simulation number
#   bytes  8-11  agent
#   bytes 15-18  seed
#   bytes 24-27  iterations
#
# Deliberately NOT asserting a total row width.  Every field is "%4d" and a
# printf width is a minimum, so a count above 9999 pushes the later columns
# right -- and SAMPLE.B's spec line produces around 10,000 mutations, so these
# rows really are wider than the ones in Research/BBA/1_.O.  The four columns
# above cannot shift, because everything that can overflow sits to their right.
#-----------------------------------------------------------------------------
for n in 4 5; do
  SIM=$(sed -n "${n}p" "$OUT" | cut -c1-4   | tr -d ' ')
  SEED=$(sed -n "${n}p" "$OUT" | cut -c15-18 | tr -d ' ')
  ITER=$(sed -n "${n}p" "$OUT" | cut -c24-27 | tr -d ' ')
  [ "$SIM" = "1" ]    || fail "row $n simulation column reads '$SIM', expected 1"
  [ "$SEED" = "100" ] || fail "row $n seed column reads '$SEED', expected 100"
  # SAMPLE.B sets max count to 4000 and the agents do not reach the goal, so
  # each run ends on the maxCount test at CFS.CPP:197.
  [ "$ITER" = "4000" ] || fail "row $n iterations column reads '$ITER', expected 4000"
done
pass "simulation, seed and iteration columns are correct at their byte offsets"

A1=$(sed -n '4p' "$OUT" | cut -c8-11 | tr -d ' ')
A2=$(sed -n '5p' "$OUT" | cut -c8-11 | tr -d ' ')
[ "$A1" = "1" ] || fail "first row agent column reads '$A1', expected 1"
[ "$A2" = "2" ] || fail "second row agent column reads '$A2', expected 2"
pass "agent column reads 1 then 2, one row per agent"

#-----------------------------------------------------------------------------
# The snapshot must load back.
#-----------------------------------------------------------------------------
"$BORG" info "$WORK/1.sim" > "$WORK/info.txt" || fail "borg info could not read 1.sim"
grep -Eq '^ *agents +2 *$' "$WORK/info.txt" \
  || fail "1.sim does not report 2 agents"
grep -Eq '^ *seed +100 *$' "$WORK/info.txt" \
  || fail "1.sim does not report seed 100"
pass "1.sim round-trips through borg info"

#-----------------------------------------------------------------------------
# A malformed script must be rejected, not run with stale settings.  Drop the
# last column from the spec line.
#-----------------------------------------------------------------------------
awk 'BEGIN{OFS=" "} /^[^;*]/ && NF>40 { NF=NF-1 } { print }' "$WORK/sample.b" > "$WORK/short.b"
if "$BORG" batch "$WORK/short.b" -C "$WORK" -q 2>"$WORK/short.err"; then
  fail "a spec line with 41 columns was accepted"
fi
grep -q "matched 41" "$WORK/short.err" \
  || fail "short spec line was rejected, but not with a column-count diagnostic"
pass "a 41-column spec line is rejected with a column count"

echo
echo "====================="
echo "All checks passed."
echo
