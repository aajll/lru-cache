/**
 * SPDX-License-Identifier: MIT
 *
 * @file: lru_cache.c
 *
 * @brief
 *    Implementation of the fixed-size LRU cache.
 *
 * Deviation: MISRA C:2023 Rule 15.5 (Advisory) — multiple return statements.
 * Rationale: Each public function uses early-return for precondition checks.
 * Refactoring to a single exit point would require deeply nested
 * conditionals or goto-cleanup, reducing readability without safety benefit.
 */

#include "lru_cache.h"

#include <stddef.h>

/* -------------------------------------------------------------------------
 * Internal helpers
 * ------------------------------------------------------------------------- */

/**
 * @brief Map a key to a hash-table slot index.
 *
 * Uses a multiplicative (Fibonacci) hash to spread keys across the table.
 * The result is in [0, LRU_CACHE_HASH_TABLE_SIZE).
 *
 * @param key  The input key.
 * @return     Hash table index in range [0, LRU_CACHE_HASH_TABLE_SIZE - 1].
 */
#if (LRU_CACHE_LOOKUP_STRATEGY == LRU_CACHE_LOOKUP_HASH)
static uint16_t
compute_hash(uint32_t key)
{
        /* Fibonacci / golden-ratio multiplier -- good avalanche for 32-bit keys
         */
        const uint32_t hash_prime = 0x9E3779B9U;
        /*
         * Use modulo (not bitwise AND) so the distribution is correct for
         * any table size, including non-powers-of-two (e.g. the default
         * prime of 17).  Both operands are unsigned, so this is
         * MISRA-compliant.
         */
        return (uint16_t)((key * hash_prime)
                          % (uint32_t)LRU_CACHE_HASH_TABLE_SIZE);
}
#endif

/**
 * @brief Detach a node from the doubly-linked LRU list.
 *
 * @param cache_ptr  Cache instance.
 * @param node_idx   Index of a linked node, in [0, LRU_CACHE_MAX_ENTRIES).
 */
static void
remove_from_list(lru_cache_t *cache_ptr, int16_t node_idx)
{
        int16_t prev = cache_ptr->nodes[node_idx].prev_idx;
        int16_t next = cache_ptr->nodes[node_idx].next_idx;

        if (prev != -1) {
                cache_ptr->nodes[prev].next_idx = next;
        } else {
                cache_ptr->head_idx = next; /* was the head */
        }

        if (next != -1) {
                cache_ptr->nodes[next].prev_idx = prev;
        } else {
                cache_ptr->tail_idx = prev; /* was the tail */
        }
}

/**
 * @brief Insert a node at the front (MRU end) of the doubly-linked list.
 *
 * @param cache_ptr  Cache instance.
 * @param node_idx   Index of an unlinked node, in [0, LRU_CACHE_MAX_ENTRIES).
 */
static void
add_to_front(lru_cache_t *cache_ptr, int16_t node_idx)
{
        cache_ptr->nodes[node_idx].next_idx = cache_ptr->head_idx;
        cache_ptr->nodes[node_idx].prev_idx = -1;

        if (cache_ptr->head_idx != -1) {
                cache_ptr->nodes[cache_ptr->head_idx].prev_idx = node_idx;
        } else {
                cache_ptr->tail_idx = node_idx; /* list was empty */
        }

        cache_ptr->head_idx = node_idx;
}

/**
 * @brief Locate the node index for a key using the configured strategy.
 *
 * @param cache_ptr  Cache instance (const; LRU list not mutated here).
 * @param key        Key to find.
 * @return           Node index on hit, -1 on miss.
 */
static int16_t
find_node_index(const lru_cache_t *cache_ptr, uint32_t key)
{
#if (LRU_CACHE_LOOKUP_STRATEGY == LRU_CACHE_LOOKUP_HASH)
        uint16_t hash_idx = compute_hash(key);
        uint16_t probe_count = 0U;

        while (probe_count < LRU_CACHE_MAX_PROBES) {
                int16_t stored_idx = cache_ptr->hash_table[hash_idx];

                if (stored_idx == (int16_t)-1) {
                        return -1; /* true empty slot -- key absent */
                }

                if (stored_idx == LRU_CACHE_HASH_TOMBSTONE) {
                        /* tombstone from eviction -- continue probing */
                        hash_idx =
                            (uint16_t)((hash_idx + 1U)
                                       % (uint32_t)LRU_CACHE_HASH_TABLE_SIZE);
                        probe_count++;
                        continue;
                }

                /* Any other slot value is a node index. */
                if (cache_ptr->nodes[stored_idx].key == key) {
                        return stored_idx;
                }

                /* Linear probe to next slot (wraps around). */
                hash_idx = (uint16_t)((hash_idx + 1U)
                                      % (uint32_t)LRU_CACHE_HASH_TABLE_SIZE);
                probe_count++;
        }
        return -1; /* probe limit reached */

#else /* LRU_CACHE_LOOKUP_LINEAR */
        uint16_t i = 0U;
        while (i < cache_ptr->capacity) {
                if (cache_ptr->nodes[i].key == key) {
                        return (int16_t)i;
                }
                i++;
        }
        return -1;
#endif
}

