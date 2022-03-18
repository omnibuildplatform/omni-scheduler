/*
 * ext_pool.c
 *
 * extend libsolv pool
 *
 */

#include "repo.h"
#include "common.h"
#include "ext_pool.h"

void create_considered(Pool *pool, Repo *repoonly, Map *considered, int unorderedrepos);

/*
 * create what providers
 */
void
ext_pool_createwhatprovides(Pool *pool, int unorderedrepos)
{
  if (pool->considered)
  {
    map_free(pool->considered);
    solv_free(pool->considered);
  }
  pool->considered = solv_calloc(sizeof(Map), 1);

  create_considered(pool, 0, pool->considered, unorderedrepos);

  pool_createwhatprovides(pool);
}

Pool *
ext_pool_create()
{
  Pool *pool = pool_create();

  set_disttype(pool, DISTTYPE_RPM);

  pool_str2id(pool, ID_BUILDSERVICE_ID, 1);
  pool_str2id(pool, ID_BUILDSERVICE_REPOCOOKIE, 1);
  pool_str2id(pool, ID_BUILDSERVICE_EXTERNAL, 1);
  pool_str2id(pool, ID_BUILDSERVICE_DODURL, 1);
  pool_str2id(pool, ID_BUILDSERVICE_DODCOOKIE, 1);
  pool_str2id(pool, ID_BUILDSERVICE_DODRESOURCES, 1);
  pool_str2id(pool, ID_BUILDSERVICE_ANNOTATION, 1);
  pool_str2id(pool, ID_BUILDSERVICE_MODULES, 1);
  pool_str2id(pool, ID_DIRECTDEPSEND, 1);

  pool_freeidhashes(pool);

  return pool;
}
