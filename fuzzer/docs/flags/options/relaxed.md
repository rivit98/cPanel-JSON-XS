# `relaxed`

| | |
|---|---|
| field | `jx_cfg_t.options` bit 12 (`jx_option_names[12]`), separately for `dec` and `enc` |
| distribution | on with p = 1/2, independent of every other option bit (one `rand64() & JXO_ALL` draw) |
| set by | `jx_object()`: `$obj->relaxed(1)` or `$obj->relaxed(0)`, only when it differs from the state after `new` |
| default after `new` | off |
| side effects | the setter is `SET_RELAXED`: it sets or clears `allow_barekey`, `allow_singlequote` and `allow_dupkeys` together with `relaxed`. `jx_object()` tracks that, so a later bit that disagrees is set back by its own setter |

## What it changes

Decoder only: accepts trailing commas in arrays and objects, `#` comments up to end of line, and literal TAB characters inside strings.

## Why it is tested

These are extra branches in whitespace skipping and the array and object loops. The json seed group has hand-written relaxed documents, and `json.dict` has the tokens, so the comment and trailing-comma paths get inputs that reach them.

## Interactions

- `test_incr`: the incremental parser's own scanner (`incr_parse` before the full decode) has to agree with the decoder on comments. A `#` that spans a chunk boundary (`AUX_INCR_CHUNK`) tests that.
