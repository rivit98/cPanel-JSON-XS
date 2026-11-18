# `require_types`

| | |
|---|---|
| field | `jx_cfg_t.options` bit 22 (`jx_option_names[22]`), separately for `dec` and `enc` |
| distribution | on with p = 1/8: drawn like every other bit and then ANDed with two more random draws (`JXO_RARE` in `setup_cfg()`). Measured: 0.125 |
| set by | `jx_object()`: `$obj->require_types(1)` or `$obj->require_types(0)`, only when it differs from the current state |
| default after `new` | off |

## What it changes

Encoder: croaks unless a type spec is given (`encode($v, $types)`).

## Why it is tested

Mostly cuts encoding short, hence p = 1/8; with a type spec (`@JXF::types`, `BOP_TYPES`, the types decode found) it is the typed encoder's own path.
