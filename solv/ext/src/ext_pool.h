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

extern int ext_pool_is_considered_packages(Pool *pool, Id p);

extern void ext_pool_createwhatprovides(Pool *pool, int unorderedrepos);

extern const char * ext_pool_pkg2reponame(Pool *pool, Id p);
extern const char * ext_pool_pkg2name(Pool *pool, Id p);
extern const char * ext_pool_pkg2srcname(Pool *pool, Id p);

#ifdef __cplusplus
}
#endif

#endif /* OMNI_SCHEDULER_EXT_POOL_H */

