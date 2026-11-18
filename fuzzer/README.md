# AFL++ campaign for Cpanel::JSON::XS

One entry point, `./fuzz` at the repository root, drives everything: builds,
seeds, the campaign, triage and coverage. The setup is the JSON::XS campaign's
(`../JSON-XS-4.04`, itself ported from libfyaml's), carried over command for
command; what differs is the module's API - many more options, type specs,
bignums, `encode_to` - and everything that follows from that is described
under [The harness](#the-harness).

```bash
./fuzz build                        # every variant; ./fuzz build fast for one
FUZZ_GROUP=json TC=test_json JOBS=2 ./fuzz run   # start the campaign and attach
```

## Requirements

`./fuzz deps` checks the tools - on the host and inside the backend - and
names what is missing and which commands it breaks.

**On the host, whatever the backend**:

| binary | needed by |
|---|---|
| `bash` 4.3+ | every command (`mapfile`) |
| `tmux` | `run`, `stop` |
| `lscpu` | every command: one instance per physical core |
| `awk`, `df` | `status` |
| `nice`, `ps`, GNU `xargs` | `triage-all` |
| `curl`, `sha256sum`, `tar` | `build perl-msan` (downloads the perl source once) |
| `docker` | `FUZZ_BACKEND=docker`, `clean containers` |
| `taskset`, `pgrep`, `pkill` | `FUZZ_BACKEND=host`: core pinning, `status`, `stop` |
| `ssh`, `rsync` | `grab` |

**In the backend** — on `PATH` for `host`, inside
`aflplusplus/aflplusplus:stable` for `docker`:

| binary | needed by | notes |
|---|---|---|
| `afl-fuzz`, `afl-cc`, `afl-clang-fast`, `afl-clang-lto` | `run`, `build` | |
| `afl-showmap`, `afl-cmin`, `afl-tmin`, `afl-whatsup` | `seeds`, `cmin`, `coverage --edges`, `status` | |
| `cmake`, `make`, `clang` | `build` | |
| `perl` with its `CORE/` headers and `libperl.so.5.x` | `build` | Debian/Ubuntu: `perl` + `libperl5.x` (+ `libperl-dev` for the headers if `perl.h` is missing). `ExtUtils::ParseXS` and `ExtUtils::Embed` are core |
| `timeout`, `llvm-symbolizer` | `triage` | |
| `cov-analysis`, `llvm-profdata`, `llvm-cov` | `coverage` | |

The host backend is the tested one. The docker image has perl, but whether it
ships perl's `CORE/*.h` has not been checked - `./fuzz build fast` under
`FUZZ_BACKEND=docker` says so at once if not.

## Commands

`./fuzz help` lists them; `./fuzz <command> -h` prints that command's options.
They are libfyaml's, unchanged in behaviour:

| | |
|---|---|
| `deps` | check the tools are installed |
| `build` | build the harness variants — see [The builds](#the-builds) |
| `seeds` | the seed corpora — see [Seed groups](#seed-groups) |
| `run` | start the campaign in tmux — see [The instance mix](#the-instance-mix) |
| `status` | dashboard and health checks |
| `stop` | end the campaign |
| `triage` | replay one artifact through the sanitizer builds, one log per build in `logs/` (`logs_<TC>/` with `TC` set) |
| `triage-all` | replay every crash and hang of a campaign through `triage` |
| `grab` | move the crashes and hangs of remote campaigns here: `-o DIR IP...` |
| `cmin` | minimize the campaign queue for the next run |
| `coverage` | coverage report, or `--edges` for the AFL-only metric |
| `clean` | remove builds, logs, state, crashes or containers |

## Environment

| | |
|---|---|
| `FUZZ_BACKEND` | `auto` (default), `docker`, `host` |
| `FUZZ_GROUP` | seeds to start from, `fuzzer/seeds/<group>` (`json`, `build`), and the campaign's name. Unset: one empty seed, campaign `all` |
| `TC` | restrict every instance to one group function (`test_json`, `test_build`) or one test case. Unset: every test case |
| `JOBS` | instances (default: one per physical core) |
| `FUZZ_PLAN` | `auto` (default), `8`, `16`, `32`, or a path |
| `FUZZ_DIR` | campaign state root (default `../fuzz/Cpanel-JSON-XS`) |
| `FUZZ_CC_MODE` | `lto` or `fast`, forcing afl-cc's instrumentation mode |
| `FUZZ_MAX_LEN` | `-G`, max input length (default 4000) |
| `FUZZ_BUILD_JOBS` | parallel compile jobs of `build` (default `nproc`) |
| `FUZZ_SV_LEAK_CHECK` | `1`/`0`: force the SV leak check on or off in any binary (on by default only in `lsan`) |
| `VERBOSE` | harness: print the decoded flags, each test case and its timing |

## Layout

```
fuzz               the entry point: ./fuzz <command> [options]
fuzzer/
├── seeds/         the seed corpora, one directory per group (checked in)
├── plans/         instance plans: 8.plan, 16.plan, 32.plan
├── build/         every build variant, perl-msan and the downloaded tarballs (generated)
└── lib/           one file per command, common.sh sourced first
src/fuzz/
├── main.c             test cases, groups, the builder VM, RA()/RF() section
├── fuzz_perl.h        the embedded interpreter, Cpanel::JSON::XS objects, input scalars, SV leak check
├── fuzz_flags.h       struct flags_t and the 4-byte seed -> flags expansion
├── fuzz_utils.h       T()/T2()/G(), TC selection, the per-test-case perl scope, fuzz2's main()
├── fixture.pl         the perl half: test case bodies, callbacks, type specs, THAW/FREEZE/TO_JSON classes, tie classes
├── json.dict          afl-fuzz -x dictionary
├── gen_build_seeds.pl the build group's seed programs
├── embed.pl           fixture.pl -> fixture_pl.h at build time
├── upstream.cmake     downloads, checks and unpacks the Cpanel-JSON-XS release
└── CMakeLists.txt     the release pin, xsubpp XS.xs -> XS.c, and the fuzz / fuzz2 / fuzz_cov targets
```

Campaign state lives outside the repo: `../fuzz/Cpanel-JSON-XS/{state/<group or all>,logs,logs_<TC>,cov}`.

## The harness

Cpanel::JSON::XS is C, but its API is Perl: a `Cpanel::JSON::XS` object,
configured by setter methods, whose `decode`/`encode`/`incr_parse` take and
return perl scalars. The harness therefore embeds a perl interpreter.

* **The module is fetched, not vendored.** No file of Cpanel::JSON::XS is in
  this repository. Configuring a build downloads the CPAN release
  (`Cpanel-JSON-XS-4.81.tar.gz`, pinned by version and SHA-256 in
  `src/fuzz/CMakeLists.txt`) into `fuzzer/build/` once, and
  `src/fuzz/upstream.cmake` unpacks it, unpatched, into each variant's
  `fuzzer/build/<variant>/upstream/Cpanel-JSON-XS-4.81/` - the `XS.xs`
  sanitizer frames point at. A missing file there is unpacked again by the
  next `cmake --build`. To build offline, put the tarball in `fuzzer/build/`
  first; to fuzz another release, change `FUZZ_UPSTREAM_VERSION`,
  `FUZZ_UPSTREAM_SHA256` (and `FUZZ_UPSTREAM_URL` if needed) and rebuild with
  `./fuzz build --clean`.
* **The module is linked statically.** `XS.xs` goes through
  `ExtUtils::ParseXS` at build time, and `XS.c` and `utf8_range.c` (the
  module's second object, the SIMD UTF-8 validator) are compiled into the
  harness; `xs_init()` registers the boot function as
  `Cpanel::JSON::XS::bootstrap`, which `XSLoader::load` jumps to instead of
  loading a `.so`. The unmodified `XS.pm`, `XS/Boolean.pm` and `XS/Type.pm`
  are loaded from the build tree's `perl5/`. Everything they need is core
  perl, so nothing is vendored. The library sources are untouched.
* **Only the module and the harness are instrumented.** libperl is the
  system's shared library. AFL's map holds ~3000 edges (LTO) - all of them the
  module's or the harness's; the interpreter is invisible to it. ASAN still
  sees every heap block perl allocates (its malloc interceptor is global), so
  an overflow by the module into a perl buffer is caught; an overflow *read
  inside libperl* is not.
* **One interpreter, before the fork server.** `LLVMFuzzerInitialize()`
  constructs perl, loads the module (its `BOOT:` runs), compiles `fixture.pl`
  (which also preloads `Math::BigInt`, `Math::BigFloat` and `Encode`, which the
  module would otherwise load on first use) and runs one warm-up exec per
  group, all before libAFLDriver starts the deferred fork server. Persistent
  mode is libAFLDriver's.
* **A perl scope per test case.** `T_RUN()` wraps every test case in
  `ENTER/SAVETMPS ... FREETMPS/LEAVE` and clears `$@`; everything a test case
  creates is mortal, so the persistent loop never carries state between
  inputs.
* **Determinism.** `PERL_HASH_SEED=0` and `PERL_PERTURB_KEYS=0` are set
  before the interpreter exists: hash key order (which the non-canonical
  encoder walks) is otherwise randomised per hash. One source remains: a
  scalar type spec or `allow_stringify` on a reference writes its address.
* **Test logic is perl, configuration is C.** The C test case builds the two
  configured objects (`jx_object()`, every option through its real setter,
  so the XS glue is fuzzed too), the input scalar (`make_input_sv()`) and,
  for `test_build`, the perl value and its type spec; then it calls
  `JXF::t_<name>` in `fixture.pl`, the sequence of module calls, each in its
  own `eval`.

### The flag seed

Bytes 0..3 of every input are the seed; `setup_flags()` expands it through
`srand()`/`rand()` into `struct flags_t`:

| field | what it sets |
|---|---|
| `dec`, `enc` | two objects: the 25 boolean options, `pretty`, `max_depth` (never above the default 512), `max_size`, `indent_length`, `stringify_infnan`, `sort_by`, `filter_json_object` and `filter_json_single_key_object` callbacks, an explicit `allow_nonref` call (`F_NONREF_SET`), a subclass instead of the class itself |
| `input_kind` | how the input becomes a scalar: bytes, `utf8::upgrade`d, `utf8::decode`d, a tied scalar, a shared-hash-key COW scalar, a numified string |
| `aux` | the rest, by bit range: incr_parse chunk size / context / utf8 toggling / incr_text / incr_reset, decode_prefix rounds, the optional extra steps of `t_decode`/`t_encode` (typed decode/encode, `encode_to`), the functional interface variant, the encode type spec, which getter is called |

Every field, its distribution and why it is there: [docs/flags/](docs/flags/README.md).

`max_depth` stays at or below 512 on purpose: the limit exists to keep the
recursive coders off the end of the stack, and a campaign that raised it would
rediscover that deep recursion overflows, once per input shape.

### Seed groups

| group | group function (`TC=`) | test cases | input |
|---|---|---|---|
| `json` | `test_json` | `test_decode` (decode - optionally with its type output -, re-encode with both objects and with the found types, decode again), `test_decode_prefix`, `test_incr` (chunked incremental parser), `test_functional` (`decode_json`/`encode_json`, `_from_json`/`_to_json`) | JSON text |
| `build` | `test_build` | `test_encode` (encode with a type spec, `encode_to`, then decode the output) | a builder program |

The **builder** (`build_value()` in `main.c`) runs the input as a stack-machine
program that builds the encoder's input - what JSON text cannot express:
dualvars and numified strings, IV/UV/NV edge values, Inf/NaN, strings with and
without the UTF8 flag, references to scalars/code/globs, blessed objects of the
fixture classes, tied hashes and arrays, shared hash key scalars, read-only
values, shared substructures, `Math::BigInt`/`Math::BigFloat` objects
(`BOP_BIGNUM`) - and, after `BOP_TYPES`, the encoder's type spec: JSON_TYPE_*
constants (`BOP_TYPE_CONST`), arrays, hashes and the
`Cpanel::JSON::XS::Type::{ArrayOf,HashOf,AnyOf,Optional}` classes (`BOP_BLESS`). Every byte is a valid opcode
(`byte % BOP__COUNT`); it never builds a reference cycle (perl would leak it,
and the leak check would blame the module).

Seeds, checked in under `fuzzer/seeds/<group>/`, minimized with `afl-cmin`:

| group | material |
|---|---|
| `json` | JSONTestSuite (`test_parsing/`, `test_transform/`), the JSON::XS campaign's hand-written documents (tags of every fixture class, relaxed syntax, single-key filter keys, number and escape edge cases, incremental streams, deep nesting), and Cpanel-specific ones: bare keys, single quotes, duplicate keys, bignums, inf/nan, UTF-8/16/32 BOMs, comments |
| `build` | `src/fuzz/gen_build_seeds.pl`: every opcode and operand table entry, containers, tied containers, blessed objects of every class, bignums, type specs of every kind |

```bash
./fuzz seeds --import ~/more-json --group json
perl src/fuzz/gen_build_seeds.pl /tmp/b && ./fuzz seeds --import /tmp/b --group build
```

`--import` adds three flag prefixes per file. As with libfyaml, feeding one
input to both groups costs stability, so run a group with its own `TC`:

```bash
FUZZ_GROUP=json  TC=test_json  ./fuzz run
FUZZ_GROUP=build TC=test_build ./fuzz run
```

### What is not carried over from libfyaml, and why

* **Allocation failure injection.** Every perl allocation goes through
  `safesysmalloc()`, which on `NULL` prints "Out of memory!" and exits - perl
  has no out-of-memory path for a library to get wrong. `alloc_fail_nth` is
  gone from `flags_t`.
* **The deterministic heap.** libfyaml needed it because the library hashes
  raw pointers. Cpanel::JSON::XS does not, and perl's hash randomisation is switched
  off by environment instead.
* **The libfyaml-specific fixtures** (corpus documents, allocator recipes).

### What is added

* **The SV leak check.** A refcount leak in an XS module loses SVs into perl's
  arenas, which stay reachable - LSAN never reports them. `T_RUN()` compares
  `PL_sv_count` (perl's live-SV counter, the one behind "Scalars leaked")
  before and after each test case; a test case that grew it runs once more,
  and a second growth aborts with `SUMMARY: SV leak: N SV(s) in <test case>`.
  On in the `lsan` variant (`-DFUZZ_SV_LEAK_CHECK`) and with
  `FUZZ_SV_LEAK_CHECK=1` anywhere - `fuzz2` included.

## The builds

| build | flags | used for |
|---|---|---|
| `fast` | plain afl-cc | every throughput lane; catches SEGV/abort |
| `asan` | `AFL_USE_ASAN=1 AFL_USE_UBSAN=1` | the full-check lanes |
| `cmplog` | `AFL_LLVM_CMPLOG=1` | `afl-fuzz -c` |
| `msan` | `AFL_USE_MSAN=1`, linked against `perl-msan` | triage; the plans get MSAN from `sand-msan` |
| `lsan` | `AFL_USE_LSAN=1`, `-DFUZZ_SV_LEAK_CHECK` | per-input malloc leaks via `__AFL_LEAK_CHECK()` and perl refcount leaks via the SV leak check |
| `laf` | `AFL_LLVM_LAF_ALL=1` | laf-intel lanes |
| `sand-asan` | `AFL_USE_ASAN=1 AFL_USE_UBSAN=1 AFL_LLVM_ONLY_FSRV=1` | SAND oracle for `afl-fuzz -w` |
| `sand-msan` | `AFL_USE_MSAN=1 AFL_LLVM_ONLY_FSRV=1`, `perl-msan` | SAND MSAN oracle |
| `cov` | `-fprofile-instr-generate`, plain clang | `fuzz_cov` for `./fuzz coverage` |
| `repro` | `-O0`, asan+ubsan, plain clang | `fuzz2`: the `RA()`/`RF()` reproducer |
| `repro-msan` | `-O0`, msan, plain clang, `perl-msan` | `fuzz2` for MSAN-only findings |
| `perl-msan` | perl 5.40.1 from source, `-fsanitize=memory`, static, no threads | what the three MSAN builds link |

**MSAN needs an instrumented perl.** MSAN reports every read of memory that
uninstrumented code initialised, and every SV the module touches was initialised
by libperl - against the system libperl it would report on every exec (the
reason libfyaml's MSAN builds drop libclang). `./fuzz build msan` (or
`sand-msan`, `repro-msan`) first builds `fuzzer/build/perl-msan` from the
perl 5.40.1 tarball, SHA-256 pinned in `build.sh`, downloaded from
www.cpan.org unless already at `fuzzer/build/perl-5.40.1.tar.gz`.
`./fuzz build --clean` keeps it and the downloaded tarballs; `./fuzz clean builds` does not. Until it is
built, `run` drops every `-w MSAN_ORACLE` with a warning, exactly as for any
missing oracle.

The MSAN variants are **untested**: building perl from downloaded source was
not run while this setup was written. Expect to look at `perl-msan/build.log`
the first time, and at whether perl's own code reports under MSAN at startup.

## The instance mix

The plans (`plans/8.plan`, `16.plan`, `32.plan`) are libfyaml's, unchanged:
`main` + full ASAN lanes + CMPLOG + laf + diverse native lanes with SAND
oracles, `lsan` from 16 lanes up. `FUZZ_PLAN=auto` takes the smallest plan
with at least `JOBS` lanes and runs its first `JOBS` lanes, so `JOBS=2` is
`main` + `asan`. `-a FMT` is `text`, or `binary` with `FUZZ_GROUP=build`.

## Notes / gotchas

* **Seed shape.** Bytes 0..3 are the flag seed; a seed must be ≥ 5 bytes.
* **`afl-cmin` needs `@@`**, as with libfyaml: under showmap the AFLDriver
  target ignores the shared-memory input.
* **Fixture classes must stay well-behaved.** A `TO_JSON` returning a new
  object of its own class recurses until the stack ends - in any serialiser,
  so that is the fixture's bug, not the module's (it happened in the JSON::XS
  campaign: `R::TO_JSON`).
  Anything the harness or `fixture.pl` does that a sane perl program would not
  makes a finding worthless.
* **Blessing into `Cpanel::JSON::XS` is excluded** from the builder: its
  typemap takes any blessed referent's PV for a JSON struct, so `DESTROY`
  would read garbage - misuse, not a bug. The boolean classes (`JSON::PP::Boolean`, `JSON::XS::Boolean`)
  are only blessed onto scalar references, as their overloads expect.
* **Addresses in the output.** A scalar type spec (`JSON_TYPE_INT`, `FLOAT`,
  `STRING`) or `allow_stringify` applied to a reference makes the encoder
  numify/stringify it, which is its heap address
  (`encode(bless({}, "X"), JSON_TYPE_FLOAT)` gives `109620103726840.0`). The
  output then differs between runs - a little AFL instability, and decoding
  it may touch perl state for the first time on any run (a new
  `Math::BigFloat` path under `allow_bignum`). This is why the SV leak check
  reruns a growing test case up to three times instead of once: with one
  rerun those first-time caches showed up as 2-SV "leaks" that a perl loop
  does not reproduce.
* **Bignum hangs.** `decode` with `allow_bignum` keeps `1E3709551615` as a
  small `Math::BigFloat`; `encode` with `allow_stringify` but *without*
  `allow_bignum` stringifies it through the `""` overload, which writes out
  every digit (stack: `encode_stringify` → `Perl_amagic_call` → memcpy). The
  json group saves these as hangs within minutes. It is Math::BigFloat doing
  what it is asked, reached by a plausible decode→encode program - a DoS
  rather than a memory bug; decide once whether to report it, then triage
  hangs with that in mind. Since it was reported, `fixture.pl` wraps
  `Math::BigFloat::bstr` to return scientific notation (`bsstr`) once the
  exponent exceeds `BIGFLOAT_MAX_EXP` (10000), so these inputs no longer end
  as OOM kills; the decoder still sees the full exponent.
* **Known library bugs are not stepped around.** (4.81 needed one
  workaround, for report 3's `JSON_TYPE_BOOL` crash; it was removed with the
  4.82 fix.) The nine 4.81 findings (`incr_parse` crashes, encode crashes,
  bignum hangs, decode leaks) are all fixed in 4.82; the reports as sent are
  archived in `reports/cpanel-jsonxs-4.81-reports/`. The 4.81 `incr_parse`
  crashes looked like JSON::XS 4.04's reports 1/3/5 but were not the same
  roots - those reproducers are clean on 4.81.
* **Crash inputs are never minimized**, as with libfyaml.
