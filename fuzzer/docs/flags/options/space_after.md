# `space_after`

| | |
|---|---|
| field | `jx_cfg_t.options` bit 7 (`jx_option_names[7]`), separately for `dec` and `enc` |
| distribution | on with p = 1/2, independent of every other option bit (one `rand64() & JXO_ALL` draw) |
| set by | `jx_object()`: `$obj->space_after(1)` or `$obj->space_after(0)`, only when it differs from the state after `new` (and `pretty`) |
| default after `new` | off |

## What it changes

Encoder only: a space is written after every `:` and `,`.

## Why it is tested

Like `space_before`, it changes output length per element, so buffer growth differs from the compact form, and the setter takes part in the `pretty` on/off sequence.

## Interactions

- `pretty` turns it on; a cleared bit then turns it off again (see [pretty.md](pretty.md)).
