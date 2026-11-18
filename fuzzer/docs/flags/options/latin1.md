# `latin1`

| | |
|---|---|
| field | `jx_cfg_t.options` bit 1 (`jx_option_names[1]`), separately for `dec` and `enc` |
| distribution | on with p = 1/2, independent of every other option bit (one `rand64() & JXO_ALL` draw) |
| set by | `jx_object()`: `$obj->latin1(1)` or `$obj->latin1(0)`, only when it differs from the state after `new` |
| default after `new` | off |

## What it changes

Encoder: the output is ISO-8859-1 octets. Characters U+0080..U+00FF are written as single bytes and anything above U+00FF as `\uXXXX`. Decoder: the input is treated as a string of characters, the same as without `utf8`.

## Why it is tested

It is a third output mode next to `utf8` and plain characters, with its own boundary at U+00FF. Strings that mix bytes in 0x80..0xFF with wider characters, and the same text with and without the UTF8 flag (`input_kind`, `BOP_STR_UPGRADE`), probe the encoder's per-character decision.

## Interactions

- With `ascii`: `ascii` wins for U+0080..U+00FF.
- With `utf8`: documented as unusual. Both are set in about 1/4 of objects, which is the point: a supported but odd combination.
