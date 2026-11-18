# `allow_nonref`

| | |
|---|---|
| field | `jx_cfg_t.options` bit 8 (`jx_option_names[8]`), separately for `dec` and `enc` |
| distribution | on with p = 15/16. The base option draw sets it with p = 1/2 and three bits of `r` force it on in 7 of 8 cases, so it is off only when both say off: 1/2 x 1/8 = 1/16. (The comment in `setup_cfg()` says 7/8; the measured rate is 0.9377.) With `nonref_set` (p = 1/4) the setter is called even when the bit already matches, which sets `F_NONREF_SET` - the incremental parser refuses a bare top-level scalar without it. |
| set by | `jx_object()`: `$obj->allow_nonref(1)` or `$obj->allow_nonref(0)`, when it differs from the state after `new`, or always with `nonref_set` |
| default after `new` | **on**: `json_init()` enables it, so the harness calls `->allow_nonref(0)` only when the bit is clear |

## What it changes

With it on, the decoder accepts a top-level scalar (`1`, `"x"`, `null`) and the encoder writes one. With it off, the top level must be an array or object, and anything else is an error.

## Why it is tested, and why it is skewed

Most of the JSONTestSuite seeds and many builder programs have a scalar at the top level. With it off, those inputs stop at the first check and the rest of the decoder or encoder is never reached, so off is kept rare (1/16) but present: the rejection path and its error message are still covered.

## Interactions

- `AUX_BUILD_ROOT` wraps a single top-level builder value in an array, which reaches the encoder even with `allow_nonref` off.
- `test_functional` ignores it: `decode_json`/`encode_json` use their own defaults.
