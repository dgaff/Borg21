#!/bin/sh
#=============================================================================
# End-to-end test for the 1995 16-bit files.
#
# legacy16.cpp already pins the decoded field values; this drives the same
# fixtures through the `borg legacy` subcommands, which is the path a person
# actually uses, and checks the three claims that only the whole pipeline can
# make:
#
#   * a converted snapshot loads through plain `borg info` and reports the 1995
#     numbers
#   * converting a file twice gives byte-identical output, so the conversion is
#     deterministic and has no dependence on where it runs
#   * replaying the 1995 configuration reproduces the 1995 trajectory
#
# The fixtures are unmodified 1995 artifacts from Research/BBA -- see the header
# of tests/legacy16.cpp.
#
# Usage: run_legacy.sh <path to borg> <fixture dir> <scratch dir>
#=============================================================================

set -e

BORG="$1"
FIX="$2"
WORK="$3"

if [ -z "$BORG" ] || [ -z "$FIX" ] || [ -z "$WORK" ]; then
  echo "usage: run_legacy.sh <borg> <fixturedir> <workdir>" >&2
  exit 2
fi

SIM="$FIX/1.SIM"
POS="$FIX/1_1.P"
RUL="$FIX/BBA.RUL"

fail() { echo "FAIL  $1" >&2; exit 1; }
pass() { echo "PASS  $1"; }

echo
echo "Borg 16-bit legacy end-to-end"
echo "============================="
echo

rm -rf "$WORK"
mkdir -p "$WORK"

#-----------------------------------------------------------------------------
# info -- the decode, through the CLI.
#-----------------------------------------------------------------------------
"$BORG" legacy info "$SIM" > "$WORK/sim.txt" || fail "legacy info failed on the .SIM"
grep -q 'DLCS (NETWORK) layout'    "$WORK/sim.txt" || fail "layout not reported as DLCS"
grep -Eq '^ *seed +100$'           "$WORK/sim.txt" || fail "seed not reported as 100"
grep -Eq '^ *global clock +23$'    "$WORK/sim.txt" || fail "global clock not reported as 23"
grep -Eq '^ *type +CONCAVE_OBST$'  "$WORK/sim.txt" || fail "env type not reported as CONCAVE_OBST"
grep -Eq '^ *obstacles +401$'      "$WORK/sim.txt" || fail "obstacle count not reported as 401"
pass "legacy info decodes the 1995 .SIM"

"$BORG" legacy info "$POS" > "$WORK/pos.txt" || fail "legacy info failed on the .P"
grep -q '16-bit .P position dump' "$WORK/pos.txt" || fail ".P not recognised as a .P"
grep -Eq '^ *agents +1$'          "$WORK/pos.txt" || fail ".P agent count wrong"
pass "legacy info decodes the 1995 .P, told apart from a .SIM by content"

# --rules prints the learned rule list.  BBA.RUL's first rule is the one the
# 1995 operator gave strength 100, and it is still rule 0 in the snapshot.
"$BORG" legacy info "$SIM" --rules > "$WORK/rules.txt" || fail "legacy info --rules failed"
grep -Eq '^ +0 +100000000 +001000001 +21\.5625$' "$WORK/rules.txt" \
  || fail "rule 0 is not BBA.RUL's first rule with the strength the run left it"
grep -c '^    [0-9 ][0-9 ][0-9]  [01#]' "$WORK/rules.txt" > "$WORK/rulecount"
[ "$(cat "$WORK/rulecount")" = "32" ] \
  || fail "expected 32 rule lines, got $(cat "$WORK/rulecount")"
pass "legacy info --rules prints all 32 classifiers"

#-----------------------------------------------------------------------------
# check -- the .SIM and the .P must agree.  Two 1995 files, two code paths.
#-----------------------------------------------------------------------------
"$BORG" legacy check "$SIM" "$POS" > "$WORK/check.txt" || fail "legacy check reported a mismatch"
grep -q 'AGREE' "$WORK/check.txt" || fail "legacy check did not report AGREE"
grep -q '24 positions checked' "$WORK/check.txt" || fail "legacy check compared the wrong count"
pass "the .SIM and .P hold the same 24 positions, bit-for-bit"

