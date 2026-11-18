# `allow_blessed`

| | |
|---|---|
| field | `jx_cfg_t.options` bit 10 (`jx_option_names[10]`), separately for `dec` and `enc` |
| distribution | on with p = 1/2, independent of every other option bit (one `rand64() & JXO_ALL` draw) |
| set by | `jx_object()`: `$obj->allow_blessed(1)` or `$obj->allow_blessed(0)`, only when it differs from the state after `new` |
| default after `new` | off |

## What it changes

Encoder: a blessed reference that is not turned into something else by `convert_blessed` or `allow_tags` is written as `null` instead of causing an error.

## Why it is tested

Builder programs bless references into the fixture classes `F E S H N R O U` and the boolean classes (`BOP_BLESS`). With `allow_blessed` off, every such value ends the encode with an error; with it on, the encoder continues past it and the rest of the structure is written.

## Interactions

- Checked after `allow_tags` (`FREEZE`) and `convert_blessed` (`TO_JSON`). It decides only what happens to objects neither of those handled, such as class `U` (no methods).
