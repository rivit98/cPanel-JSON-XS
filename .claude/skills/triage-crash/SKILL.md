---
name: triage-crash
description: Use when given a Cpanel::JSON::XS fuzzer artifact (AFL crash or hang file), the sanitizer log from replaying one, or a logs_test_* directory of such logs, or when asked to triage crashes, add an RA()/RF() reproducer to main.c, or write a reportN.md for upstream.
argument-hint: <artifact | logs dir> [sanitizer log]
---

# Crash triage

Input: $ARGUMENTS

You get a fuzzer **artifact** and the **sanitizer log** from replaying it
(`./fuzz triage <artifact>` writes one log per build), or a whole
`logs_test_<group>/` directory of them. For every new root cause you produce:

1. an `RA()` entry in `src/fuzz/main.c`, to find the test case that fails
2. an `RF()` entry in `main.c`, a self-contained reproducer inside the harness
3. `reports/reportN.md`, a report ready to send upstream - see
   [report-template.md](report-template.md)

Triage establishes whether the problem is real, new and the library's, and
then produces the entries without deeper analysis. Everything runs on this
checkout's `repro` / `repro-msan` builds.

**Batch the work, rebuild once per phase.** Never go root by root: cluster all
the roots first (§1), add *all* their `RA()` entries in one edit and rebuild
once (§5), run `vc<n>` for all of them, then paste *all* the `RF()` blocks in
one edit and rebuild once (§6). Number the roots 1..N up front.

**Do not dig into the library.** The sanitizer stack, the failing `test_*`
function and its `fixture.pl` body are enough; reading `XS.xs` is for
duplicate checks only (§7), never for explaining the bug.

## 1. Many logs: group, then dedup by replay

```
uv run --directory ~/workspace/fuzzing/fuzzing-tools libfuzzer-parse <logs dir> > parse.txt
```

The parser groups by top frames; its groups are not root causes (`fast` logs
have no stack, and one root shows up once per sanitizer). Replay each group's
`input:` through the reproducers and compare the **replay**:

```
( ulimit -s 8192; TC=test_<group> timeout 30 fuzzer/build/repro/fuzz2 <artifact> )
( ulimit -s 8192; TC=test_<group> timeout 30 fuzzer/build/repro-msan/fuzz2 <artifact> )
( FUZZ_SV_LEAK_CHECK=1 TC=test_<group> timeout 30 fuzzer/build/repro/fuzz2 <artifact> )
```

Script it over all groups (`xargs -P2` - the host is shared), keep the
`SUMMARY:` line, the UBSAN `runtime error:` line and the first `XS.xs` frames,
and cluster on those. `VERBOSE=1` prints the decoded flags (`stdbuf -o0`, or a
crash eats them) - several clusters often share one configuration (an option,
`input_kind`, the incr_parse chunking in `aux`), which is the cheapest way to
see that two stacks are one root.

If `./fuzz build` is running (`pgrep -af 'fuzz build'`), copy the `fuzz2`
binaries to the scratchpad and replay those.

## 2. Is it new, and is it the library?

- Compare with the `RA()`/`RF()` groups at the bottom of `main.c`, each tagged
  with its `reportN.md`, and the reports themselves (`reports/`).
- **Suspect the harness first.** A finding is only worth anything if a sane
  perl program can reach it. Check what the failing test case did:
  `jx_object()`/`make_input_sv()` in `fuzz_perl.h`, the builder in `main.c`
  (`build_value()`) and the `JXF::t_*` sub in `src/fuzz/fixture.pl`. A fixture
  class or callback that misbehaves in a way no serialiser could survive (a
  `TO_JSON` returning a new object of its own class, a cycle) is a fixture
  bug: fix the fixture, not the report.
- Frames only in libperl, with Cpanel::JSON::XS not on the stack, are perl's or
  the harness's, not the module's.
- An `SV leak` abort: confirm with `FUZZ_SV_LEAK_CHECK=1` on `fuzz2`, several
  times - a leak reproduces every time. The check reruns the test case up to
  three times, so lazily created perl caches rarely count, but a type spec
  that makes the encoder print a reference's address makes the output (and
  what decoding it touches first) vary with ASLR. Then confirm in a plain perl
  loop against a tarball build (RSS or SV counts growing per iteration)
  before writing anything.
- An MSAN report whose origin is outside Cpanel::JSON::XS and perl-msan (glibc, an
  uninstrumented XS extension) is almost certainly a false positive.
