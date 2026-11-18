# `AUX_INCR_TEXT`: assign `incr_text` before parsing

| | |
|---|---|
| bit | `aux` 7 |
| used by | `test_incr` |

## What it does

When set, `t_incr` first assigns the first chunk directly to the parser's
buffer, then removes it from the input and feeds the rest as usual:

```perl
$dec->incr_text = substr($in, 0, $chunk);
```

At the end of every run, `t_incr` also reads `incr_text` once, whatever this
bit says. That read croaks if parsing is in progress (the documented
restriction), so it covers the error path too.

## Why it is tested

`incr_text` is an lvalue accessor. Assigning to it replaces the buffer from
the outside, including its UTF8 flag, while the parser state was set up for
an empty buffer. This covers the documented way to prime or edit the buffer.
