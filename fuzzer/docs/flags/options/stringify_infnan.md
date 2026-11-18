# `stringify_infnan`

| | |
|---|---|
| field | `jx_cfg_t.infnan`, separately for `dec` and `enc` |
| distribution | set with p = 1/4; then uniform over modes 0..3 and the invalid 4 |
| set by | `jx_object()`: `$obj->stringify_infnan(mode)` |

## What it changes

How the encoder writes Inf/NaN (and NaN/Inf bignums): 0 as `null`, 1 as strings, 2 as the platform's stringification, 3 as bare `inf`/`nan` (invalid JSON). 4 croaks in the setter.

## Why it is tested

Each mode is a different writer; mode 2 depends on the C library's spelling.
