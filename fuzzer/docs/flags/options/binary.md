# `binary`

| | |
|---|---|
| field | `jx_cfg_t.options` bit 2 (`jx_option_names[2]`), separately for `dec` and `enc` |
| distribution | on with p = 1/2, independent of every other option bit (one `rand64() & JXO_ALL` draw) |
| set by | `jx_object()`: `$obj->binary(1)` or `$obj->binary(0)`, only when it differs from the current state |
| default after `new` | off |

## What it changes

Encoder: strings are written as binary-safe escapes - every byte above 0x7f as `\\xNN`/`\\u00NN`, never as UTF-8. Decoder: accepts `\\xNN` escapes.

## Why it is tested

A third string encoding mode next to `ascii`/`latin1`/`utf8`, with its own escape writer in `encode_str()` and its own escape parser.
