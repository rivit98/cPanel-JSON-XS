# `allow_dupkeys`

| | |
|---|---|
| field | `jx_cfg_t.options` bit 21 (`jx_option_names[21]`), separately for `dec` and `enc` |
| distribution | on with p = 1/2, independent of every other option bit (one `rand64() & JXO_ALL` draw) |
| set by | `jx_object()`: `$obj->allow_dupkeys(1)` or `$obj->allow_dupkeys(0)`, only when it differs from the current state |
| default after `new` | off |

## What it changes

Decoder: duplicate object keys are allowed (the last one wins). Off: a duplicate key croaks. Also set by `relaxed` and by `dupkeys_as_arrayref`.

## Why it is tested

Duplicate detection runs per key on every object; the error path frees the half-built hash.
