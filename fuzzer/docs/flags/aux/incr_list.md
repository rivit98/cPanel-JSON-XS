# `AUX_INCR_LIST`: list or scalar context

| | |
|---|---|
| bit | `aux` 6 |
| used by | `test_incr` |

## What it does

- **1, list context:** `my @v = $dec->incr_parse($part)`. Cpanel::JSON::XS appends the
  chunk and returns every complete value in the buffer. On an error,
  `incr_skip` is called to drop the bad text.
- **0, void then scalar context:** `$dec->incr_parse($part)` in void context
  only appends; then `$dec->incr_parse` is called in scalar context up to 64
  times, taking one value each time until it returns `undef`; on an error
  `incr_skip` is called and the loop stops.

## Why it is tested

These are the two documented ways to use the incremental parser, and they go
through different code: list context loops inside XS and builds a return
list, scalar context returns after each value and resumes on the next call.
Both have to leave the buffer and scan state consistent for the next chunk,
including after `incr_skip`.
