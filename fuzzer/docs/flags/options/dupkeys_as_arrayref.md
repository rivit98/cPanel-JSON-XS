# `dupkeys_as_arrayref`

| | |
|---|---|
| field | `jx_cfg_t.options` bit 24 (`jx_option_names[24]`), separately for `dec` and `enc` |
| distribution | on with p = 1/2, independent of every other option bit (one `rand64() & JXO_ALL` draw) |
| set by | `jx_object()`: `$obj->dupkeys_as_arrayref(1)` or `$obj->dupkeys_as_arrayref(0)`, only when it differs from the current state |
| default after `new` | off |

## What it changes

Decoder: values of a duplicated key are collected into an array reference (`{"a":1,"a":2}` gives `{a => [1, 2]}`). Turning it on also turns on `allow_dupkeys`; turning it off does not turn that off.

## Why it is tested

Rewrites already-stored hash values in place while decoding - ownership of the replaced value is easy to get wrong.
