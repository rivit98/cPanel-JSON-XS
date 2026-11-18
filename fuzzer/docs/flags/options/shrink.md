# `shrink`

| | |
|---|---|
| field | `jx_cfg_t.options` bit 9 (`jx_option_names[9]`), separately for `dec` and `enc` |
| distribution | on with p = 1/2, independent of every other option bit (one `rand64() & JXO_ALL` draw) |
| set by | `jx_object()`: `$obj->shrink(1)` or `$obj->shrink(0)`, only when it differs from the state after `new` |
| default after `new` | off |

## What it changes

After encoding (and for decoded strings), the string buffers are shrunk to fit their contents (`SvPV_shrink_to_cur` / `sv_utf8_downgrade` where possible).

## Why it is tested

It adds a reallocation (`SvPV_shrink_to_cur`) after the output buffer has been grown, and a downgrade attempt on decoded strings. A pointer kept across that call would be stale, which ASAN catches as a use-after-free or overflow.

## Interactions

- Has the most effect with the long strings from `BOP_STR_LONG` and with large `indent` output, where the buffer was grown well past the final length.
