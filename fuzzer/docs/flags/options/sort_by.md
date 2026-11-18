# `sort_by`

| | |
|---|---|
| field | `jx_cfg_t.sort_by`, separately for `dec` and `enc` |
| distribution | set with p = 1/8; then uniform over `@JXF::sort_by` = `(undef, 0, 1, sub { $a cmp $b })` |
| set by | `jx_object()`: `$obj->sort_by($arg)` |

## What it changes

Stores (a copy of) the argument and turns `canonical` on when it is true; the block itself is ignored by this version. Called with `undef`/`0` it frees the stored value.

## Why it is tested

A setter that owns an SV: replacing and freeing it, and `DESTROY` releasing it, are refcount paths.
