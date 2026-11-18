# `type_all_string`

| | |
|---|---|
| field | `jx_cfg_t.options` bit 23 (`jx_option_names[23]`), separately for `dec` and `enc` |
| distribution | on with p = 1/8: drawn like every other bit and then ANDed with two more random draws (`JXO_RARE` in `setup_cfg()`). Measured: 0.125 |
| set by | `jx_object()`: `$obj->type_all_string(1)` or `$obj->type_all_string(0)`, only when it differs from the current state |
| default after `new` | off |

## What it changes

Encoder: every scalar is written as a string, whatever its flags. Decoder: the types it returns (`decode($text, $types)`) say string for every scalar.

## Why it is tested

Flattens the encoder's number/string decision, hence p = 1/8.
