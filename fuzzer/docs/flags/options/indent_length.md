# `indent_length`

| | |
|---|---|
| field | `jx_cfg_t.indent_length`, separately for `dec` and `enc` |
| distribution | set with p = 1/4; then uniform over `{0, 1, 2, 4, 8, 15, 16, -1}` (`jx_indent_lengths[]`) |
| set by | `jx_object()`: `$obj->indent_length(n)` |

## What it changes

Spaces per indentation level when `indent` is on (default 3). 16 and -1 are out of range: a warning, no change.

## Why it is tested

Changes the buffer growth arithmetic of `encode_indent()`; 0 and 15 are the bounds.
