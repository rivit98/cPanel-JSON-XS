# `allow_singlequote`

| | |
|---|---|
| field | `jx_cfg_t.options` bit 16 (`jx_option_names[16]`), separately for `dec` and `enc` |
| distribution | on with p = 1/2, independent of every other option bit (one `rand64() & JXO_ALL` draw) |
| set by | `jx_object()`: `$obj->allow_singlequote(1)` or `$obj->allow_singlequote(0)`, only when it differs from the current state |
| default after `new` | off |

## What it changes

Decoder: strings may be single-quoted (`'a'`), with `\\'` as an escape. Also set by `relaxed`. The incremental parser has two modes of its own for it (`INCR_M_SQSTR`, `INCR_M_SQBS`).

## Why it is tested

A second string terminator through `_decode_str()`, and two extra states in the incremental scanner.
