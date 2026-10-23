#!/usr/bin/env bash
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$HERE"
BUILD_DIR="${BUILD_DIR:-build}"
HOST="$BUILD_DIR/lmspace"
UNAME_S="$(uname -s)"
case "$UNAME_S" in MINGW*|MSYS*|CYGWIN*) HOST="$HOST.exe";; esac
WORK="$(mktemp -d "${TMPDIR:-/tmp}/lmspace-test-XXXXXX")"
trap 'rm -rf "$WORK"' EXIT
# Native Windows tools (node, the host) cannot address git-bash /tmp or /d/a paths.
ROOT="$HERE"
case "$UNAME_S" in MINGW*|MSYS*|CYGWIN*) WORK="$(cd "$WORK" && pwd -W | tr '\134' '/')"; ROOT="$(pwd -W | tr '\134' '/')";; esac
pass=0
ok(){ pass=$((pass+1)); echo "ok $pass - $1"; }
fail(){ echo "FAIL: $*" >&2; exit 1; }
sha(){ if command -v sha256sum >/dev/null 2>&1; then sha256sum "$1" | cut -c1-64; else shasum -a 256 "$1" | cut -c1-64; fi; }

# lmscript resolves cli="lmspace" on PATH first, so programs bind to the binary under test.
PATH="$(cd "$BUILD_DIR" 2>/dev/null && pwd || echo "$HERE/$BUILD_DIR"):$PATH"
export PATH
LMSCRIPT="$(scripts/select-compiler.sh)"
scripts/sync-sdk.sh --verify >/dev/null || fail "SDK header pin mismatch"
ok "SDK header matches sdk.lock"
scripts/sync-deps.sh --verify >/dev/null || fail "third_party pin mismatch"
ok "third_party matches deps.lock"

GEN=""
if [ ! -f "$BUILD_DIR/CMakeCache.txt" ] && command -v ninja >/dev/null 2>&1; then GEN="-G Ninja"; fi
# shellcheck disable=SC2086
cmake -S . -B "$BUILD_DIR" $GEN -DCMAKE_BUILD_TYPE=Release >/dev/null
cmake --build "$BUILD_DIR" --parallel "${BUILD_JOBS:-4}" >"$WORK/build.log" 2>&1 || { tail -40 "$WORK/build.log" >&2; fail "build"; }
ok "build"
ctest --test-dir "$BUILD_DIR" --output-on-failure || fail "native physics tests"
ok "native physics regression tests"
# Produce an original user-kernel fixture at runtime for the DSL ephemeris test.
"$BUILD_DIR/spice_tests" "build/synthetic-host.bsp" --keep >/dev/null || fail "SPK runtime fixture"

want_version="lmspace $(sed -n 's/^project(lmspace VERSION \([0-9.]*\).*/\1/p' CMakeLists.txt)"
[ "$("$HOST" --version)" = "$want_version" ] || fail "--version is not exactly '$want_version'"
ok "--version prints exactly '$want_version'"
python3 scripts/gen-schema.py >/dev/null && git diff --quiet -- schema/lmspace.dsl.json 2>/dev/null || [ ! -e .git ] || fail "schema/lmspace.dsl.json is stale: run scripts/gen-schema.py"
ok "schema matches the registrations in cpp/main.cpp"
"$HOST" --dsl-schema --json >"$WORK/schema.json" || fail "embedded schema print"
cmp -s "$WORK/schema.json" schema/lmspace.dsl.json || fail "embedded schema differs from schema/lmspace.dsl.json"
ok "--dsl-schema --json prints byte-identical embedded schema bytes"
diff -u <("$LMSCRIPT" --dsl-signatures schema/lmspace.dsl.json) <("$HOST" --dsl-signatures) || fail "host/compiler signatures differ"
ok "host registry matches compiler signatures"
"$HOST" --notices | grep -q "NAIF" || fail "--notices missing third-party text"
"$HOST" --notices | grep -q "Lucent Technologies" || fail "--notices missing the libf2c notice"
ok "--notices prints the third-party license texts"

