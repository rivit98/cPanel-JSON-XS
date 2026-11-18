# `convert_blessed`

| | |
|---|---|
| field | `jx_cfg_t.options` bit 11 (`jx_option_names[11]`), separately for `dec` and `enc` |
| distribution | on with p = 1/2, independent of every other option bit (one `rand64() & JXO_ALL` draw) |
| set by | `jx_object()`: `$obj->convert_blessed(1)` or `$obj->convert_blessed(0)`, only when it differs from the state after `new` |
| default after `new` | off |

## What it changes

Encoder: a blessed object with a `TO_JSON` method is replaced by the result of calling it.

## Why it is tested

It runs perl code in the middle of encoding. The fixture classes in [fixture.pl](../../../../src/fuzz/fixture.pl) cover the cases that are hard to handle correctly:

| class | `TO_JSON` |
|---|---|
| `F` | a new array (well-behaved) |
| `E` | dies |
| `S` | returns the object itself |
| `H` | a new hash |
| `N` | `undef` |
| `R` | an object of class `F` (recursion one level deep, never into its own class) |
| `O` | an overloaded object's stringification |
| `U` | no method |

## Interactions

- `allow_tags` is checked first. If both apply, `FREEZE` wins.
- A `die` inside `TO_JSON` unwinds through the encoder. The `eval` in `t_encode`/`t_decode` catches it, and the leak check looks for SVs left behind.
