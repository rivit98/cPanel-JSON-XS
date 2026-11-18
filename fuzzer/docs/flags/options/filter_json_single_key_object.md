# `filter_json_single_key_object`

| | |
|---|---|
| fields | `jx_cfg_t.filter_sk_keys` (bit i: install a filter for `$JXF::sk_keys[i]`) and `jx_cfg_t.filter_sk_cb` (rotation of the callbacks) |
| distribution | the key mask is drawn with p = 1/4, otherwise 0; when drawn, it is 6 random bits, so each key is filtered with p = 1/8 overall and at least one key with p ≈ 0.246 |
| set by | `jx_object()`: for each set bit i, `$obj->filter_json_single_key_object($sk_keys[i], $sk_cb[(filter_sk_cb + i) % 5])` |

`filter_sk_cb` is always drawn (uniform 0..4) but only matters when keys are
selected. Rotating by the key index gives different keys different callbacks
on the same object.

## What it changes

Decoder: for an object with exactly one key, if a filter is registered for
that key, the callback is called with the key's value. If it returns one
value, that value replaces the whole object; nothing means keep; more than
one is an error.

## Keys

`""`, `"a"`, `"__class__"`, `"\x{263a}"` (a non-Latin-1 key, so the lookup
has to match a UTF-8 key), `"$type"`, `"1"`. `json.dict` and the json seeds
contain single-key objects with these keys.

## Callbacks

| # | body | covers |
|---|---|---|
| 0 | `sub { () }` | keep the object |
| 1 | `sub { $_[0] }` | replace the object with its value |
| 2 | `sub { [ $_[0], $_[0] ] }` | the same value referenced twice |
| 3 | `sub { (1, 2) }` | the "more than one value" error |
| 4 | `sub { die "sk_cb\n" }` | exception from inside the decoder |

## Why it is tested

The lookup takes a hash key that may or may not be UTF-8 and finds it in the
filter hash; the callback has all the risks described in
[filter_json_object.md](filter_json_object.md). Registering several keys on
one object also covers the code that adds entries to the filter hash.
