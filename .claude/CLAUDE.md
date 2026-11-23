# Cpanel::JSON::XS fuzzing checkout

This is the Cpanel-JSON-XS CPAN release with an AFL++ campaign on top:
`./fuzz` drives builds, the campaign and triage (see `fuzzer/README.md`), and
the harness is `src/fuzz/` - `main.c` (C test cases, groups, the builder),
`fuzz_perl.h` (embedded perl), `fixture.pl` (the perl half). No file of the
library is in the repository: the build downloads the CPAN release
(`Cpanel-JSON-XS-4.83.tar.gz`, version and SHA-256 pinned in
`src/fuzz/CMakeLists.txt`) into `fuzzer/build/` and `src/fuzz/upstream.cmake`
unpacks it, unpatched, into `fuzzer/build/<variant>/upstream/Cpanel-JSON-XS-4.83/`
(that is where `XS.xs` is). Never add upstream files to the repository or patch
the unpacked copy. The setup was ported from the JSON::XS campaign in
`../JSON-XS-4.04`.

## Host

The host is shared and under heavy load: run at most **two** AFL instances
locally (`JOBS=2`), and keep parallel replays and builds at `-P2` /
`FUZZ_BUILD_JOBS=2`.

## Crash triage

Turning an artifact, a sanitizer log or a whole `logs_test_*` directory into
`RA()`/`RF()` pairs and `reportN.md` files: use the `triage-crash` skill
(`/triage-crash <artifact | logs dir> [log]`).

- Define every `RA()`/`RR()` for a triage batch in one edit and rebuild once;
  the same for the `RF()` entries.
- Digging into `XS.xs` is not required to file a report. The sanitizer stack,
  the failing `test_*` function and its `JXF::t_*` body are enough.
- The reproducer in `reportN.md` is a perl script derived from the failing
  `JXF::t_*` sub and the `RF()` flags, not one written from library knowledge.
- Suspect the harness and the fixture first: a finding counts only if a sane
  perl program reaches it.
- An SV leak must reproduce in a plain perl loop (flat vs. growing RSS over
  ~10^5 iterations, `perl -Mblib` against a tarball build in the scratchpad)
  before it is reported. With a scalar type spec (`JSON_TYPE_INT/FLOAT/STRING`)
  the encoder writes a reference's address, which makes replays
  nondeterministic (fuzzer/README.md, Notes).

## Builds

- Every build directory lives in `fuzzer/build/<variant>/`; the reproducers
  are `fuzzer/build/repro` (ASAN+UBSAN) and `fuzzer/build/repro-msan` (MSAN,
  linked against `fuzzer/build/perl-msan`).
- Reuse existing builds and rebuild incrementally
  (`cmake --build fuzzer/build/repro --target fuzz2`). Configure only when the
  directory is missing, through `./fuzz build <variant>`.
- Never build in `fuzzer/build/` while `./fuzz build` runs
  (`pgrep -af 'fuzz build'`).

## Reports

- Reports live in `reports/reportN.md` (one issue per file), never at the
  repository root. The `/* reportN.md */` tags in `main.c` refer to them.
- Every reported bug keeps its `RA()` + `RF()` pair at the bottom of
  `src/fuzz/main.c`, tagged `/* reportN.md */`.
- Never send a report on your own: that is the user's call.
- Once upstream fixes a bug, confirm that `tc<n>` passes, then delete its pair.
