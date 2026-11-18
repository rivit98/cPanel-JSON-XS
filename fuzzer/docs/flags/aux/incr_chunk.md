# `AUX_INCR_CHUNK`: incremental parser chunk size

| | |
|---|---|
| bits | `aux` 0..5 (6 bits) |
| value | chunk size = 1 + bits, so 1..64 bytes, uniform |
| used by | `test_incr` → `JXF::t_incr($dec, $in, $chunk, ...)` |

## What it does

`t_incr` cuts the input into pieces of `$chunk` characters and feeds them to
`$dec->incr_parse` one at a time, the way a program reading from a socket
would.

## Why it is tested

The incremental parser keeps a scan position and a nesting state between
calls, and has to resume correctly when a chunk ends in the middle of a
string, an escape, a number, a UTF-8 sequence, a `relaxed` comment or a
tagged value. A chunk size of 1 puts a boundary at every position; larger
sizes cover the cases where several values or a whole document arrive in one
chunk. The size is the same for the whole input, so the boundaries are at
fixed positions the mutator can line up with.
