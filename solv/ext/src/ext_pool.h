/*
 * ext_pool.h
 *
 */

#ifndef OMNI_SCHEDULER_EXT_POOL_H
#define OMNI_SCHEDULER_EXT_POOL_H

#include "pool.h"

#ifdef __cplusplus
extern "C" {
#endif

extern Pool * ext_pool_create();

extern void ext_pool_createwhatprovides(Pool *pool, int unorderedrepos);

#ifdef __cplusplus
}
#endif

#endif /* OMNI_SCHEDULER_EXT_POOL_H */