- Replay 2-3 times before calling a crash unreproducible.

## 3. The code version

The library is the CPAN release pinned in `src/fuzz/CMakeLists.txt`
(`FUZZ_UPSTREAM_VERSION`, `FUZZ_UPSTREAM_SHA256`). None of it is in the
repository: the build downloads `Cpanel-JSON-XS-<version>.tar.gz` into
`fuzzer/build/`, checks the SHA-256 and unpacks it unpatched into
`fuzzer/build/<variant>/upstream/Cpanel-JSON-XS-<version>/` - read `XS.xs`
there. The reproducer in a report is built from that same tarball (copy it
into the scratchpad).

## 4. The reproducer builds

| build | directory | binary |
|---|---|---|
| ASAN+UBSAN | `fuzzer/build/repro` | `fuzz2` |
| MSAN (origins, perl-msan) | `fuzzer/build/repro-msan` | `fuzz2` |

After editing `main.c`, rebuild incrementally:

```
cmake --build fuzzer/build/repro --target fuzz2
cmake --build fuzzer/build/repro-msan --target fuzz2
```

Configure only through `./fuzz build repro` / `./fuzz build repro-msan`.
Never create a build directory outside `fuzzer/build/`.

## 5. RA: find the failing test case

At the bottom of `main.c`, inside `#if defined REPRODUCER` and before
`main()`, add the next free numbers - every root in this one edit, then
rebuild **once**:

```c
/* reportN.md */
RA(n, "/absolute/path/to/artifact")
```

```
for n in 1 2 3; do VERBOSE=1 stdbuf -o0 fuzzer/build/repro/fuzz2 vc$n > vc$n.out 2> vc$n.err; done
```

- stdout gets a ready-to-paste `RF(n, test_func, ...)` block, among the flag
  dumps.
- stderr gets `=== Running <test> ===` before each test case and a timing line
  after it. The first rounds are `warmup_exec()` (one json, one build input).
  After them, the last `Running` line with no timing line after it is the
  failing test case.
- `TC=<test case or group>` runs only that.

## 6. RF: the reproducer

Paste the printed blocks under their `RA()`s - all roots in one edit, then one
rebuild. Replace `test_func` with the failing test case, and above it put the
sanitizer that caught the bug in a few words, then how the report is sent
(see [report-template.md](report-template.md)):

```c
/* ASAN */
// file reportN.md as a GitHub issue (see report-template.md)
RF(n,
test_incr,
(&(struct flags_t){ ... }),
"\x7b\x22...",
25
)
```

Rebuild. `fuzz2 tc<n>` has to fail with the same `SUMMARY` as the log, under
the same sanitizer (SV leaks: with `FUZZ_SV_LEAK_CHECK=1`).

## 7. Root cause

Just enough to catch duplicates: name the condition ("incr_parse with utf8
switched between chunks", "a single-key filter that dies") from the flags,
the input and the stack - not from reading `XS.xs`.

- A timeout is a bug only with a stack: sample it
  (`ASAN_OPTIONS=handle_segv=1:fast_unwind_on_fatal=0 timeout -s SEGV 8 fuzz2 <artifact>`).
- An ASAN `allocation-size-too-big` is the sanitizer's limit; perl exits on
  a failed allocation anyway.
- For MSAN, trust the origin, not the crash frame.

## 8. Standalone program and report

Follow [report-template.md](report-template.md). The reproducer in the report
is a **perl script** against a Cpanel::JSON::XS built from the tarball: the failing `JXF::t_*`
sub from `fixture.pl`, with the objects `jx_object()` would build spelled out
as setter calls and the input from `rf_data.py --perl` - not a program written
from an understanding of the library. Sending the report happens only when the
user asks.

## Common mistakes

| Mistake | Instead |
|---|---|
| Sizing a root by the parser's biggest group | Replay; `fast`'s `unknown` groups mix roots |
| Blaming Cpanel::JSON::XS for a fixture class or builder program no sane code would write | Fix the fixture (§2) |
| Rebuilding `repro` while `./fuzz build` runs | Replay snapshot copies |
| Writing the perl reproducer from scratch | Start from the `JXF::t_*` sub and the flags (report-template §7) |
| One `RA()` / `RF()` edit + rebuild per root | All of them in one edit, one rebuild per phase |
| Running the replays wide | `-P2` at most: the host is shared |