# And it must NOT agree when the pair does not belong together.  The .SIM read as
# a .P is the cheapest mismatch to construct and is rejected outright.
if "$BORG" legacy check "$SIM" "$SIM" > "$WORK/badcheck.txt" 2>&1; then
  fail "legacy check accepted a .SIM in place of the .P"
fi
pass "legacy check refuses a .SIM offered as the .P"

#-----------------------------------------------------------------------------
# convert -- and then load the result with the ordinary, 1995-unaware loader.
#-----------------------------------------------------------------------------
"$BORG" legacy convert "$SIM" "$WORK/1.sim" > "$WORK/conv.txt" \
  || fail "legacy convert failed"
[ -f "$WORK/1.sim" ] || fail "legacy convert wrote no output"
pass "legacy convert wrote a modern .sim"

"$BORG" info "$WORK/1.sim" > "$WORK/modern.txt" \
  || fail "the converted .sim did not load through plain borg info"
grep -Eq '^ *agents +1 *$'      "$WORK/modern.txt" || fail "converted .sim: agent count wrong"
grep -Eq '^ *seed +100 *$'      "$WORK/modern.txt" || fail "converted .sim: seed wrong"
grep -Eq '^ *maxCount +500 *$'  "$WORK/modern.txt" || fail "converted .sim: maxCount wrong"
grep -Eq '^ *global clock +23 *$' "$WORK/modern.txt" || fail "converted .sim: clock wrong"
pass "the converted .sim loads through plain borg info and reports the 1995 values"

# Deterministic: converting again must produce identical bytes.
"$BORG" legacy convert "$SIM" "$WORK/1-again.sim" > /dev/null || fail "second convert failed"
cmp -s "$WORK/1.sim" "$WORK/1-again.sim" \
  || fail "converting the same .SIM twice produced different bytes"
pass "conversion is deterministic"

# A .P converts too, and a .P is the same shape in either build.
"$BORG" legacy convert "$POS" "$WORK/1_1.p" > /dev/null || fail "legacy convert failed on the .P"
[ -s "$WORK/1_1.p" ] || fail "converted .P is empty"
pass "legacy convert handles a .P"

#-----------------------------------------------------------------------------
# csv -- what the 1995 .P files existed for.
#-----------------------------------------------------------------------------
"$BORG" legacy csv "$POS" -o "$WORK/path.csv" > /dev/null || fail "legacy csv failed"
head -1 "$WORK/path.csv" | grep -q '^agent,step,x,y,heading$' \
  || fail "csv header is not the documented column list"
LINES=$(wc -l < "$WORK/path.csv" | tr -d ' ')
[ "$LINES" = "25" ] || fail "csv has $LINES lines, expected 25 (1 header + 24 positions)"
sed -n '2p' "$WORK/path.csv" | grep -q '^1,0,0.000000,0.000000,1.000000$' \
  || fail "csv first row is not the recorded start position"
pass "legacy csv writes 24 positions with the 1995 start point intact"

# The .SIM must produce the same CSV as the .P, since they hold the same data.
"$BORG" legacy csv "$SIM" -o "$WORK/path-sim.csv" > /dev/null || fail "legacy csv failed on the .SIM"
cmp -s "$WORK/path.csv" "$WORK/path-sim.csv" \
  || fail ".SIM and .P produced different CSV"
pass "the .SIM and the .P export identical CSV"

#-----------------------------------------------------------------------------
# replay -- the whole point.  Re-run the 1995 configuration and compare.
#-----------------------------------------------------------------------------
"$BORG" legacy replay "$SIM" -r "$RUL" > "$WORK/replay.txt" \
  || { cat "$WORK/replay.txt"; fail "replay of 1.SIM did not reproduce the 1995 run"; }
grep -q 'all 24 positions and headings IDENTICAL' "$WORK/replay.txt" \
  || fail "replay did not report an identical trajectory"
grep -q 'REPLAY REPRODUCED THE 1995 RUN EXACTLY' "$WORK/replay.txt" \
  || fail "replay verdict was not an exact reproduction"
grep -q 'all 32 rules match' "$WORK/replay.txt" \
  || fail "replay did not confirm BBA.RUL matches the snapshot's rules"
grep -q 'random draws: 1995 535, here 535' "$WORK/replay.txt" \
  || fail "replay did not reproduce the 1995 random-draw count"
