# `unblessed_bool`

| | |
|---|---|
| field | `jx_cfg_t.options` bit 20 (`jx_option_names[20]`), separately for `dec` and `enc` |
| distribution | on with p = 1/2, independent of every other option bit (one `rand64() & JXO_ALL` draw) |
| set by | `jx_object()`: `$obj->unblessed_bool(1)` or `$obj->unblessed_bool(0)`, only when it differs from the current state |
| default after `new` | off |

## What it changes

Decoder: `true`/`false` become `!!1`/`!!0` (perl's immortal yes/no) instead of `JSON::PP::Boolean` objects. Encoder: perl's own booleans are written as `true`/`false`.

## Why it is tested

Changes which values reach the encoder as booleans, and the decoder's literal path.
