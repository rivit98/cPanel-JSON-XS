# `subclass`

| | |
|---|---|
| field | `jx_cfg_t.subclass`, separately for `dec` and `enc` |
| distribution | p = 1/8 |
| set by | `jx_object()`: `JXF::Sub->new` instead of `Cpanel::JSON::XS->new`; `JXF::Sub` is `@ISA = ('Cpanel::JSON::XS')` |

## What it changes

The object is blessed into a subclass. Every method's typemap check then goes through `sv_derived_from()` instead of the stash comparison.

## Why it is tested

The typemap's slow path, and `new` with a class other than its own.
