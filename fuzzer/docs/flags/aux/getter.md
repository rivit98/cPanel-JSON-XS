# `AUX_GETTER`: which `get_*` is called

| | |
|---|---|
| bits | `aux` 20..24 (5 bits) |
| value | index into `jx_getter_names[]`, 29 entries; 5 bits over 29 entries, so the first three are drawn twice as often |
| used by | `jx_object()`, on every object it configures (`dec` and `enc` get the same getter) |

## What it does

After configuring an object, `jx_object()` calls one getter on it: the 25
boolean `get_*` ALIASes (one per option in `jx_option_names`),
`get_indent_length`, `get_max_depth`, `get_max_size` or `get_stringify_infnan`.

## Why it is tested

The getters are XS ALIASes that share one function and select the flag by
`ix`; `get_indent_length`, `get_max_depth`, `get_max_size` and `get_stringify_infnan` are separate functions. One call per
object keeps them covered at almost no cost, and checks that a getter works
on an object in any configuration. `t_incr` also calls `get_utf8` when
`AUX_INCR_SWAP` is set.
