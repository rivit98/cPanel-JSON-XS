# Build the harness variants, through whichever backend FUZZ_BACKEND resolves to.
#
#   ./fuzz build                  # every variant
#   ./fuzz build msan lsan        # only these
#   ./fuzz build --clean [...]    # wipe fuzzer/build first
#
# A variant that fails does not stop the others. The summary at the end lists
# what was built and what was not, and the exit status is non-zero if anything
# failed.
#
# src/fuzz/main.c is an LLVMFuzzerTestOneInput harness that embeds perl and
# links Cpanel::JSON::XS (XS.xs -> XS.c, utf8_range.c) statically - the CPAN
# release is downloaded and unpacked by the build itself (src/fuzz/upstream.cmake),
# nothing of it is in the repository; afl-cc links its own
# libAFLDriver.a for it (that is what -fsanitize=fuzzer means on the link line
# of an AFL build). src/fuzz/CMakeLists.txt leaves every instrumentation flag
# to the AFL_USE_* environment variables. Only the harness and the module are
# instrumented: libperl is the system's, so the edge map is Cpanel::JSON::XS's.
#
#   fast     no sanitizer          throughput lanes, catches SEGV/abort
#   asan     ASAN + UBSAN          full-check lanes; AFL_USE_UBSAN implies
#                                  -fno-sanitize-recover
#   cmplog   CMPLOG/RedQueen       handed to afl-fuzz -c, never fuzzed directly
#   msan     MSAN                  uninitialized reads, for triage. Linked
#                                  against perl-msan: an uninstrumented libperl
#                                  under MSAN is a false-positive farm
#   lsan     LSAN + SV leak check  per-input malloc leaks via __AFL_LEAK_CHECK()
#                                  and perl refcount leaks via PL_sv_count
#                                  (-DFUZZ_SV_LEAK_CHECK, fuzz_perl.h)
#   laf      laf-intel             split compares, for the plans' laf lanes
#   sand-asan / sand-msan          SAND oracles: sanitizer, fork server, no
#                                  edge map - passed to afl-fuzz -w
#   cov      no engine, no san     coverage, built with plain clang
#   repro    no engine, asan+ubsan fuzz2: the RR()/RF() reproducer, -O0, plain clang
#   repro-msan  no engine, msan    fuzz2 again against perl-msan, for findings
#                                  only MSAN reports
#   perl-msan                      not a harness: perl itself, built from source
#                                  with -fsanitize=memory, which the three MSAN
#                                  variants link. Built on demand by them.

# The perl the MSAN variants link (build_perl_msan() below); USAGE names it.
PERL_MSAN_VERSION="${PERL_MSAN_VERSION:-5.40.1}"
PERL_MSAN_SHA256="${PERL_MSAN_SHA256:-02f8c45bb379ed0c3de7514fad48c714fd46be8f0b536bfd5320050165a1ee26}"

USAGE="usage: $PROG build [--clean] [<variant>...]

  <variant>...  build only these, e.g. ./fuzz build sand-msan
  --clean       wipe fuzzer/build/ first - also how you switch backends

With no variant every one is built: ${FUZZ_VARIANTS[*]}

The MSAN variants (msan, sand-msan, repro-msan) link an MSAN-instrumented perl,
fuzzer/build/perl-msan, built from the perl $PERL_MSAN_VERSION source the
first time one of them is built (or with ./fuzz build perl-msan). The tarball
is downloaded from www.cpan.org and checked against a pinned SHA-256; put it at
fuzzer/build/perl-$PERL_MSAN_VERSION.tar.gz beforehand to build offline.

Variant names are checked before anything builds, so a typo in the last one
does not cost the builds in front of it. A variant that fails does not stop the
rest; the summary at the end gives each one's time, size and binary, or its
exit status and log, and the command exits non-zero if any failed. Every build
tees its full output to fuzzer/build/<variant>/build.log."
usage_guard "$@"

require_backend

CMAKE_COMMON="-DCMAKE_BUILD_TYPE=RelWithDebInfo"
B_SRC="$B_REPO/src/fuzz"