/* -------------------------------------------------------------------------
 * Public API
 * ------------------------------------------------------------------------- */

void
lru_cache_init(lru_cache_t *cache_ptr, uint16_t capacity)
{
        if (cache_ptr == NULL) {
                return;
        }

        if (capacity > LRU_CACHE_MAX_ENTRIES) {
                cache_ptr->capacity = (uint16_t)LRU_CACHE_MAX_ENTRIES;
        } else if (capacity == 0U) {
                cache_ptr->capacity = 1U;
        } else {
                cache_ptr->capacity = capacity;
        }
        cache_ptr->size = 0U;
        cache_ptr->head_idx = -1;
        cache_ptr->tail_idx = -1;

        /* Initialise all node slots (Rule 9.1: no uninitialised reads). */
        for (uint16_t i = 0U; i < LRU_CACHE_MAX_ENTRIES; i++) {
                cache_ptr->nodes[i].key = LRU_CACHE_INVALID_KEY;
                cache_ptr->nodes[i].value = 0U;
                cache_ptr->nodes[i].prev_idx = -1;
                cache_ptr->nodes[i].next_idx = -1;
        }

#if (LRU_CACHE_LOOKUP_STRATEGY == LRU_CACHE_LOOKUP_HASH)
        /* Mark every hash-table slot as empty. */
        for (uint16_t i = 0U; i < LRU_CACHE_HASH_TABLE_SIZE; i++) {
                cache_ptr->hash_table[i] = -1;
        }
#endif
}

bool
lru_cache_get(lru_cache_t *cache_ptr, uint32_t key, uint32_t *out_value)
{
        if (cache_ptr == NULL || out_value == NULL
            || key == LRU_CACHE_INVALID_KEY) {
                return false;
        }

        int16_t node_idx = find_node_index(cache_ptr, key);
        if (node_idx == -1) {
                return false;
        }

        *out_value = cache_ptr->nodes[node_idx].value;

        remove_from_list(cache_ptr, node_idx);
        add_to_front(cache_ptr, node_idx);

        return true;
}

/**
 * @brief Convert trailing tombstones to empty slots after an eviction.
 *
 * When a tombstone's successor slot is empty (-1), the tombstone itself
 * can safely become empty because no probe chain crosses it.  We then
 * walk backward and convert any consecutive preceding tombstones that
 * now also trail into empty space.  Bounded to MAX_PROBES iterations
 * for deterministic WCET.
 *
 * @param cache_ptr       Cache instance.
 * @param tombstone_slot  The slot that was just set to TOMBSTONE.
 */
#if (LRU_CACHE_LOOKUP_STRATEGY == LRU_CACHE_LOOKUP_HASH)
static void
cleanup_trailing_tombstones(lru_cache_t *cache_ptr, uint16_t tombstone_slot)
{
        /* Check if the slot AFTER the tombstone is empty. */
        uint16_t next_slot = (uint16_t)((tombstone_slot + 1U)
                                        % (uint32_t)LRU_CACHE_HASH_TABLE_SIZE);

        if (cache_ptr->hash_table[next_slot] != (int16_t)-1) {
                return; /* successor is occupied; tombstone must stay */
        }

        /* Convert this tombstone to empty. */
        cache_ptr->hash_table[tombstone_slot] = (int16_t)-1;

        /* Walk backward converting consecutive tombstones to empty. */
        uint16_t steps = 1U;
        uint16_t prev_slot =
            (uint16_t)((tombstone_slot + (uint32_t)LRU_CACHE_HASH_TABLE_SIZE
                        - 1U)
                       % (uint32_t)LRU_CACHE_HASH_TABLE_SIZE);

        while (steps < LRU_CACHE_MAX_PROBES
               && cache_ptr->hash_table[prev_slot]
                      == LRU_CACHE_HASH_TOMBSTONE) {
                cache_ptr->hash_table[prev_slot] = (int16_t)-1;
                prev_slot =
                    (uint16_t)((prev_slot + (uint32_t)LRU_CACHE_HASH_TABLE_SIZE
                                - 1U)
                               % (uint32_t)LRU_CACHE_HASH_TABLE_SIZE);
                steps++;
        }
}
#endif

