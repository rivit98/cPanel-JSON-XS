# `allow_barekey`

| | |
|---|---|
| field | `jx_cfg_t.options` bit 15 (`jx_option_names[15]`), separately for `dec` and `enc` |
| distribution | on with p = 1/2, independent of every other option bit (one `rand64() & JXO_ALL` draw) |
| set by | `jx_object()`: `$obj->allow_barekey(1)` or `$obj->allow_barekey(0)`, only when it differs from the current state |
| default after `new` | off |

## What it changes

Decoder: object keys may be bare identifiers (`{foo:1}`). Also set by `relaxed`.

## Why it is tested

A separate key scanner (identifier characters, UTF-8 identifiers) that strict JSON never reaches.