case "$UNAME_S" in
  Linux) if ldd "$HOST" >/dev/null 2>&1; then fail "executable is dynamically linked"; fi; ok "executable is fully static (no dynamic loader)";;
  Darwin) bad="$(otool -L "$HOST" | tail -n +2 | grep -v -E '/usr/lib/(libSystem|libc\+\+|libresolv)' || true)"
          [ -z "$bad" ] || fail "non-system dylib dependencies: $bad"; ok "only system libraries linked";;
  MINGW*|MSYS*|CYGWIN*)
    if command -v objdump >/dev/null 2>&1; then
      bad="$(objdump -p "$HOST" | grep 'DLL Name' | grep -i -E 'libstdc|libgcc|winpthread|libgomp' || true)"
      [ -z "$bad" ] || fail "MinGW runtime DLLs are imported: $bad"
      ok "no MinGW runtime DLL imports"
    else ok "static-link check skipped (no objdump)"; fi;;
  *) ok "static-link check skipped on $UNAME_S";;
esac

# Programs under tests/: compile to bytecode and run (they assert their own numbers).
mkdir -p build
n=0
for src in tests/*.lms; do
  name="$(basename "$src" .lms)"
  "$LMSCRIPT" "$src" "build/$name.dslb" >"$WORK/$name.log" 2>&1 || { cat "$WORK/$name.log" >&2; fail "$name compile"; }
  "$HOST" --dsl "build/$name.dslb" >"$WORK/$name.out" 2>"$WORK/$name.err" || { cat "$WORK/$name.out" "$WORK/$name.err" >&2; fail "$name run"; }
  grep -q '"ok": true' "$WORK/$name.out" || { cat "$WORK/$name.out" >&2; fail "$name did not report ok"; }
  n=$((n+1))
done
ok "$n test programs compile as bytecode v5, run and pass their own assertions"

# Gallery: every image the README references exists and is a valid PNG/GIF; every linked example exists.
img_ok() { # file
  head -c 8 "$1" | od -An -tx1 | tr -d ' \n' | grep -q '^89504e470d0a1a0a' && return 0
  head -c 6 "$1" | grep -q '^GIF8' && return 0
  return 1
}
gal=0
for img in $(grep -oE 'docs/images/[A-Za-z0-9_.-]+\.(png|gif)' README.md | sort -u); do
  [ -s "$img" ] || fail "README references missing image $img"
  img_ok "$img" || fail "$img is not a valid PNG/GIF"
  [ "$(wc -c <"$img")" -le 3145728 ] || fail "$img is larger than 3 MB"
  gal=$((gal+1))
done
for ex in $(grep -oE 'examples/[A-Za-z0-9_/.-]+\.lms' README.md | sort -u); do
  [ -f "$ex" ] || fail "README links missing example $ex"
done
ok "README gallery: $gal images exist and are valid, linked examples exist"

# Examples are self-contained: each runs its own computation, draws with lmplot and writes images/ and
# build/ under the current directory. They need lmplot (on PATH or installable), so skip loudly without it.
if command -v lmplot >/dev/null 2>&1 || [ "${LMSPACE_TEST_NETWORK:-}" = 1 ]; then
  EXDIR="$WORK/exrun"; mkdir -p "$EXDIR"
  n=0
  for src in examples/*.lms; do
    name="$(basename "$src" .lms)"
    case "$name" in anim_*) command -v ffmpeg >/dev/null 2>&1 || { echo "SKIPPED: $name needs ffmpeg"; continue; };; esac
    (cd "$EXDIR" && "$LMSCRIPT" "$ROOT/$src") >"$WORK/$name.log" 2>&1 || { cat "$WORK/$name.log" >&2; fail "$name example"; }
    n=$((n+1))
  done
  imgs=0
  for f in "$EXDIR"/images/*.png; do [ -s "$f" ] && img_ok "$f" || fail "example image $f missing or not a PNG"; imgs=$((imgs+1)); done
  ok "$n examples ran by themselves (compute, draw) and wrote $imgs valid PNGs"
  for f in "$EXDIR"/build/examples/*.csv; do echo "$(sha "$f") $(basename "$f")"; done >"$WORK/before.txt"
  for src in examples/*.lms; do
    case "$(basename "$src" .lms)" in anim_*) command -v ffmpeg >/dev/null 2>&1 || continue;; esac
    (cd "$EXDIR" && "$LMSCRIPT" "$ROOT/$src") >/dev/null 2>&1 || fail "$src rerun"
  done
  for f in "$EXDIR"/build/examples/*.csv; do echo "$(sha "$f") $(basename "$f")"; done >"$WORK/after.txt"
  cmp -s "$WORK/before.txt" "$WORK/after.txt" || fail "result files differ on rerun"
  ok "all $(wc -l <"$WORK/after.txt" | tr -d ' ') result files are byte-identical on rerun"
else
  echo "SKIPPED: examples (no lmplot on PATH and LMSPACE_TEST_NETWORK unset): images were not regenerated"
  ok "examples skipped loudly (no lmplot, LMSPACE_TEST_NETWORK unset)"
fi

# Error paths: each snippet must fail the run with exit 3 and a message naming the problem.
expect_error() { # name, snippet, expected message fragment
  cat >"$WORK/$1.lms" <<LMS
p = _dsl_ cli="lmspace";
$2
end _dsl_
savedsl(p, "$WORK/$1.dslb");
LMS
  "$LMSCRIPT" "$WORK/$1.lms" >/dev/null 2>"$WORK/$1.cerr" || { cat "$WORK/$1.cerr" >&2; fail "$1 compile"; }
  set +e; out="$("$HOST" --dsl "$WORK/$1.dslb" 2>/dev/null)"; code=$?; set -e
  [ "$code" = 3 ] || fail "$1: exit code $code, expected 3 ($out)"
  echo "$out" | grep -q "$3" || fail "$1: message lacks '$3': $out"
}
# Error snippets are maintained with the registered API below.
expect_error badmu 'let x = period(1.0, -1.0);' "mu"
expect_error badorbit 'let x = from_elements(elements(1.0, 2.0, 0.0, 0.0, 0.0, 0.0), 1.0);' "semilatus"
expect_error index 'let x = hohmann(1.0, 2.0, 1.0).get(2, 0);' "outside"
expect_error vector_index 'let x = vector(0.0, 1.0, 2.0).get(3);' "outside"
expect_error singular 'let x = lambert(vector(1.0, 0.0, 0.0), vector(-1.0, 0.0, 0.0), 1.0, 1.0, true);' "collinear"
expect_error badstep 'let s = space.system(); s.add(space.body("a", 1.0, 1.0, 0.0, 0.0, 0.0, 1.0, 0.0)); let x = s.propagate(1.0, 2, 0.0, "rk45", 0.000001);' "grid"
expect_error badmethod 'let s = space.system(); s.add(space.body("a", 1.0)); let x = s.propagate(1.0, 2, 0.1, "invalid", 0.000001);' "method"
expect_error kernel 'space.load_kernel("does-not-exist.bsp");' "CSPICE"
expect_error noephemeris 'let x = space.ephemeris("MOON", "EARTH", "J2000", 0.0);' "CSPICE"
expect_error frame 'let x = space.frame("J2000", "INVALID_FRAME", 0.0);' "CSPICE"
expect_error nobodies 'let x = space.system().propagate(1.0, 2, 0.1, "rk45", 0.000001);' "at least one body"
expect_error assertfail 'assert_close(1.0, 2.0, 0.1);' "assert_close"
expect_error nan_assert 'assert_close(sqrt(-1.0), 0.0, 0.1);' "assert_close"
expect_error badtol 'assert_close(1.0, 1.0, -0.1);' "assert_close"
ok "errors stop with exit 3 and clear messages (14 cases)"

# Memory budget: a runaway loop of large propagation tables stops with a run error (exit 3, message
# on stderr and in the JSON line) instead of exhausting the machine. A small budget everywhere; on
# Linux also the default 4096 MiB under a 6 GiB address-space limit (ulimit only here, never in the host).
cat >"$WORK/runaway.lms" <<LMS
p = _dsl_ cli="lmspace";
let s = space.system();
let k = 0;
while (k < 16) {
  s.add(space.body("b", 1.0, 1.0 + float(k), 0.0, 0.0, 0.0, 1.0, 0.0));
  k = k + 1;
}
let i = 0;
while (i < 1000000) {
  let m = s.propagate(0.0, 1000000, 1.0, "symplectic", 0.000001);
  i = i + 1;
}
end _dsl_
savedsl(p, "$WORK/runaway.dslb");
LMS
"$LMSCRIPT" "$WORK/runaway.lms" >/dev/null || fail "runaway.lms compile"
set +e; out="$(LMSPACE_MAX_MEMORY_MB=64 "$HOST" --dsl "$WORK/runaway.dslb" 2>"$WORK/runaway.err")"; code=$?; set -e
[ "$code" = 3 ] || fail "runaway loop under LMSPACE_MAX_MEMORY_MB=64: exit $code, expected 3 ($out)"
echo "$out" | grep -q "memory budget exceeded" || fail "runaway loop: JSON lacks the budget message: $out"
grep -q "LMSPACE_MAX_MEMORY_MB" "$WORK/runaway.err" || fail "runaway loop: no budget message on stderr"
set +e; LMSPACE_MAX_MEMORY_MB=-5 "$HOST" --dsl "$WORK/runaway.dslb" >/dev/null 2>"$WORK/badenv.err"; code=$?; set -e
[ "$code" = 2 ] && grep -q "positive whole number" "$WORK/badenv.err" || fail "bad LMSPACE_MAX_MEMORY_MB: exit $code"
if [ "$UNAME_S" = Linux ]; then
  set +e; out="$(ulimit -v 6291456; "$HOST" --dsl "$WORK/runaway.dslb" 2>/dev/null)"; code=$?; set -e
  [ "$code" = 3 ] && echo "$out" | grep -q "limit 4096 MiB" || fail "runaway loop with the default budget under ulimit -v 6 GiB: exit $code ($out)"
  ok "memory budget: runaway loop stops at 64 MiB and at the default 4096 MiB (under ulimit -v 6 GiB)"
else
  ok "memory budget: runaway loop stops at LMSPACE_MAX_MEMORY_MB=64 (default-budget run is Linux-only)"
fi

# Dry run loads/verifies bytecode without executing numerical work or writing CSV.
cat >"$WORK/dry.lms" <<LMS
p = _dsl_ cli="lmspace";
let out = space.system().add(space.body("a", 1.0, 1.0, 0.0, 0.0, 0.0, 1.0, 0.0)).add(space.body("b", 1.0, -1.0, 0.0, 0.0, 0.0, -1.0, 0.0)).propagate(1.0, 5, 0.01, "rk45", 0.000000001);
out.save_csv("$WORK/dry.csv");
end _dsl_
savedsl(p, "$WORK/dry.dslb");
LMS
"$LMSCRIPT" "$WORK/dry.lms" >/dev/null 2>&1 || fail "dry-run program compile"
"$HOST" --dry-run --dsl "$WORK/dry.dslb" >"$WORK/dry.out" || fail "dry run"
[ ! -e "$WORK/dry.csv" ] || fail "dry run created CSV"
"$HOST" --dsl "$WORK/dry.dslb" >/dev/null || fail "dry program run"
[ -s "$WORK/dry.csv" ] || fail "real run wrote no CSV"
ok "dry run verifies without writing outputs"

for flags in '--unknown' '--version extra' '--dsl-schema --bogus' '--dry-run' '--dsl --bogus'; do
  set +e
  # Deliberate word splitting: each fixed test string is a CLI argument sequence.
  # shellcheck disable=SC2086
  "$HOST" $flags >/dev/null 2>&1
  code=$?
  set -e
  [ "$code" = 2 ] || fail "usage flags '$flags': exit $code"
done
ok "invalid CLI combinations use exit 2"

set +e; "$HOST" --dsl "$WORK/does-not-exist.dslb" >/dev/null 2>&1; code=$?; set -e
[ "$code" = 3 ] || fail "missing bytecode exit code $code, expected 3"
set +e; "$HOST" >/dev/null 2>&1; code=$?; set -e
[ "$code" = 2 ] || fail "usage exit code $code, expected 2"
ok "usage and load errors use exit codes 2 and 3"

echo "all $pass checks passed"