bool
lru_cache_put(lru_cache_t *cache_ptr, uint32_t key, uint32_t value)
{
        if (cache_ptr == NULL || key == LRU_CACHE_INVALID_KEY) {
                return false;
        }

        /* --- Update path: key already present ----------------------------- */
        int16_t node_idx = find_node_index(cache_ptr, key);
        if (node_idx != -1) {
                cache_ptr->nodes[node_idx].value = value;
                remove_from_list(cache_ptr, node_idx);
                add_to_front(cache_ptr, node_idx);
                return true;
        }

        /* =================================================================
         * Insert path — two-phase commit
         *
         * PHASE 1: Pre-check (read-only).  Determine all positions and
         *          indices needed for insertion.  If any step fails,
         *          return false with zero state modifications.
         *
         * PHASE 2: Commit (all mutations, guaranteed to succeed).
         * ================================================================= */

        bool needs_eviction = (cache_ptr->size >= cache_ptr->capacity);
        int16_t lru_idx = -1;
        int16_t free_idx = -1;

        /* --- PHASE 1: Pre-check ------------------------------------------ */

#if (LRU_CACHE_LOOKUP_STRATEGY == LRU_CACHE_LOOKUP_HASH)
        uint16_t evict_hash_slot = 0U;
        uint16_t insert_hash_slot = 0U;
#endif

        if (needs_eviction) {
                /* A full cache holds at least one entry, so the tail is a
                 * node. */
                lru_idx = cache_ptr->tail_idx;

#if (LRU_CACHE_LOOKUP_STRATEGY == LRU_CACHE_LOOKUP_HASH)
                /*
                 * Locate the eviction victim's hash slot (don't modify it).
                 * Insertion placed it within MAX_PROBES slots of its hash,
                 * so the slot reached on the last probe is the victim's.
                 */
                evict_hash_slot = compute_hash(cache_ptr->nodes[lru_idx].key);
                uint16_t evict_probe = 1U;

                while ((evict_probe < LRU_CACHE_MAX_PROBES)
                       && (cache_ptr->hash_table[evict_hash_slot] != lru_idx)) {
                        evict_hash_slot =
                            (uint16_t)((evict_hash_slot + 1U)
                                       % (uint32_t)LRU_CACHE_HASH_TABLE_SIZE);
                        evict_probe++;
                }
#endif

                /* Reuse the evicted node's slot. */
                free_idx = lru_idx;
        } else {
                /* Nodes are only freed by an eviction that refills them, so
                 * nodes [0, size) are in use and nodes[size] is free. */
                free_idx = (int16_t)cache_ptr->size;
        }

#if (LRU_CACHE_LOOKUP_STRATEGY == LRU_CACHE_LOOKUP_HASH)
        /*
         * Probe for the new key's hash insertion slot.
         * Accept: empty (-1), tombstone, or the eviction victim's slot
         * (which will be freed in the commit phase).
         */
        uint16_t hash_idx = compute_hash(key);
        uint16_t insert_probe = 0U;
        bool insert_slot_found = false;

        while (insert_probe < LRU_CACHE_MAX_PROBES) {
                int16_t slot_val = cache_ptr->hash_table[hash_idx];

                if (slot_val == (int16_t)-1
                    || slot_val == LRU_CACHE_HASH_TOMBSTONE
                    || (needs_eviction && hash_idx == evict_hash_slot)) {
                        insert_hash_slot = hash_idx;
                        insert_slot_found = true;
                        break;
                }
                hash_idx = (uint16_t)((hash_idx + 1U)
                                      % (uint32_t)LRU_CACHE_HASH_TABLE_SIZE);
                insert_probe++;
        }

        if (!insert_slot_found) {
                return false; /* probe exhausted — no state was modified */
        }
#endif

        /* --- PHASE 2: Commit (all mutations, guaranteed to succeed) ------ */

        if (needs_eviction) {
#if (LRU_CACHE_LOOKUP_STRATEGY == LRU_CACHE_LOOKUP_HASH)
                /* Tombstone old hash slot. */
                cache_ptr->hash_table[evict_hash_slot] =
                    LRU_CACHE_HASH_TOMBSTONE;

                /* Clean up trailing tombstones to prevent accumulation. */
                cleanup_trailing_tombstones(cache_ptr, evict_hash_slot);
#endif

                /* Remove victim from LRU list and clear its key. */
                remove_from_list(cache_ptr, lru_idx);
                cache_ptr->nodes[lru_idx].key = LRU_CACHE_INVALID_KEY;
                /* size stays the same — replacing one item */
        } else {
                cache_ptr->size++;
        }

        /* Write key/value to the node. */
        cache_ptr->nodes[free_idx].key = key;
        cache_ptr->nodes[free_idx].value = value;

#if (LRU_CACHE_LOOKUP_STRATEGY == LRU_CACHE_LOOKUP_HASH)
        /* Write node index into the hash table. */
        cache_ptr->hash_table[insert_hash_slot] = free_idx;
#endif

        /* Link at MRU position. */
        add_to_front(cache_ptr, free_idx);

        return true;
}
