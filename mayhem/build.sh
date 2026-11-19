#!/usr/bin/env bash
#
# mayhem/build.sh — build this repo's fuzz harness(es). EDIT per repo.
#
# Runs inside the commit image (mayhem/Dockerfile) as `mayhem` in /mayhem. The base image
# (ghcr.io/savantenvs/base) already exports the build contract — use these, don't redefine:
#   CC, CXX             stock clang / clang++
#   LIB_FUZZING_ENGINE  -fsanitize=fuzzer   (link into each harness that has a LLVMFuzzer entry)
#   SANITIZER_FLAGS     -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer
#                       (ASan + UBSan, both set to HALT — so Mayhem catches memory AND UB defects)
#   DEBUG_FLAGS         -g -gdwarf-3   (DWARF debug info — always on for fuzz/standalone builds,
#                       independent of the sanitizer off-switch; DWARF version must be < 4)
#   RUST_DEBUG_FLAGS    -C debuginfo=2 -C force-frame-pointers=yes -C llvm-args=-gdwarf-3
#                       (thread through RUSTFLAGS on every cargo-fuzz build)
#   GO_DEBUG_FLAGS      -gcflags=all=-N -l
#                       (thread through go build / go-fuzz-build so the linked ELF keeps symbols)
#   SRC                 /mayhem (the repo source)
#
# Contract: build EVERYTHING here — one runnable binary per fuzz harness, AND the project's test
# suite (so mayhem/test.sh only has to RUN it, never compile). Keep it ADDITIVE (build upstream as
# upstream documents; don't edit upstream files). IMPORTANT: build the PROJECT ITSELF with
# $SANITIZER_FLAGS and $DEBUG_FLAGS (not just the harness) so the fuzzed code is instrumented
# AND carries DWARF < 4 symbols — otherwise ASan/UBSan only see the harness, not the library
# you're trying to find bugs in, and backtraces won't resolve project source lines. Build the TEST
# suite with the project's NORMAL flags (a clean, independent build) so test.sh stays an honest
# functional oracle and won't false-fail on benign UB. Leave the test binary/runner where test.sh
# expects it.
set -euo pipefail

# clang rejects SOURCE_DATE_EPOCH='' (empty) — it must be unset or a valid integer.
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH

# Build knobs come from the ENVIRONMENT (overridable), with sane defaults — no if-statements,
# just parameter-expansion fallbacks. Default sanitizers are the base's ASan+UBSan (halting); override
# per build via the Dockerfile's `--build-arg SANITIZER_FLAGS="..."`.
# NB: SANITIZER_FLAGS uses `=` (no colon) on purpose — `=` only fills when the var is UNSET, so an
# explicit EMPTY value (`--build-arg SANITIZER_FLAGS=`) is honored and builds with NO sanitizers
# (useful when you want the program's natural crash / full backtrace, not an ASan report). The other
# knobs use `:=` (default on empty too). MAYHEM_JOBS sets build parallelism (falls back to nproc).
: "${SANITIZER_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer}"
# DEBUG_FLAGS carries DWARF debug info INDEPENDENTLY of the sanitizer off-switch (so an empty
# SANITIZER_FLAGS still yields DWARF symbols). DWARF MUST be < 4 (Mayhem triage can't read >=4); clang-19's
# plain `-g` emits DWARF-5, so `-gdwarf-3` is explicit. Apply $DEBUG_FLAGS to the fuzz/harness/standalone
# builds (NOT the test/oracle build). Rust/Go carry the same intent via their language flags below.
: "${DEBUG_FLAGS:=-g -gdwarf-3}"
: "${RUST_DEBUG_FLAGS:=-C debuginfo=2 -C force-frame-pointers=yes -C llvm-args=-gdwarf-3}"
: "${GO_DEBUG_FLAGS:=-gcflags=all=-N -l}"
: "${CC:=clang}" ; : "${CXX:=clang++}" ; : "${LIB_FUZZING_ENGINE:=-fsanitize=fuzzer}"
: "${MAYHEM_JOBS:=$(nproc)}"
# COVERAGE_FLAGS: empty by default → no effect on the normal oracle build. Set it via the Dockerfile's
# `--build-arg COVERAGE_FLAGS="-fprofile-instr-generate -fcoverage-mapping"` to instrument the TEST
# build for source-coverage measurement (how much of the project the test suite actually exercises —
# a quality signal for the oracle; complements the anti-reward-hack sabotage check). APPEND it to the
# test build's compile+link flags in step 3 (NOT the fuzz build); after `test.sh` runs, merge with
# `llvm-profdata` and report with `llvm-cov`. Empty value is honored (`=`, not `:=`).
: "${COVERAGE_FLAGS=}"
export SANITIZER_FLAGS DEBUG_FLAGS RUST_DEBUG_FLAGS GO_DEBUG_FLAGS CC CXX LIB_FUZZING_ENGINE MAYHEM_JOBS COVERAGE_FLAGS

cd "$SRC"

# zelda3 is a plain-Makefile SDL2 game. Its untrusted-input parsers (config .ini, BPS patch, GLSL
# shader presets + PNG textures) are compiled here from upstream's own sources, with the same defines
# upstream's Makefile uses, into mayhem/build/ (gitignored, so `git clean -ffdX` resets it). Every
# project TU is instrumented: sanitizers + coverage (-fsanitize=fuzzer-no-link) + DWARF 3.
B="$SRC/mayhem/build"
mkdir -p "$B/fuzz" "$B/test"
SDL_CFLAGS="$(sdl2-config --cflags)"
SDL_LIBS="$(sdl2-config --libs)"
PROJ_DEFS="-I$SRC $SDL_CFLAGS -DSYSTEM_VOLUME_MIXER_AVAILABLE=0"
H="$SRC/mayhem/harnesses"
PROJ_SRCS="src/util.c src/glsl_shader.c third_party/gl_core/gl_core_3_1.c"

