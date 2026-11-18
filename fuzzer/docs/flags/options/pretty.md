# `pretty`

| | |
|---|---|
| field | `jx_cfg_t.pretty`, separately for `dec` and `enc` |
| distribution | p = 1/8 (`(r & 7) == 0`, the last field drawn from `r`) |
| set by | `jx_object()`: `$obj->pretty(1)` first, before any other setter |

## What it changes

`pretty` is shorthand that turns on `indent`, `space_before` and `space_after` at once.

## How the harness applies it

`jx_object()` calls `->pretty(1)` and then, for those three options, treats
"on" as the current state. The option bits still decide the final state: a
bit that is clear is turned off again with `->indent(0)` and so on. So the
encoder output is always defined by the three option bits; `pretty` only adds
a call sequence:

```perl
my $o = Cpanel::JSON::XS->new->pretty(1);
$o->space_before(0);   # when bit 5 is clear
```

## Why it is tested

It covers the `pretty` method itself, a separate XS entry point that changes
several flags at once, and the case where flags it set are cleared again
afterwards. The output is the same as setting the three flags directly, so
nothing is lost by keeping it rare.
