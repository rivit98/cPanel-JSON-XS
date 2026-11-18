# `ascii`

| | |
|---|---|
| field | `jx_cfg_t.options` bit 0 (`jx_option_names[0]`), separately for `dec` and `enc` |
| distribution | on with p = 1/2, independent of every other option bit (one `rand64() & JXO_ALL` draw) |
| set by | `jx_object()`: `$obj->ascii(1)` or `$obj->ascii(0)`, only when it differs from the state after `new` |
| default after `new` | off |

## What it changes

Encoder: every character above U+007F is written as a `\uXXXX` escape, and characters above U+FFFF as surrogate pairs, so the output is pure 7-bit ASCII. Decoder: no effect on what is accepted.

## Why it is tested

It routes every non-ASCII character through the escape writer, including the surrogate-pair split, which is otherwise reached only for non-BMP characters. In `test_encode`, builder strings (`BOP_STR_UPGRADE`, `BOP_STR_DECODE`, `BOP_KEY` with the UTF-8 bit) supply wide characters; in `test_decode` the decoded strings are re-encoded through it.

## Interactions

- With `latin1`: `ascii` takes precedence for characters above U+007F.
- With `utf8`: the output is both octets and ASCII.
- Re-decoding the `ascii` output (`DECODE_STEPS`, `ENCODE_STEPS`) parses the surrogate-pair escapes back.
