# `nonref_set`

| | |
|---|---|
| field | `jx_cfg_t.nonref_set`, separately for `dec` and `enc` |
| distribution | p = 1/4 |
| set by | `jx_object()`: `$obj->allow_nonref(x)` called even when the bit already matches |

## What it changes

Sets `F_NONREF_SET`: the incremental parser only accepts a bare top-level scalar (number, `true`, ...) when `allow_nonref` was called explicitly.

## Why it is tested

Without it `incr_parse` would never reach the bare-scalar path, since `allow_nonref` is on by default and the setter would never be called.
