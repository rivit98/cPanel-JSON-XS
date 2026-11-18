# `allow_tags`

| | |
|---|---|
| field | `jx_cfg_t.options` bit 14 (`jx_option_names[14]`), separately for `dec` and `enc` |
| distribution | on with p = 1/2, independent of every other option bit (one `rand64() & JXO_ALL` draw) |
| set by | `jx_object()`: `$obj->allow_tags(1)` or `$obj->allow_tags(0)`, only when it differs from the state after `new` |
| default after `new` | off |

## What it changes

Encoder: a blessed object with a `FREEZE` method is written in the tagged form `("Class")[values...]`. Decoder: accepts that syntax and calls `Class->THAW("JSON", values...)`.

## Why it is tested

It is a non-standard syntax with its own parser branch, and a callback in both directions. The fixture classes have one-letter names so the mutator can produce a tag that names one; `json.dict` has them too.

| class | `THAW` | `FREEZE` |
|---|---|---|
| `F` | blesses the values | returns them |
| `E` | dies | dies |
| `S` | returns nothing | returns the object itself |
| `H` | builds a hash (odd counts give an odd hash) | returns a flattened hash |
| `N` | returns nothing | returns nothing |
| `R` | returns a blessed scalar ref | returns other objects and refs |
| `O` | returns an overloaded object | returns its string/number forms |
| `U` | no method | no method |

## Interactions

- Checked before `convert_blessed` on encode.
- `filter_json_object` and `filter_json_single_key_object` apply to the arrays inside a tag's values as normal.
