# `AUX_PREFIX_N`: `decode_prefix` rounds

| | |
|---|---|
| bits | `aux` 10..13 (4 bits) |
| value | `$rounds` 0..15; the loop runs at most `$rounds + 1` times (1..16) |
| used by | `test_decode_prefix` → `JXF::t_decode_prefix($dec, $in, $rounds)` |

## What it does

```perl
for (0 .. $rounds) {
  last unless length $in;
  my ($v, $off) = $dec->decode_prefix($in);   # in eval
  last if $@ || !$off;
  substr($in, 0, $off, "");
}
```

Each round decodes one JSON text from the front of the input and cuts off the
number of characters it consumed.

## Why it is tested

`decode_prefix` returns the length it consumed, in characters, and the
caller uses it to advance. If the returned offset is wrong (for example, bytes
in `utf8` mode versus characters, or after trailing whitespace), the next
round starts in the wrong place. Several rounds over a stream of values (the
json seeds contain such streams) test that the offset is right each time.
