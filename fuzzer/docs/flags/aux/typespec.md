# `AUX_TYPESPEC`: the encoder's type spec

| | |
|---|---|
| bits | `aux` 34..37 (4 bits) |
| value | 0: undef (no type spec); 1..15: `@JXF::types[value - 1]` |
| used by | `test_encode`, only when the builder program did not build a type spec itself (`BOP_TYPES`) |

## What it does

`@JXF::types` (`fixture.pl`) holds type specs built once with
`Cpanel::JSON::XS::Type`: each scalar constant, `json_type_arrayof`,
`json_type_hashof`, `json_type_anyof`, `json_type_null_or_anyof`,
`json_type_optional`, plain array/hash specs, nested combinations - and `99`,
which is no type and croaks.

## Why it is tested

A fixed spec rarely matches the built value's shape, which is the point:
mismatches (an array spec for a hash, a scalar spec for a reference) are the
encoder's error paths. Matching shapes come from `BOP_TYPES` programs and from
the types decode returns (`AUX_ENCODE_STEPS` bit 5, `AUX_DECODE_STEPS` bit 4).

A scalar spec (`JSON_TYPE_INT/FLOAT/STRING`) on a reference makes the encoder
numify or stringify it - its heap address. The output then varies between
runs; see the SV leak check notes in `fuzzer/README.md`.
