#!/usr/bin/env bash
#
# mayhem/test.sh — RUN this repo's OWN functional test suite (already built by mayhem/build.sh).
# exit 0 = pass. EDIT per repo. PATCH-grade oracle: after an agent patches the source, the grader
# rebuilds (build.sh) then runs this. DELETE this file if the repo has no meaningful tests.
#
# IMPORTANT:
#  * Must assert BEHAVIOR/OUTPUT, not just exit status. The oracle has to check asserted values /
#    golden-output diffs / known-answer results — so a PATCH that "fixes" a bug by making the program
#    exit(0) (or any no-op) FAILS here. Running inputs and checking only "exit 0 / didn't crash" is
#    NOT a functional test (it's trivially reward-hackable) — use the project's real assertion suite.
#  * Do NOT build here — mayhem/build.sh already compiled the test suite (with the project's normal
#    flags). This script only RUNS the pre-built tests and reports counts. If the test runner is
#    missing, that's a build.sh bug — fail loudly rather than silently rebuilding.
#  * REQUIRED OUTPUT — a CTRF (https://ctrf.io) summary so Mayhem/the PATCH grader reads the counts:
#      - writes a CTRF JSON report to ${CTRF_REPORT:-$SRC/ctrf-report.json}, and
#      - prints a one-line `CTRF {...}` marker to stdout (same JSON, compact).
#    Only `results.summary` (with tests/passed/failed/pending/skipped/other) is required.
#    Use the emit_ctrf helper below; it computes tests = passed+failed+skipped and sets the exit
#    code (0 iff failed==0). Map your framework's output to passed/failed/skipped.
set -uo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH
: "${MAYHEM_JOBS:=$(nproc)}"   # build parallelism; env-overridable, falls back to nproc (use -j"$MAYHEM_JOBS")
cd "$SRC"

# emit_ctrf <tool> <passed> <failed> [skipped] [pending] [other]
# Writes a CTRF report (file + stdout `CTRF {...}` marker) and returns non-zero iff failed>0.
emit_ctrf() {
  local tool="$1" passed="$2" failed="$3" skipped="${4:-0}" pending="${5:-0}" other="${6:-0}"
  local tests=$(( passed + failed + skipped + pending + other ))
  cat > "${CTRF_REPORT:-$SRC/ctrf-report.json}" <<JSON
{
  "results": {
    "tool": { "name": "$tool" },
    "summary": {
      "tests": $tests,
      "passed": $passed,
      "failed": $failed,
      "pending": $pending,
      "skipped": $skipped,
      "other": $other
    }
  }
}
JSON
  printf 'CTRF {"results":{"tool":{"name":"%s"},"summary":{"tests":%d,"passed":%d,"failed":%d,"pending":%d,"skipped":%d,"other":%d}}}\n' \
    "$tool" "$tests" "$passed" "$failed" "$pending" "$skipped" "$other"
  [ "$failed" -eq 0 ]
}

# The suite is mayhem/selftest.c (built by build.sh with upstream's normal -O2 flags): known-answer
# checks of the config parser, BPS decoder and GLSL preset loader against mayhem/testdata/. Upstream
# ships no test suite of its own. It prints PASS/FAIL per check and a final
# "SELFTEST passed=<n> failed=<n>" line; a missing or unparsable summary counts as a failure.
BIN="$SRC/mayhem/build/zelda3_selftest"
if [ ! -x "$BIN" ]; then
  echo "test.sh: $BIN missing (mayhem/build.sh should have built it)" >&2
  emit_ctrf "zelda3-selftest" 0 1 0
  exit 1
fi
out="$("$BIN" "$SRC/mayhem/testdata" 2>&1)"; rc=$?
echo "$out"
summary="$(printf '%s\n' "$out" | grep -E '^SELFTEST passed=[0-9]+ failed=[0-9]+$' | tail -1)"
passed="$(printf '%s\n' "$out" | grep -c '^PASS ')"
failed="$(printf '%s\n' "$out" | grep -c '^FAIL ')"
if [ -z "$summary" ] || [ "$summary" != "SELFTEST passed=$passed failed=$failed" ] || [ "$passed" -eq 0 ]; then
  echo "test.sh: self-test produced no consistent summary (rc=$rc)" >&2
  failed=$((failed + 1))
elif [ "$rc" -ne 0 ] && [ "$failed" -eq 0 ]; then
  echo "test.sh: self-test exited $rc with no failed check" >&2
  failed=1
fi
emit_ctrf "zelda3-selftest" "$passed" "$failed" 0
