# `AUX_DECODE_STEPS`: extra steps of `test_decode`

| | |
|---|---|
| bits | `aux` 14..19 (6 bits, each independent, p = 1/2) |
| used by | `test_decode` → `JXF::t_decode($dec, $enc, $in, $steps)` |

## What it does

`t_decode` always does `$v = $dec->decode($in)` (with bit 4: `$dec->decode($in, $types)`,
which also returns the types it found) and, if that worked,
`$out = $enc->encode($v)`. The bits add:

| bit | step | what it covers |
|---|---|---|
| 0 (`aux` 14) | `$dec->encode($v)` | encoding with the decoder's options, such as an `allow_tags` object encoding its own results |
| 1 (`aux` 15) | `$enc->decode($out)` | round trip: the encoder object reads its own output |
| 2 (`aux` 16) | `$dec->decode($out)` | round trip through the original decoder, with options that may not match the encoder's (`utf8` versus not) |
| 3 (`aux` 17) | `Cpanel::JSON::XS::encode_json($v)` | the functional encoder, with default options |
| 4 (`aux` 18) | `decode($in, $types)`, then `$enc->encode($v, $types)` | the decoder's type output and the typed encoder, on a value and a type spec that match |
| 5 (`aux` 19) | `$dec->decode($typed)` | the typed encoder's output read back (only with bit 4) |

## Why it is tested

A decoded value is the most natural input for an encoder: it contains
exactly what the decoder produces (filter callback results, `THAW` objects,
bignums, booleans, strings with or without the UTF8 flag). The steps put
that value through every combination of objects. Each step is optional, so
the fuzzer can also find inputs whose new coverage comes from the decode
alone, with fewer calls per exec.