pass "replay reproduces 1.SIM exactly: trajectory, statistics and draw count"

# Replaying against the wrong rule file must be called out rather than quietly
# producing a divergence with no explanation.
if "$BORG" legacy replay "$SIM" -r /dev/null > "$WORK/badreplay.txt" 2>&1; then
  fail "replay with an empty rule file reported success"
fi
pass "replay against the wrong rules fails rather than reporting a match"

#-----------------------------------------------------------------------------
# rules -- .RUL needs no conversion, only the arithmetic loadRules() skips.
#-----------------------------------------------------------------------------
"$BORG" legacy rules "$RUL" > "$WORK/rul.txt" || fail "legacy rules failed"
grep -Eq 'header lines +1$'       "$WORK/rul.txt" || fail "BBA.RUL header count wrong"
grep -Eq 'rule lines +33$'        "$WORK/rul.txt" || fail "BBA.RUL rule count wrong"
grep -Eq 'rule width +9 alleles$' "$WORK/rul.txt" || fail "BBA.RUL rule width wrong"
pass "legacy rules counts BBA.RUL's 1 header and 33 rules"

#-----------------------------------------------------------------------------
# The converted .P must load back into the live simulator.  This is what makes
# the conversion worth anything: the 1995 trajectory becomes live state.
#-----------------------------------------------------------------------------
"$BORG" pos "$WORK/1_1.p" > "$WORK/posload.txt" \
  || fail "the converted .P did not load through borg pos"
grep -Eq '^ *agents +1$'         "$WORK/posload.txt" || fail "loaded .P: agent count wrong"
grep -Eq '^ *maxCount +500 ' "$WORK/posload.txt" \
  || fail "loaded .P: maxCount was not derived as 500"
grep -q '24 positions'           "$WORK/posload.txt" || fail "loaded .P: position count wrong"
grep -q 'end   (216.98, 51.00)'  "$WORK/posload.txt" \
  || fail "loaded .P does not end where the 1995 run did"
pass "the converted .P loads into the live simulator with the 1995 trajectory"

# And a .P this build writes must load too, so the format is not write-only.
"$BORG" run -q -a 1 -m 60 -p "$WORK/native.P" || fail "borg run -p failed"
"$BORG" pos "$WORK/native.P" > "$WORK/native.txt" || fail "borg pos could not read its own .P"
grep -q '61 positions' "$WORK/native.txt" \
  || fail "a 60-tick run did not write 61 positions"
pass "a .P written by this build round-trips through borg pos"

# Cross-format confusion must be caught in BOTH directions.  Both a 1995 .SIM and
# a native one begin with the same "2.1\0" stamp, so the version string is no help
# at all -- the guards look at the first integer instead.
if "$BORG" info "$SIM" > "$WORK/wrongway.txt" 2>&1; then
  fail "plain borg info accepted a 1995 16-bit .SIM"
fi
grep -q 'borg legacy convert' "$WORK/wrongway.txt" \
  || fail "borg info rejected the 1995 .SIM without pointing at legacy convert"
pass "plain borg info refuses a 1995 .SIM and names the conversion command"

if "$BORG" legacy info "$WORK/1.sim" > "$WORK/otherway.txt" 2>&1; then
  fail "legacy info accepted a native .sim"
fi
grep -q 'use "borg info" instead' "$WORK/otherway.txt" \
  || fail "legacy info rejected the native .sim without pointing back at borg info"
pass "legacy info refuses a native .sim and names borg info"

#-----------------------------------------------------------------------------
# Damaged input.  Truncation is only caught by the exact-EOF rule.
#-----------------------------------------------------------------------------
dd if="$SIM" of="$WORK/trunc.SIM" bs=1 count=6000 2>/dev/null
if "$BORG" legacy info "$WORK/trunc.SIM" > "$WORK/trunc.txt" 2>&1; then
  fail "a truncated .SIM was accepted"
fi
grep -q 'not a readable 1995 Borg 2.1 .SIM' "$WORK/trunc.txt" \
  || fail "truncated .SIM was rejected, but without saying why"
pass "a truncated .SIM is refused, with a reason"

echo
echo "============================="
echo "All checks passed."
echo
