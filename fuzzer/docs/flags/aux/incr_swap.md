# `AUX_INCR_SWAP`: switch `utf8` between chunks

| | |
|---|---|
| bit | `aux` 9 |
| used by | `test_incr` |

## What it does

When set, before every third chunk (chunk index 2, 5, 8, ...) `t_incr` flips
the decoder's `utf8` option:

```perl
$dec->utf8(!$dec->get_utf8);
```

## Why it is tested

When the next `incr_parse` sees that the buffer's encoding no longer matches
the object's `utf8` setting, it converts the buffer (upgrading or
downgrading it) while keeping its scan position. Positions are byte offsets,
so a conversion that changes the byte length of the text before the position
has to adjust it. The documentation allows switching between calls, so this
is a legitimate, if unusual, use.

Turning `utf8` on for a buffer that holds characters above 0xFF makes the
downgrade fail with "Wide character", which covers that error path.