build_variant() { # <name> <target> <extra cmake args> [env...] -> the build's exit status
  local name="$1" target="$2" extra="$3"; shift 3
  local hdir="$BUILD_ROOT/$name" bdir log rc
  bdir="$(cpath "$BUILD_ROOT/$name")"
  log="$hdir/build.log"

  ensure_dirs "$hdir"
  log_step "building '$name' ($target) -> $hdir"

  # Everything the build prints goes to the terminal AND to the variant's own
  # build.log. PIPESTATUS, not $?, because tee is last in the pipe.
  set +e
  {
    echo "=== $(date '+%F %T')  variant=$name  target=$target  backend=$FUZZ_BACKEND"
    echo "=== env: $*"
    afl_run -n "$CONTAINER_PREFIX-build-$name" $(env_args "$@") -- "
      set -e
      if [ '$target' != fuzz ]; then
        # cov and the reproducers are deliberately NOT built with afl-cc: they
        # carry no engine, and llvm-cov should measure Cpanel::JSON::XS, not AFL's
        # edge counters.
        export CC=clang
      else
        # afl-clang-lto when afl-cc reports LTO available: collision-free edge
        # IDs instead of hashed ones. Override with FUZZ_CC_MODE=lto|fast.
        case \"\${FUZZ_CC_MODE:-auto}\" in
          lto)  CC=afl-clang-lto ;;
          fast) CC=afl-clang-fast ;;
          *)
            if afl-cc -h 2>&1 | grep -qE '\[LTO\].*AVAILABLE'; then
              CC=afl-clang-lto
            else
              CC=afl-clang-fast
            fi ;;
        esac
        export CC
      fi
      echo \"    compiler: \$CC\"
      cd $bdir
      cmake $CMAKE_COMMON $extra $B_SRC >/dev/null
      cmake --build . --target $target -j\${FUZZ_BUILD_JOBS:-\$(nproc)}
    "
  } 2>&1 | tee "$log"
  rc=${PIPESTATUS[0]}
  set -e

  if [ "$rc" -ne 0 ]; then
    log_err "'$name' build failed (exit $rc) - full log: $log"
  else
    log_ok "$name built - log: $log"
  fi
  return "$rc"
}

# ── perl-msan ─────────────────────────────────────────────────────────────────
# MSAN flags every read of memory that uninstrumented code initialised, and
# every SV Cpanel::JSON::XS reads was initialised by libperl - so the MSAN variants
# need a perl built with -fsanitize=memory. Static libperl, no threads: the
# harness needs neither a shared libperl nor ithreads, and both only add
# uninstrumented-looking paths.
#
# -fsanitize-recover=memory with halt_on_error=0 for the build itself:
# miniperl runs hundreds of build steps, and a report in perl's own code
# (there are known ones in old Configure probes) must not end the build. The
# variants that link this perl run with halt_on_error=1 (SAN_ENV).
# exit_code=0 as well: in recover mode MSAN still exits non-zero at exit when
# it counted any report, report_umrs=0 or not. Measured on 5.40.1: 79 reports,
# all in dist/PathTools/Cwd.xs (bsd_realpath, unix_canonpath), loaded by
# pod/buildtoc - and make stopped at pod/perltoc.pod. `perl -e 0` reports
# nothing, and the harness never loads Cwd.
PERL_MSAN_DIR="$BUILD_ROOT/perl-msan"
PERL_MSAN_BIN="$PERL_MSAN_DIR/install/bin/perl"

build_perl_msan() {
  local tarball="$BUILD_ROOT/perl-$PERL_MSAN_VERSION.tar.gz" log="$PERL_MSAN_DIR/build.log" rc

  ensure_dirs "$PERL_MSAN_DIR"
  log_step "building 'perl-msan' (perl $PERL_MSAN_VERSION, -fsanitize=memory) -> $PERL_MSAN_DIR"
  if [ ! -s "$tarball" ]; then
    log "downloading perl-$PERL_MSAN_VERSION.tar.gz from www.cpan.org"
    curl -sSfL -o "$tarball.part" "https://www.cpan.org/src/5.0/perl-$PERL_MSAN_VERSION.tar.gz" \
      || { log_err "download failed - put the tarball at $tarball"; return 1; }
    mv "$tarball.part" "$tarball"
  fi
  echo "$PERL_MSAN_SHA256  $tarball" | sha256sum -c --quiet - \
    || { log_err "$tarball: SHA-256 mismatch (expected $PERL_MSAN_SHA256)"; return 1; }

  rm -rf "$PERL_MSAN_DIR/src" "$PERL_MSAN_DIR/install"
  ensure_dirs "$PERL_MSAN_DIR/src"
  tar -xzf "$tarball" -C "$PERL_MSAN_DIR/src" --strip-components=1

  local msan="-fsanitize=memory -fsanitize-memory-track-origins=2 -fsanitize-recover=memory -fno-omit-frame-pointer"
  set +e
  afl_run -n "$CONTAINER_PREFIX-build-perl-msan" $(env_args "MSAN_OPTIONS=halt_on_error=0:report_umrs=0:exit_code=0") -- "
    set -e
    cd $(cpath "$PERL_MSAN_DIR/src")
    ./Configure -des -Dprefix=$(cpath "$PERL_MSAN_DIR/install") -Dcc=clang \
      -Doptimize='-O1 -g' -Accflags='$msan' -Aldflags='-fsanitize=memory' \
      -Alddlflags='-shared -fsanitize=memory' \
      -Uusethreads -Uuseshrplib -Dusemymalloc=n -Dman1dir=none -Dman3dir=none
    make -j\${FUZZ_BUILD_JOBS:-\$(nproc)}
    make install
  " >"$log" 2>&1
  rc=$?
  set -e
  if [ "$rc" -ne 0 ] || [ ! -x "$PERL_MSAN_BIN" ]; then
    log_err "perl-msan build failed (exit $rc) - full log: $log"
    return 1
  fi
  log_ok "perl-msan built - $PERL_MSAN_BIN, log: $log"
}

