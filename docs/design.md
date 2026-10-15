# Design

`lru-cache` maps 32-bit keys to 32-bit values in a fixed number of entries, evicting the least recently used entry when it is full, with no heap and a known worst-case time. This document describes the recency list, the two lookup strategies, insertion and eviction, the memory footprint, and how the library is verified. The API is in [api.md](api.md).

## Goals

- All memory is in one structure the caller places, and its size is known at compile time.
- Every operation finishes in time bounded by a compile-time constant, with no recursion.
- An operation that fails leaves the cache exactly as it was.
- Portable C11, including targets where `CHAR_BIT` is 16.

Variable-size keys or values, eviction callbacks, expiry by age, and synchronisation are out of scope.

## The recency list

`lru_cache_t` holds:

- `nodes`, an array of `LRU_CACHE_MAX_ENTRIES` nodes, each with a key, a value, and the `int16_t` indices of its neighbours in the list, -1 at either end.
- `head_idx` and `tail_idx`, the most and least recently used nodes, -1 when the cache is empty.
- `capacity`, set by `lru_cache_init`, and `size`, the number of entries held.
- `hash_table`, `LRU_CACHE_HASH_TABLE_SIZE` slots that map keys to node indices under the hash strategy.

The list is linked by array indices, not pointers. The structure can therefore be copied or moved as a whole and stays valid, and no code forms a pointer from arithmetic. The `int16_t` indices are why `LRU_CACHE_MAX_ENTRIES` is limited to 32767.

Every successful `lru_cache_get` and `lru_cache_put` unlinks the node it touched and relinks it at the head, which takes a fixed number of index writes. The tail is therefore always the entry to evict.

Nodes are never freed except by eviction, which refills them at once. While the cache is filling, nodes `0` to `size - 1` are in use and node `size` is the next free one, so no free list is needed. A node that holds no entry has the key `LRU_CACHE_INVALID_KEY`, which is why that key cannot be stored.

## Lookup strategies

The strategy decides how a key is found. It is fixed when `src/lru_cache.c` is compiled, and the list behaves the same under both.

The linear strategy compares the key with nodes `0` to `capacity - 1` in turn. A lookup costs at most `capacity` comparisons, every valid put succeeds, and the hash table is allocated but not used.

The hash strategy computes `(key * 0x9E3779B9) % LRU_CACHE_HASH_TABLE_SIZE` in 32-bit unsigned arithmetic, a multiplicative hash that spreads nearby keys across the table, and probes from that slot to the following ones, wrapping at the end. A slot holds a node index, -1 for empty, or a tombstone. A lookup stops at the first empty slot or after `LRU_CACHE_MAX_PROBES` slots, so it costs at most that many slot reads and key comparisons whatever the capacity.

### Tombstones

Evicting an entry cannot simply empty its slot, because a key inserted later in the same probe sequence would then be cut off from its hash: a lookup would stop at the empty slot and miss it. The slot becomes a tombstone instead. A lookup steps over a tombstone, counting it as a probe, and an insert may reuse it.

Tombstones that end a probe sequence carry no information. After an eviction, if the slot after the new tombstone is empty, the tombstone becomes empty, and so do up to `LRU_CACHE_MAX_PROBES - 1` tombstones immediately before it. This keeps tombstones from filling the table under steady eviction.

### Probe window and rejected inserts

A new key must be placed within `LRU_CACHE_MAX_PROBES` slots of its hash, so that every lookup can find it within the same bound. If every slot in that window holds a live entry, other than the one about to be evicted, `lru_cache_put` returns false and changes nothing. How often this happens depends on how full the table is, and the default 17 slots for 16 entries is nearly full once the cache is:

| Entries / table slots / probes | Keys 1 to 16 into an empty cache | New random keys rejected when full |
| --- | --- | --- |
| 16 / 17 / 4 (defaults) | 10 held | 59% |
| 16 / 37 / 4 | 16 held | 4.4% |
| 16 / 67 / 4 | 16 held | 0.56% |
| 16 / 37 / 8 | 16 held | 0.24% |
| 16 / 17 / 17 | 16 held | 0% |

The rates are from 10^6 pseudo-random keys on x86-64; they depend only on the hash and the configuration, not on the target. A rejected put is safe, because the cache never returns a wrong value, but the cache then holds fewer entries than its capacity. Size the table at about twice the entry count, raise the probe limit, or use the linear strategy when every insert must succeed. A probe limit equal to the table size never rejects, at the cost of a worst case that grows with the table.

## Insertion and eviction

`lru_cache_put` first looks the key up. If it is present, the value is replaced and the node moved to the head.

For a new key, the insert runs in two phases. The first reads only: it decides whether an eviction is needed, finds the victim's hash slot by probing from the victim's hash, and finds a slot for the new key in its probe window. A slot in the window qualifies if it is empty, a tombstone, or the victim's slot, which the second phase frees. If no slot qualifies, the function returns false at this point, before anything is written. The second phase writes: it tombstones the victim's slot and clears trailing tombstones, unlinks the victim and clears its key, writes the new key and value into the freed or next unused node, records the node in the table, and links it at the head. Nothing in the second phase can fail.

Under the linear strategy the first phase only chooses the node, so the insert always completes.

## Worst-case cost

Every loop is bounded by a configuration constant:

| Operation | Linear strategy | Hash strategy |
| --- | --- | --- |
| `lru_cache_init` | `LRU_CACHE_MAX_ENTRIES` node writes | the same, plus `LRU_CACHE_HASH_TABLE_SIZE` slot writes |
| `lru_cache_get` | `capacity` key comparisons | `LRU_CACHE_MAX_PROBES` slot reads and key comparisons |
| `lru_cache_put` | `capacity` key comparisons | four probe loops of at most `LRU_CACHE_MAX_PROBES` steps: lookup, victim slot, insert slot, tombstone clean-up |

List updates add a constant number of index writes to both.

## Footprint

`sizeof(lru_cache_t)` is four 16-bit fields, `LRU_CACHE_MAX_ENTRIES` nodes of two `uint32_t` and two `int16_t` each, and `LRU_CACHE_HASH_TABLE_SIZE` `int16_t` slots, padded to the alignment of `uint32_t`. With the defaults that is 236 bytes on x86-64: 8 of fields, 192 of nodes, 34 of table and 2 of padding.

On a target where `CHAR_BIT` is 16, `sizeof` counts 16-bit words and a `uint32_t` takes two. A node is then 6 words, and the default structure is 118 words, the same 236 octets. The table is allocated under both strategies, so a linear build can save its space only by setting `LRU_CACHE_HASH_TABLE_SIZE` to `LRU_CACHE_MAX_ENTRIES`, the smallest value the header accepts.

`LRU_CACHE_MAX_ENTRIES` and `LRU_CACHE_HASH_TABLE_SIZE` change this layout, so the library and its callers must be compiled with the same values. [integration.md](integration.md#configuration) shows how.

## Concurrency

The library takes no locks and keeps no global state, so separate caches are independent. A single cache is not safe for concurrent use: `lru_cache_get` relinks the node it finds, so even two lookups on the same cache race. Every call on a cache shared between contexts, including an interrupt handler, needs the caller's own exclusion, such as a mutex or masked interrupts, around the whole call.

## Failure behaviour

| Condition | Result |
| --- | --- |
| NULL `cache_ptr` to `lru_cache_init` | Nothing is done. |
| NULL `cache_ptr` or `out_value`, or the key `LRU_CACHE_INVALID_KEY` | `false`; nothing is read or written. |
| Key absent on `lru_cache_get` | `false`; `*out_value` and the recency order are unchanged. |
| No slot in the probe window on a new key (hash strategy) | `false` from `lru_cache_put`; the cache is unchanged, and the key is not cached. |
| Capacity of 0 or above `LRU_CACHE_MAX_ENTRIES` | Treated as 1, or as `LRU_CACHE_MAX_ENTRIES`. |
| Cache used before `lru_cache_init` | Undefined; the indices hold arbitrary values. |
| Two contexts calling on the same cache at once | Undefined; the caller must serialise them. |
| Library and caller built with different layouts | Undefined; the library reads and writes outside the caller's structure. |
| Invalid configuration | Compile error from a `_Static_assert`. |

## MISRA C:2023

The library is analysed with [`misch`](https://pypi.org/project/misch/), a cppcheck-based MISRA C:2023 checker, over `include/` and `src/`, once for each lookup strategy. Each run has its own baseline of accepted findings in `analysis/baseline/`, and CI fails on any finding outside it. The version macros in `lru_cache.h` carry an inline deviation of Rule 2.5 (unused macro), because they exist for consumers and the library does not use them.

| Rule | Hash | Linear | Cause |
| --- | --- | --- | --- |
| 15.5 | 9 | 6 | Early returns, which reject invalid arguments and end probe loops. |
| 12.1 | 6 | 3 | Comparisons joined by `\|\|` or `&&` without parentheses. |
| 10.7 | 5 | 0 | `(slot + 1U) % (uint32_t)LRU_CACHE_HASH_TABLE_SIZE` in the hash probing code, where the 16-bit composite operand meets a wider divisor. |
| 8.7 | 3 | 3 | Public functions with no caller outside `src/lru_cache.c` in the analysed code. |
| 2.5 | 1 | 1 | The strategy identifier the build does not use. |

The baselines record the findings as they stand. Formal compliance needs a certified analyser and a deviation record from the consuming project.

## Verification

The tests run on the host:

| Test | What it checks |
| --- | --- |
| `lru_cache / hash strategy` | Initialisation, hits and misses, eviction order, update in place, promotion on lookup, NULL arguments, the reserved key, capacity clamping, filling to capacity and sequential eviction; and for the hash strategy, sequential integer keys, collisions followed by eviction, an eviction whose victim is reached on the last probe, rejected inserts with and without eviction leaving the cache unchanged, tombstone clean-up under repeated eviction, eviction from a collided run, and tombstone reuse. |
| `lru_cache / linear strategy` | The same tests, without the hash-specific ones, against a library built with the linear strategy. |
| `lru_cache docs examples` | Every C block in the README, `CONTRIBUTING.md` and `docs/` compiles against the public header. |

CI runs every test under AddressSanitizer and UndefinedBehaviorSanitizer, gates line and branch coverage of `src/` and `include/` at 100 percent across both strategies, builds the library in release mode for each strategy, checks formatting, and runs the MISRA analysis for each strategy. It also compiles the library with warnings as errors under TI C2000 CGT 22.6.2 and 25.11.1, in both the COFF and EABI formats and for both strategies.
