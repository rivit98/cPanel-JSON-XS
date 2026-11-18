# `utf8`

| | |
|---|---|
| field | `jx_cfg_t.options` bit 3 (`jx_option_names[3]`), separately for `dec` and `enc` |
| distribution | on with p = 1/2, independent of every other option bit (one `rand64() & JXO_ALL` draw) |
| set by | `jx_object()`: `$obj->utf8(1)` or `$obj->utf8(0)`, only when it differs from the state after `new` |
| default after `new` | off |

## What it changes

Encoder: the output is UTF-8 encoded octets. Decoder: the input must be UTF-8 octets, and a string with code points above 0xFF is an error ("Wide character").

## Why it is tested

It is the main switch between the octet and character code paths. Combined with `input_kind` it gives every pairing of octet/character mode and UTF8-flagged/unflagged input scalar, including invalid UTF-8 under `utf8::decode` (kind 2), where the flag stays off.

## Also changed at run time

- `test_incr` with `AUX_INCR_SWAP` flips `utf8` on the `dec` object between chunks, so the incremental parser has to convert its buffer half-way through (see [../aux/incr_swap.md](../aux/incr_swap.md)).
- `test_functional` ignores this flag: `decode_json`/`encode_json` are always `utf8`.
