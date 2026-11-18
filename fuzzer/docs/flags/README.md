# The 4-byte flag seed

Every fuzzer input is laid out as

```
bytes 0..3   uint32 seed (host byte order, memcpy'd)  -> struct flags_t
bytes 4..    payload: JSON text (json group) or a builder program (build group)
```

`LLVMFuzzerTestOneInput()` ([main.c](../../../src/fuzz/main.c)) returns at once
for inputs of 4 bytes or fewer, copies the first four bytes into `seed`, calls
`setup_flags(seed, &flags)` ([fuzz_flags.h](../../../src/fuzz/fuzz_flags.h)) and
then runs both groups (`test_json`, `test_build`) on the rest of the input with
the same `flags`. A `TC=` filter restricts that to one group or test case.

## How the seed becomes a configuration

`setup_flags()` calls `srand(seed)` and then draws everything from that one
`rand()` stream, always in the same order:

1. `setup_cfg(&flags->dec)`: the decoder object's configuration
2. `setup_cfg(&flags->enc)`: the encoder object's configuration
3. `input_kind = rand64() % 6`
4. `aux = rand64()`: 64 raw bits; each consumer reads its own bit range (`FLAGS_AUX()`)

`rand64()` joins several 31-bit `rand()` results into one 64-bit value.
`setup_cfg()` uses one `rand64()` for the 25 option bits, two more for the
rare ones (`JXO_RARE`), and one, `r`, for the rest, consuming it a few bits
per field.

The order is fixed. Adding a draw anywhere but at the end would shift every
later field and silently change the configuration of every artifact already on
disk, `RR()`/`RA()` reproducers included. New draws go after `aux`, and new
`aux` consumers get a new bit range (append only, never move one).

## Why a seed instead of raw flag bytes

- **The configuration is its own search dimension.** Because the seed goes
  through a PRNG, flipping one bit gives an unrelated configuration, not a
  neighbouring one. The mutator is not meant to edit flags field by field. AFL
  keeps a seed when that configuration plus that payload reaches new coverage,
  so useful configurations survive in the queue alongside the payloads that
  need them.
- **The payload stays clean.** Four bytes is the whole cost: the JSON text or
  builder program after it is never interpreted as configuration, so dictionary
  tokens and JSONTestSuite seeds keep their meaning.
- **Probabilities are designed, not left to chance.** Each field has a chosen
  distribution: rare for options that cut most paths short (`max_size`,
  `max_depth`), common for cheap ones. Raw bytes would make every bit 50/50.
- **Replays are deterministic.** The seed is inside the artifact, so a replay
  of the same artifact gets the same configuration. `RF()` reproducers store
  the expanded `struct flags_t` as a C initializer (`flags_to_struct_string()`),
  so they do not depend on libc's `rand()` or on later changes to
  `setup_flags()`.
- **Imported seeds start with several configurations.** `./fuzz seeds --import`
  writes every file three times, with the prefixes `00 00 00 00`,
  `2a 13 7f 01` and `ff ff ff ff` (`FLAG_PREFIXES`, `fuzzer/lib/seeds.sh`).

## Seeing what a seed expands to

Replay an input with `verbose` on to get `print_flags()` output (`dec.options`,
`max_depth`, filters, `input_kind`, `aux`). Triage writes the same
configuration as the `RF()` initializer.

## Who uses which part

| test case | `dec` | `enc` | `input_kind` | `aux` ranges |
|---|---|---|---|---|
| `test_decode` | decodes the input | re-encodes the result | yes | `DECODE_STEPS`, `GETTER` |
| `test_decode_prefix` | yes | - | yes | `PREFIX_N`, `GETTER` |
| `test_incr` | yes | - | yes | `INCR_*`, `GETTER` |
| `test_functional` | - | - | yes | `FUNC_MODE` |
| `test_encode` (build) | decodes the output | encodes the built value | - | `BUILD_ROOT`, `ENCODE_STEPS`, `TYPESPEC`, `GETTER` |

## Options tested

Measured over 1,000,000 seeds, with the `dec` configuration (`enc` is drawn the same way and is independent of `dec`).

### Cpanel::JSON::XS object configuration (`dec` and `enc` each)

