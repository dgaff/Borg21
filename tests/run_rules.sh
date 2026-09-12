#!/bin/sh
#=============================================================================
# Cross-variant .RUL comparison.
#
# rules_roundtrip.cpp runs once against each variant of borgcore and leaves the
# .RUL it produced from the 1995 BBA.RUL fixture.  This compares the two.
#
# That comparison is the sharpest statement of the fix.  taskEnvironment::
# saveRules() used to write its "; Agent N" header only #ifdef NETWORK, so the
# two variants produced DIFFERENT files from identical state, and the plain-LCS
# one could not reload its own output -- loadRules() skips a line per agent in
# both variants, so the plain build lost its first rule every time.
#
# Now both variants write the same header, because reset() assigns agent IDs as
# i+1 and the plain branch formats exactly that.  So the files must be identical,
# and a .RUL must be interchangeable between the two builds -- unlike a .sim,
# which the note at the top of CFS.H correctly says is not.
#
# Usage: run_rules.sh <dlcs .RUL> <plain .RUL> <1995 fixture>
#=============================================================================

set -e

DLCS="$1"
PLAIN="$2"
FIX="$3"

if [ -z "$DLCS" ] || [ -z "$PLAIN" ] || [ -z "$FIX" ]; then
  echo "usage: run_rules.sh <dlcs.RUL> <plain.RUL> <fixture.RUL>" >&2
  exit 2
fi

fail() { echo "FAIL  $1" >&2; exit 1; }
pass() { echo "PASS  $1"; }

echo
echo "Borg .RUL cross-variant comparison"
echo "=================================="
echo

[ -f "$DLCS" ]  || fail "the DLCS build left no .RUL at $DLCS"
[ -f "$PLAIN" ] || fail "the plain-LCS build left no .RUL at $PLAIN"
pass "both variants produced a .RUL"

#-----------------------------------------------------------------------------
# The heart of it.
#-----------------------------------------------------------------------------
if ! cmp -s "$DLCS" "$PLAIN"; then
  echo "--- DLCS build wrote ---"   >&2
  head -4 "$DLCS"                   >&2
  echo "--- plain-LCS build wrote ---" >&2
  head -4 "$PLAIN"                  >&2
  fail "the two variants wrote different .RUL files"
fi
pass "the DLCS and plain-LCS builds write byte-identical .RUL files"

#-----------------------------------------------------------------------------
# Both must carry the header, numbered from 1, since that is the line loadRules
# consumes.  A missing header is exactly the old plain-LCS bug.
#-----------------------------------------------------------------------------
for f in "$DLCS" "$PLAIN"; do
  head -1 "$f" | grep -q '^; Agent 1$' \
    || fail "$f does not begin with the '; Agent 1' header loadRules skips"
done
pass "both files begin with the header line loadRules expects"

#-----------------------------------------------------------------------------
# And both must reproduce the 1995 fixture's rules.  BBA.RUL's header is
# followed by 33 rule lines for a 32-slot list, so compare the first 32 --
# the 33rd was never read in 1995 either.
#-----------------------------------------------------------------------------
for f in "$DLCS" "$PLAIN"; do
  LINES=$(wc -l < "$f" | tr -d ' ')
  [ "$LINES" = "33" ] || fail "$f has $LINES lines, expected 33 (1 header + 32 rules)"
done
pass "both files hold one header and 32 rules"

# Compare rule by rule on VALUES, not on text.  BBA.RUL was hand-edited and
# spells its strengths four ways ("10.000000", "10.0", "0.000000", "1"), while
# saveRules always writes "%lf" -- so the strength is normalised on both sides
# before comparing.  The condition and action are compared literally.
# CR is stripped so the fixture's line endings do not matter either.
norm() { tr -d '\r' < "$1" | sed -n '2,33p' | awk '{printf "%s %s %.6f\n",$1,$2,$3}'; }
norm "$FIX"  > "$PLAIN.expected"
norm "$DLCS" > "$PLAIN.got"
cmp -s "$PLAIN.expected" "$PLAIN.got" \
  || { diff "$PLAIN.expected" "$PLAIN.got" | head -8 >&2
       fail "the saved rules do not match the 1995 BBA.RUL they were loaded from"; }
pass "all 32 rules match the 1995 BBA.RUL, condition, action and strength"

rm -f "$PLAIN.expected" "$PLAIN.got"

echo
echo "=================================="
echo "All checks passed."
echo
