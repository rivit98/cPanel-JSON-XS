# `input_kind`

| | |
|---|---|
| field | `flags_t.input_kind` |
| distribution | `rand64() % 6`, uniform (1/6 each) |
| used by | `make_input_sv()` ([fuzz_perl.h](../../../src/fuzz/fuzz_perl.h)), in every json-group test case: `test_decode`, `test_decode_prefix`, `test_incr`, `test_functional` |
| not used by | `test_encode`: its input is built by `build_value()` |

## What it changes

The same payload bytes are passed to Cpanel::JSON::XS as a perl scalar in one of six
internal representations:

| kind | name | how it is made | what the decoder sees |
|---|---|---|---|
| 0 | bytes | `newSVpvn` | a plain byte string, UTF8 flag off |
| 1 | upgraded | `sv_utf8_upgrade` | every byte as one character, UTF8 flag on; bytes ≥ 0x80 become two-byte sequences internally |
| 2 | decoded | `sv_utf8_decode` | UTF8 flag on only if the bytes are valid UTF-8; otherwise identical to kind 0 |
| 3 | tied | `tie my $s, 'JXF::TS'` (a `Tie::StdScalar`) | a scalar with get magic: the value comes from `FETCH` |
| 4 | shared hash key | a hash key of a new hash, via `newSVhek` | a copy-on-write scalar that shares its buffer with the hash key table (`SvIsCOW_shared_hash`) |
| 5 | number then string | `SvNV`, `SvIV` on the string | the PV, with the private numeric flags (`pIOK`/`pNOK`) set |

## Why it is tested

A perl program passes JSON text to the decoder in any of these forms,
depending on where it came from: a file read in raw mode, a `:utf8` layer,
`Encode::decode`, a tied variable, a hash key, a string that was once compared
numerically. The decoder has to handle each correctly:

- kinds 0/1/2 and the `utf8` option together give every pairing of octet and
  character mode with an unflagged or flagged scalar;
- kind 3 checks that magic is honoured (and called once) rather than reading
  the scalar's buffer directly;
- kind 4 checks that a shared, read-only buffer is not modified in place
  (`decode_json` and the incremental parser may need to copy or downgrade it);
- kind 5 checks that the decoder reads the string, not the cached number.

`t_decode` and `t_functional` pass the scalar itself (`$_[2]`, `$_[0]`), not
a copy, so its magic and COW state reach Cpanel::JSON::XS. `t_decode_prefix` and
`t_incr` copy it into a lexical first (they cut it into pieces), so a tied
input (kind 3) reaches them only as its fetched value; the UTF8 flag survives
the copy.
