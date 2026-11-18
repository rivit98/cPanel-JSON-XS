# `filter_json_object`

| | |
|---|---|
| field | `jx_cfg_t.filter_object`: 0 = none, otherwise 1 + index into `@JXF::obj_cb` |
| distribution | set with p = 1/4; then uniform over the 6 callbacks |
| set by | `jx_object()`: `$obj->filter_json_object($JXF::obj_cb[i])` |

## What it changes

Decoder: after every JSON object is decoded, the callback is called with the
new hash reference. If it returns nothing, the hash is kept; if it returns
one scalar, that scalar replaces the hash; more than one value is an error.

## The callbacks ([fixture.pl](../../../../src/fuzz/fixture.pl))

| # | body | covers |
|---|---|---|
| 1 | `sub { () }` | keep the hash |
| 2 | `sub { $_[0] }` | replace the hash with itself (refcount of the same SV) |
| 3 | `sub { "x" }` | replace it with a string |
| 4 | `sub { (1, 2) }` | the "more than one value" error |
| 5 | `sub { die "obj_cb\n" }` | a perl exception thrown from inside the decoder |
| 6 | `sub { [ sort keys %{ $_[0] } ] }` | replace it with a new structure built from the old one |

## Why it is tested

The decoder runs perl code for every object it decodes. That brings in the
usual risks of calling back into perl: the stack may be reallocated, the
callback may die halfway through decoding, and the values it returns must be
reference-counted correctly. Callbacks 4 and 5 test unwinding from deep
inside a partly decoded structure; the SV leak check catches anything left
behind.

## Interactions

- Runs before the single-key filters are considered for that object (see
  [filter_json_single_key_object.md](filter_json_single_key_object.md)).
- Applies to objects created by `allow_tags`' `THAW` values and to every
  decoder call in the test cases, including `incr_parse` and `decode_prefix`.
