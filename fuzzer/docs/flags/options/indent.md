# `indent`

| | |
|---|---|
| field | `jx_cfg_t.options` bit 4 (`jx_option_names[4]`), separately for `dec` and `enc` |
| distribution | on with p = 1/2, independent of every other option bit (one `rand64() & JXO_ALL` draw) |
| set by | `jx_object()`: `$obj->indent(1)` or `$obj->indent(0)`, only when it differs from the state after `new` (and `pretty`) |
| default after `new` | off |

## What it changes

Encoder only: arrays and objects are written one element per line, indented by three spaces per level, with a trailing newline.

## Why it is tested

It adds an indentation writer whose width depends on nesting depth. With deep builder structures (up to `BUILD_MAX_DEPTH` 64) and `max_depth` up to 512, it exercises output-buffer growth for whitespace that does not come from the input.

## Interactions

- `pretty` turns `indent` on first; the option bit then decides the final state (see [pretty.md](pretty.md)).
- Re-decoding the indented output tests the decoder's whitespace skipping.