| option | how often | file |
|---|---|---|
| `ascii` | 1/2 | [options/ascii.md](options/ascii.md) |
| `latin1` | 1/2 | [options/latin1.md](options/latin1.md) |
| `binary` | 1/2 | [options/binary.md](options/binary.md) |
| `utf8` | 1/2 | [options/utf8.md](options/utf8.md) |
| `indent` | 1/2 | [options/indent.md](options/indent.md) |
| `canonical` | 1/2 | [options/canonical.md](options/canonical.md) |
| `space_before` | 1/2 | [options/space_before.md](options/space_before.md) |
| `space_after` | 1/2 | [options/space_after.md](options/space_after.md) |
| `allow_nonref` | 15/16 on | [options/allow_nonref.md](options/allow_nonref.md) |
| `shrink` | 1/2 | [options/shrink.md](options/shrink.md) |
| `allow_blessed` | 1/2 | [options/allow_blessed.md](options/allow_blessed.md) |
| `convert_blessed` | 1/2 | [options/convert_blessed.md](options/convert_blessed.md) |
| `relaxed` | 1/2 | [options/relaxed.md](options/relaxed.md) |
| `allow_unknown` | 1/2 | [options/allow_unknown.md](options/allow_unknown.md) |
| `allow_tags` | 1/2 | [options/allow_tags.md](options/allow_tags.md) |
| `allow_barekey` | 1/2 (plus `relaxed`) | [options/allow_barekey.md](options/allow_barekey.md) |
| `allow_singlequote` | 1/2 (plus `relaxed`) | [options/allow_singlequote.md](options/allow_singlequote.md) |
| `allow_bignum` | 1/2 | [options/allow_bignum.md](options/allow_bignum.md) |
| `escape_slash` | 1/2 | [options/escape_slash.md](options/escape_slash.md) |
| `allow_stringify` | 1/2 | [options/allow_stringify.md](options/allow_stringify.md) |
| `unblessed_bool` | 1/2 | [options/unblessed_bool.md](options/unblessed_bool.md) |
| `allow_dupkeys` | 1/2 (plus `relaxed`, `dupkeys_as_arrayref`) | [options/allow_dupkeys.md](options/allow_dupkeys.md) |
| `require_types` | 1/8 | [options/require_types.md](options/require_types.md) |
| `type_all_string` | 1/8 | [options/type_all_string.md](options/type_all_string.md) |
| `dupkeys_as_arrayref` | 1/2 | [options/dupkeys_as_arrayref.md](options/dupkeys_as_arrayref.md) |
| `pretty` | 1/8 | [options/pretty.md](options/pretty.md) |
| `max_depth` | 1/4 set | [options/max_depth.md](options/max_depth.md) |
| `max_size` | 1/16 set | [options/max_size.md](options/max_size.md) |
| `indent_length` | 1/4 set | [options/indent_length.md](options/indent_length.md) |
| `stringify_infnan` | 1/4 set | [options/stringify_infnan.md](options/stringify_infnan.md) |
| `sort_by` | 1/8 set | [options/sort_by.md](options/sort_by.md) |
| `filter_json_object` | 1/4 | [options/filter_json_object.md](options/filter_json_object.md) |
| `filter_json_single_key_object` | ~1/4 (1/8 per key) | [options/filter_json_single_key_object.md](options/filter_json_single_key_object.md) |
| `nonref_set` | 1/4 | [options/nonref_set.md](options/nonref_set.md) |
| `subclass` | 1/8 | [options/subclass.md](options/subclass.md) |

### Input representation

| option | how often | file |
|---|---|---|
| `input_kind` | 1/6 each of 6 | [input_kind.md](input_kind.md) |

### `aux` bits (call sequence choices)

| range | bits | file |
|---|---|---|
| `AUX_INCR_CHUNK` | 0..5 | [aux/incr_chunk.md](aux/incr_chunk.md) |
| `AUX_INCR_LIST` | 6 | [aux/incr_list.md](aux/incr_list.md) |
| `AUX_INCR_TEXT` | 7 | [aux/incr_text.md](aux/incr_text.md) |
| `AUX_INCR_RESET` | 8 | [aux/incr_reset.md](aux/incr_reset.md) |
| `AUX_INCR_SWAP` | 9 | [aux/incr_swap.md](aux/incr_swap.md) |
| `AUX_PREFIX_N` | 10..13 | [aux/prefix_rounds.md](aux/prefix_rounds.md) |
| `AUX_DECODE_STEPS` | 14..19 | [aux/decode_steps.md](aux/decode_steps.md) |
| `AUX_GETTER` | 20..24 | [aux/getter.md](aux/getter.md) |
| `AUX_BUILD_ROOT` | 25 | [aux/build_root.md](aux/build_root.md) |
| `AUX_ENCODE_STEPS` | 26..31 | [aux/encode_steps.md](aux/encode_steps.md) |
| `AUX_FUNC_MODE` | 32..33 | [aux/func_mode.md](aux/func_mode.md) |
| `AUX_TYPESPEC` | 34..37 | [aux/typespec.md](aux/typespec.md) |

Bits 38..63 of `aux` are free for new consumers.

## How each object is configured

`jx_object()` ([fuzz_perl.h](../../../src/fuzz/fuzz_perl.h)) always goes through
the real perl methods, so the XS glue (ALIAS dispatch, typemap checks) is
fuzzed along with the library:

1. `Cpanel::JSON::XS->new`, or `JXF::Sub->new` (a subclass) with `subclass`
2. `->pretty(1)` if `pretty`
3. for each of the 25 options in `jx_option_names` order, `->name(1|0)` only
   when the wanted state differs from the current one (`new` starts with only
   `allow_nonref` on; `pretty` has turned on `indent`, `space_before` and
   `space_after`; `relaxed` and `dupkeys_as_arrayref` change other bits too,
   which `jx_option_effect()` tracks). `allow_nonref` is called regardless
   with `nonref_set`.
4. `->max_depth(n)`, `->max_size(n)`, `->indent_length(n)`,
   `->stringify_infnan(mode)`, `->sort_by(arg)`
5. `->filter_json_object(cb)`, then `->filter_json_single_key_object(key, cb)` per selected key
6. one `get_*` call, chosen by `AUX_GETTER`
