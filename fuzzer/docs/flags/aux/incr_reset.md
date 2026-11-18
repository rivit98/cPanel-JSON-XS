# `AUX_INCR_RESET`: `incr_reset` at the end

| | |
|---|---|
| bit | `aux` 8 |
| used by | `test_incr` |

## What it does

When set, `t_incr` calls `$dec->incr_reset` after all chunks were fed, which
discards the buffer and the parser state, whatever state they are in.

## Why it is tested

Inputs often end in the middle of a value, so this resets a parser that is
partway through. Without the bit, the object is destroyed with its buffer
still full, which covers the `DESTROY` cleanup path instead. Both have to free
the buffer exactly once; the SV leak check and ASAN watch both.
