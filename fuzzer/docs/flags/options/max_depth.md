# `max_depth`

| | |
|---|---|
| field | `jx_cfg_t.max_depth`, separately for `dec` and `enc` |
| distribution | set with p = 1/4; then uniform over `{0, 1, 2, 3, 4, 8, 32, 100, 512}` (about 1/36 each) |
| set by | `jx_object()`: `$obj->max_depth(n)` |
| when not set | the setter is not called; Cpanel::JSON::XS's default of 512 applies |

`0` is stored as `JX_MAX_DEPTH_ZERO` (`0x80000000`) so that "not set" (0 in
the struct) and "`max_depth(0)`" stay distinct. `print_flags()` shows it as
`(0) 0`.

## What it changes

Decoder and encoder: nesting deeper than `n` arrays/objects is an error. With
0, nothing may nest at all, so even `[]` is rejected.

## Why it is tested

- The small values (0..4) put the depth error on the path of almost every
  input and test its unwinding: the error is raised deep inside a partly
  built structure, and anything already allocated must be freed (the SV leak
  check watches this).
- `test_incr`: the incremental parser tracks depth on its own while scanning
  for the end of a value, and has to agree with the decoder.

## Why never above 512

The limit exists to keep the recursive decoder and encoder from running off
the end of the C stack. A campaign that raised it would find a stack overflow
for every input shape that nests deeply enough: a known, documented
limitation, not a bug.
