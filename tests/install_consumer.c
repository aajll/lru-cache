/**
 * SPDX-License-Identifier: MIT
 *
 * @file install_consumer.c
 * @brief Consumer built from an installed package with the flags from
 *        `pkg-config --cflags --libs lru-cache`.
 */

#include "lru_cache.h"

#include <stdint.h>

static lru_cache_t cache;

int
main(void)
{
        uint32_t value = 0U;

        lru_cache_init(&cache, 4U);
        if (!lru_cache_put(&cache, 7U, 42U)) {
                return 1;
        }
        if (!lru_cache_get(&cache, 7U, &value) || (value != 42U)) {
                return 2;
        }
        return 0;
}
