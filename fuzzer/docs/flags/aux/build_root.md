# `AUX_BUILD_ROOT`: wrap the builder's top-level values

| | |
|---|---|
| bit | `aux` 25 |
| used by | `build_value()`, in `test_encode` (build group) |

## What it does

A builder program can leave one or several values at the top level.

- several values: always wrapped in an array reference;
- exactly one value, bit clear: that very SV is the encoder's input, not a
  copy;
- exactly one value, bit set: it is wrapped in a one-element array.

## Why it is tested

Passing the SV itself keeps its exact flags (a dualvar, a read-only value, a
shared hash key scalar, a tied container reference) at the top level, which
is the point of the builder: a copy would lose some of them. Wrapping it
instead puts the same value one level down, where the encoder reaches it
through the array path, and lets it through when `allow_nonref` is off.