need_perl_msan() {
  [ -x "$PERL_MSAN_BIN" ] || build_perl_msan
}

B_PERL_MSAN="$(cpath "$PERL_MSAN_BIN")"

build_one() { # <variant>
  case "$1" in
    fast)   build_variant fast   fuzz "" ;;
    asan)   build_variant asan   fuzz "" AFL_USE_ASAN=1 AFL_USE_UBSAN=1 ;;
    cmplog) build_variant cmplog fuzz "" AFL_LLVM_CMPLOG=1 ;;
    # MSAN is pinned to afl-clang-fast: in LTO mode the binary links but its
    # fork server dies with SIGSEGV on the first exec.
    msan)   need_perl_msan && \
            build_variant msan   fuzz "-DFUZZ_PERL=$B_PERL_MSAN" AFL_USE_MSAN=1 FUZZ_CC_MODE=fast ;;
    # -DFUZZ_SV_LEAK_CHECK: perl refcount leaks, which LSAN cannot see
    # (fuzz_perl.h), abort the exec in this lane only.
    lsan)   build_variant lsan   fuzz "-DCMAKE_C_FLAGS=-DFUZZ_SV_LEAK_CHECK" AFL_USE_LSAN=1 ;;
    # laf-intel: multi-byte compares split into byte compares, so the edge map
    # sees partial progress on keywords and magic values CMPLOG does not solve.
    laf)    build_variant laf    fuzz "" AFL_LLVM_LAF_ALL=1 ;;
    # SAND oracles (docs/SAND.md): sanitizer builds with a fork server but no
    # edge instrumentation, handed to a native lane with -w.
    sand-asan) build_variant sand-asan fuzz "" AFL_USE_ASAN=1 AFL_USE_UBSAN=1 AFL_LLVM_ONLY_FSRV=1 ;;
    sand-msan) need_perl_msan && \
               build_variant sand-msan fuzz "-DFUZZ_PERL=$B_PERL_MSAN" AFL_USE_MSAN=1 AFL_LLVM_ONLY_FSRV=1 FUZZ_CC_MODE=fast ;;
    cov)    build_variant cov    fuzz_cov "" ;;
    repro)  build_variant repro  fuzz2    "" ;;
    # The MSAN reproducer, for findings ASAN cannot see.
    repro-msan) need_perl_msan && \
                build_variant repro-msan fuzz2 "-DFUZZ_PERL=$B_PERL_MSAN -DFUZZ_REPRO_SANITIZER=memory" ;;
    perl-msan) build_perl_msan ;;
  esac
}

# Arguments are checked before anything builds: a typo in the last name should
# not cost the builds in front of it.
clean=0; variants=()
for arg in "$@"; do
  case "$arg" in
    --clean) clean=1 ;;
    perl-msan) variants+=("$arg") ;;
    *)
      known=0
      for v in "${FUZZ_VARIANTS[@]}"; do [ "$v" = "$arg" ] && known=1; done
      [ "$known" = 1 ] || die "unknown variant '$arg' (${FUZZ_VARIANTS[*]} perl-msan)"
      variants+=("$arg") ;;
  esac
done
[ "${#variants[@]}" -gt 0 ] || variants=("${FUZZ_VARIANTS[@]}")
# --clean keeps perl-msan (the slow one, independent of the harness) and the
# downloaded tarballs (perl's, Cpanel-JSON-XS's). ./fuzz clean builds removes
# them with the rest.
if [ "$clean" = 1 ]; then
  find "$BUILD_ROOT" -mindepth 1 -maxdepth 1 ! -name 'perl-msan' ! -name '*.tar.gz' -exec rm -rf {} + 2>/dev/null || true
fi

declare -A rc_of took
for v in "${variants[@]}"; do
  start=$SECONDS
  rc=0; build_one "$v" || rc=$?
  rc_of[$v]=$rc
  took[$v]=$((SECONDS - start))
done

log_step "build summary"
failed=0
for v in "${variants[@]}"; do
  if [ "$v" = perl-msan ]; then b="$PERL_MSAN_BIN"; else b="$(bin_of "$v")"; fi
  if [ "${rc_of[$v]}" -eq 0 ] && [ -x "$b" ]; then
    log_ok "$(printf '%-10s built   %6s  %5s  %s' "$v" "$(human_age "${took[$v]}")" "$(du -h "$b" | cut -f1)" "$b")"
  else
    failed=$((failed + 1))
    reason="exit ${rc_of[$v]}"
    [ "${rc_of[$v]}" -eq 0 ] && reason="no binary"
    blog="$BUILD_ROOT/$v/build.log"
    log_err "$(printf '%-10s FAILED  %6s  %-9s %s' "$v" "$(human_age "${took[$v]}")" "$reason" "$blog")"
  fi
done

[ "$failed" -eq 0 ] || die "$failed of ${#variants[@]} variant(s) failed"
exit 0
