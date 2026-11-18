# `allow_unknown`

| | |
|---|---|
| field | `jx_cfg_t.options` bit 13 (`jx_option_names[13]`), separately for `dec` and `enc` |
| distribution | on with p = 1/2, independent of every other option bit (one `rand64() & JXO_ALL` draw) |
| set by | `jx_object()`: `$obj->allow_unknown(1)` or `$obj->allow_unknown(0)`, only when it differs from the state after `new` |
| default after `new` | off |

## What it changes

Encoder: a value JSON cannot represent (a code reference, a glob, a reference to a reference other than `\0`/`\1`) is written as `null` instead of causing an error.

## Why it is tested

The builder produces exactly those values: `BOP_CODE` (`\&JXF::noop`), `BOP_GLOB` (`*STDOUT` and `\*STDOUT`), `BOP_REF` on top of an existing reference, and `BOP_SCALAR_REF` with values other than 0/1. With it on, encoding continues past them, so the rest of the structure is covered.

## Notes

- It has no effect on the decoder, but `dec` objects get it too: in `t_decode` step bit 0 and `t_encode` step bit 3 the `dec` object encodes.