# 1) Project sources + shared harness support, sanitized and coverage-instrumented.
# UBSan relaxations (ASan and every other UBSan check stay on and halting):
#  - alignment, everywhere: zelda3 reads little-endian fields with unaligned casts on purpose
#    (e.g. ApplyBps `*(uint32 *)(bps_end)` at src/util.c:229), benign on the x86/ARM targets it
#    supports; left on, it fires on ~3 of every 4 BPS inputs before the decoder runs.
#  - pointer-overflow, src/glsl_shader.c only: GlslShader_Destroy computes `gs->pass + 1` while
#    gs->pass is still NULL (src/glsl_shader.c:461), which every preset that fails before
#    `shaders = N` reaches; the loop then runs zero times.
FZ="$SANITIZER_FLAGS -fno-sanitize=alignment $DEBUG_FLAGS -fsanitize=fuzzer-no-link $PROJ_DEFS"
FOBJS=""
for f in $PROJ_SRCS; do
  o="$B/fuzz/$(basename "${f%.c}").o"
  extra=""
  [ "$f" = src/glsl_shader.c ] && extra="-fno-sanitize=pointer-overflow"
  $CC $FZ $extra -c "$SRC/$f" -o "$o"
  FOBJS="$FOBJS $o"
done
$CC $FZ -c "$H/die_abort.c" -o "$B/fuzz/die_abort.o"
$CC $FZ -c "$H/gl_stubs.c" -o "$B/fuzz/gl_stubs.o"
$CC $FZ -c "$SRC/src/config.c" -o "$B/fuzz/config.o"
$CXX $SANITIZER_FLAGS $DEBUG_FLAGS -c "$SRC/mayhem/lsan_off.cc" -o "$B/fuzz/lsan_off.o"
$CC $SANITIZER_FLAGS $DEBUG_FLAGS -c "$STANDALONE_FUZZ_MAIN" -o "$B/fuzz/standalone_main.o"
SUPPORT="$B/fuzz/die_abort.o $B/fuzz/gl_stubs.o $B/fuzz/lsan_off.o"

# The rest of the game (everything but src/main.c, which opens the SDL window/audio device and loads
# the ROM-derived zelda3_assets.dat; harnesses/zelda_main_stubs.c provides its few shared symbols),
# the SNES emulator and the bundled Opus decoder: fuzz_msu compiles src/audio.c into itself and
# links the code audio.c calls from here.
GOBJS=""
for f in $(cd "$SRC" && ls src/*.c snes/*.c | grep -v -x -e src/main.c -e src/audio.c -e src/config.c \
             -e src/util.c -e src/glsl_shader.c) third_party/opus-1.3.1-stripped/opus_decoder_amalgam.c; do
  o="$B/fuzz/game_$(echo "${f%.c}" | tr / _).o"
  $CC $FZ -c "$SRC/$f" -o "$o"
  GOBJS="$GOBJS $o"
done
$CC $FZ -c "$H/zelda_main_stubs.c" -o "$B/fuzz/zelda_main_stubs.o"

# 2) Harnesses: libFuzzer binary + run-once standalone reproducer each. fuzz_config compiles
#    src/config.c into itself (it resets config.c's file-static maps between inputs).
for h in fuzz_config fuzz_bps fuzz_glsl_preset fuzz_msu; do
  objs="$FOBJS"
  [ "$h" = fuzz_config ] || objs="$objs $B/fuzz/config.o"
  [ "$h" = fuzz_msu ] && objs="$objs $GOBJS $B/fuzz/zelda_main_stubs.o"
  $CC $FZ -c "$H/$h.c" -o "$B/fuzz/$h.o"
  $CXX $SANITIZER_FLAGS $DEBUG_FLAGS $LIB_FUZZING_ENGINE "$B/fuzz/$h.o" $objs $SUPPORT \
      $SDL_LIBS -lm -o "$B/$h"
  $CXX $SANITIZER_FLAGS $DEBUG_FLAGS "$B/fuzz/standalone_main.o" "$B/fuzz/$h.o" $objs $SUPPORT \
      $SDL_LIBS -lm -o "$B/$h-standalone"
done

# 3) Functional self-test (known-answer checks of the config parser, BPS decoder and shader-preset
#    loader), built separately with upstream's NORMAL flags (-O2, no sanitizers).
TF="-O2 $COVERAGE_FLAGS $PROJ_DEFS"
TOBJS=""
for f in $PROJ_SRCS src/config.c; do
  o="$B/test/$(basename "${f%.c}").o"
  $CC $TF -c "$SRC/$f" -o "$o"
  TOBJS="$TOBJS $o"
done
$CC $TF -c "$H/die_abort.c" -o "$B/test/die_abort.o"
$CC $TF -c "$H/gl_stubs.c" -o "$B/test/gl_stubs.o"
$CC $TF "$SRC/mayhem/selftest.c" $TOBJS "$B/test/die_abort.o" "$B/test/gl_stubs.o" \
    $SDL_LIBS -lm $COVERAGE_FLAGS -o "$B/zelda3_selftest"

echo "built: $(ls "$B" | grep -v -e '^fuzz$' -e '^test$' | tr '\n' ' ')"
