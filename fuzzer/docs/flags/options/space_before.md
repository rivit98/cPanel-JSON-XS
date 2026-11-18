# `space_before`

| | |
|---|---|
| field | `jx_cfg_t.options` bit 6 (`jx_option_names[6]`), separately for `dec` and `enc` |
| distribution | on with p = 1/2, independent of every other option bit (one `rand64() & JXO_ALL` draw) |
| set by | `jx_object()`: `$obj->space_before(1)` or `$obj->space_before(0)`, only when it differs from the state after `new` (and `pretty`) |
| default after `new` | off |

## What it changes

Encoder only: a space is written before the `:` that separates key and value.

## Why it is tested

It is one of the three whitespace options that `pretty` controls, and the cheapest to keep varied. The interesting part is not the space itself but the setter: the `pretty`/option-bit sequence calls it with both `1` and `0`, which goes through the shared ALIAS setter in the XS glue.

## Interactions

- `pretty` turns it on; a cleared bit then turns it off again (see [pretty.md](pretty.md)).
