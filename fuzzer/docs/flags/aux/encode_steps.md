# `AUX_ENCODE_STEPS`: extra steps of `test_encode`

| | |
|---|---|
| bits | `aux` 26..31 (6 bits, each independent, p = 1/2) |
| used by | `test_encode` → `JXF::t_encode($enc, $dec, $v, $steps, $types)` |

## What it does

`t_encode` always does `$out = $enc->encode($v, $types)` on the built value;
`$types` is the type spec the program built after `BOP_TYPES`, else
`@JXF::types` by [`AUX_TYPESPEC`](typespec.md), else undef. The bits add:

| bit | step | what it covers |
|---|---|---|
| 0 (`aux` 26) | `$dec->decode($out)` | the encoder's output read by a differently configured decoder |
| 1 (`aux` 27) | `$enc->decode($out)` | round trip with matching options |
| 2 (`aux` 28) | `Cpanel::JSON::XS::encode_json($v)` | the same value through the functional encoder, default options |
| 3 (`aux` 29) | `$dec->encode($v)` | the same value through a second set of encoder options |
| 4 (`aux` 30) | `$enc->encode_to($fh, $v, $types)` | the PerlIO writer (`encode_json_to()`), into an in-memory filehandle |
| 5 (`aux` 31) | `$dec->decode($out, $found)`, `$enc->encode($v2, $found)` | the types decode finds, fed back to the encoder |

## Why it is tested

The build group creates values JSON text cannot express (dualvars, Inf/NaN,
bignums, code refs, globs, tied containers, blessed objects of every fixture
class) and type specs. Encoding one of them with a second options set covers a
different path for the same value, and decoding the output checks that the
encoder never writes text its own decoder chokes on badly (crashes, not
errors). Steps 0, 1 and 5 run only if the first encode produced output.
