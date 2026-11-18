# `max_size`

| | |
|---|---|
| field | `jx_cfg_t.max_size`, separately for `dec` and `enc` |
| distribution | set with p = 1/16; then uniform over `{1, 2, 5, 16, 64, 1000}` (about 1/96 each) |
| set by | `jx_object()`: `$obj->max_size(n)` |
| when not set | unlimited (the setter is not called) |

## What it changes

Decoder: a JSON text longer than `n` bytes is rejected before parsing starts.
`decode_prefix` and the incremental parser check it as well; `incr_parse`
checks the accumulated buffer, so it can fail after several chunks.

## Why it is tested, and why rarely

Almost every input is longer than the small limits, so with `max_size` set the
decoder stops at the size check. At 1/16 the error path is covered without
cutting off most execs. The values close to typical seed lengths (16, 64,
1000) make the check pass or fail depending on the input, which tests the
boundary.

`max_size` has no effect on the encoder; `enc` objects get it anyway, and it
matters when they decode (`DECODE_STEPS` bit 1, `ENCODE_STEPS` bit 1).
