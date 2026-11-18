# `canonical`

| | |
|---|---|
| field | `jx_cfg_t.options` bit 5 (`jx_option_names[5]`), separately for `dec` and `enc` |
| distribution | on with p = 1/2, independent of every other option bit (one `rand64() & JXO_ALL` draw) |
| set by | `jx_object()`: `$obj->canonical(1)` or `$obj->canonical(0)`, only when it differs from the state after `new` |
| default after `new` | off |

## What it changes

Encoder only: object keys are sorted before writing, so the output does not depend on hash order.

## Why it is tested

The sorted path is separate code: it collects the hash entries, sorts them and then writes them. It handles tied hashes (`BOP_BEGIN_TIED`, through the iterator rather than the HE array), keys with and without the UTF8 flag (`BOP_KEY` with bit 0x80) and duplicate-looking keys that differ only in encoding.

## Notes

- Hash order is fixed for the whole campaign (`PERL_HASH_SEED=0`, `PERL_PERTURB_KEYS=0`), so the non-canonical path is deterministic too.
- `filter_json_object` callback 5 returns sorted keys, which is unrelated: that runs on the decoder.
