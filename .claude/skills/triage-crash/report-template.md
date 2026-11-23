# Standalone program and report

## 7. The standalone program

Start from the failing test case, not from scratch. Work in the scratchpad.
Cpanel::JSON::XS is used from perl, so the reproducer is a **perl script**: the
`JXF::t_*` sub the failing test case calls (`src/fuzz/fixture.pl`), with the
harness inlined away.

1. **Copy** the sub into `repro.pl`, plus any fixture class or callback the
   flags select (`F`, `E`, ... for tags and `convert_blessed`; `@JXF::obj_cb`,
   `@JXF::sk_cb`, `@JXF::sort_by`, `@JXF::types` entries by the indices in the `RF()` flags).
2. **Inline the harness away.**
   - The objects: `Cpanel::JSON::XS->new` (`JXF::Sub->new` with `.subclass`),
     `pretty` first if set, then one setter call per option in
     `jx_option_names` order whose state differs (only `allow_nonref` starts
     on; `relaxed` also sets allow_barekey/allow_singlequote/allow_dupkeys,
     `.nonref_set` calls `allow_nonref` regardless), then `max_depth`,
     `max_size`, `indent_length`, `stringify_infnan`, `sort_by` and the
     filters - the order of `jx_object()` in `fuzz_perl.h`.
   - The input:
     ```
     uv run --no-project python -I .claude/skills/triage-crash/scripts/rf_data.py src/fuzz/main.c <n> --perl
     ```
     then the `input_kind` conversion: `utf8::upgrade($data)`,
     `utf8::decode($data)`, a tied scalar, a hash key (`(keys %{{$data => 1}})[0]`)
     or `$data + 0` before use.
   - The `aux` values the C side passes (chunk size, flags) become literals.
   - A `test_build` input: write the perl value `build_value()` built by hand
     (`VERBOSE=1` does not print it - decode the program with the opcode list
     in `main.c`); dualvars need `Scalar::Util::dualvar`.
3. **Minimize a little, the code only.** Drop one call, option or `eval`, rerun;
   keep the change only if the `SUMMARY` line is unchanged. Never change the
   input bytes.

Run it against a Cpanel::JSON::XS built from the release tarball with the sanitizer,
loaded through `-Mblib`:

```
tar xzf Cpanel-JSON-XS-<version>.tar.gz && cd Cpanel-JSON-XS-<version>
perl Makefile.PL CC=clang LD=clang \
     OPTIMIZE='-g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer' \
     LDDLFLAGS="$(perl -MConfig -e 'print $Config{lddlflags}') -fsanitize=address,undefined"
make
LD_PRELOAD=$(clang -print-file-name=libclang_rt.asan-x86_64.so) \
  ASAN_OPTIONS=detect_leaks=0 perl -Mblib repro.pl
```

`LD_PRELOAD` because the system perl is not linked with ASAN. MSAN findings
need perl itself under MSAN: run the script with
`fuzzer/build/perl-msan/install/bin/perl` against a Cpanel::JSON::XS built by it. SV
leaks need no sanitizer: report `Devel::Peek`-free evidence, e.g. a loop
around the call printing `scalar(Internals::SvREFCNT(...))`, or the growth of
`Devel::Leak::NoteSV` counts if installed - whatever shows the count rising per
iteration.

## 8. The report

`reports/reportN.md` (the `reports/` directory at the repository root, never
the root itself), next free N: check existing `reports/report*.md` files and
the `reportN.md` comments in `src/fuzz/main.c`. One issue per file.

It contains exactly these parts and nothing else. No analysis, no suggested
fix:

````markdown
## Code version
Cpanel-JSON-XS <version> (CPAN), perl <perl -v version>

## How to reproduce
```perl
<repro.pl>
```

Build Cpanel::JSON::XS with <AddressSanitizer | UndefinedBehaviorSanitizer | MemorySanitizer> and run:
```
<build commands>
<run command>
```

## Output
```
<sanitizer output>
```
````

Trimming the output:

- Drop `BuildId`, the `0x...` addresses in frames, the shadow-byte block, and
  the `__libc_start_*` / `_start` frames.
- Make paths relative to the Cpanel-JSON-XS source root.
- Keep the frames that name `XS.xs`, and the perl frames between them
  collapsed to `[ ... perl ... ]`.

## Sending it

Cpanel::JSON::XS takes bug reports as GitHub issues:
https://github.com/rurban/Cpanel-JSON-XS/issues (see the top of `XS.pm`).
Suggest an issue title in your reply, `<area>: <what breaks> <when>`, and put
it above the `RF()` in `main.c` as a comment. Memory-safety bugs may deserve a
private report to the maintainer first - ask the user. Never file anything:
that is the user's call. Once it is filed, the comment is replaced by a link
to the issue.
