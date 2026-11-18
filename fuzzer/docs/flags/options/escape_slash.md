# `escape_slash`

| | |
|---|---|
| field | `jx_cfg_t.options` bit 18 (`jx_option_names[18]`), separately for `dec` and `enc` |
| distribution | on with p = 1/2, independent of every other option bit (one `rand64() & JXO_ALL` draw) |
| set by | `jx_object()`: `$obj->escape_slash(1)` or `$obj->escape_slash(0)`, only when it differs from the current state |
| default after `new` | off |

## What it changes

Encoder: `/` is written as `\\/`.

## Why it is tested

One more branch in the string escaper; cheap.
