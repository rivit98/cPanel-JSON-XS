# `allow_bignum`

| | |
|---|---|
| field | `jx_cfg_t.options` bit 17 (`jx_option_names[17]`), separately for `dec` and `enc` |
| distribution | on with p = 1/2, independent of every other option bit (one `rand64() & JXO_ALL` draw) |
| set by | `jx_object()`: `$obj->allow_bignum(1)` or `$obj->allow_bignum(0)`, only when it differs from the current state |
| default after `new` | off |

## What it changes

Decoder: integers and floats too large for IV/NV become `Math::BigInt`/`Math::BigFloat` objects (the module builds a perl string and evaluates it). Encoder: such objects are written as numbers.

## Why it is tested

The only path that turns input digits into perl code, and the encoder's bignum detection (`is_bignum_obj()`, `bignum_compact()`). Combined with `stringify_infnan` it decides how NaN/Inf bignums print. The builder's `BOP_BIGNUM` makes real bignum objects for the encoder.
