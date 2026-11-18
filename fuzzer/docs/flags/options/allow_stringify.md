# `allow_stringify`

| | |
|---|---|
| field | `jx_cfg_t.options` bit 19 (`jx_option_names[19]`), separately for `dec` and `enc` |
| distribution | on with p = 1/2, independent of every other option bit (one `rand64() & JXO_ALL` draw) |
| set by | `jx_object()`: `$obj->allow_stringify(1)` or `$obj->allow_stringify(0)`, only when it differs from the current state |
| default after `new` | off |

## What it changes

Encoder: references and objects that are not otherwise encodable are written as their stringification (`"$ref"`, or the overloaded `""`).

## Why it is tested

Calls back into perl (overloads) from the encoder; with plain refs the output contains a heap address, so the output text varies between runs.
