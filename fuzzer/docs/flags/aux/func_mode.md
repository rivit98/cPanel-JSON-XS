# `AUX_FUNC_MODE`: the functional interface variant

| | |
|---|---|
| bits | `aux` 32..33 (2 bits) |
| used by | `test_functional` → `JXF::t_functional($in, $mode)` |

| mode | call |
|---|---|
| 0 | `decode_json($in)`, then `encode_json($v)` |
| 1 | `decode_json($in, 0)`: the optional `allow_nonref` argument switching it off |
| 2 | `decode_json($in, 1, $types)`: with the type output argument |
| 3 | `_from_json($in)` / `_to_json($v)`: the non-utf8 ALIAS of the same XSUBs |

## Why it is tested

The functional XSUBs build a `JSON` struct on the C stack (`json_init()`) and
take optional arguments the method interface does not have.
