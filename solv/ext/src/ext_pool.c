/*
 * ext_pool.c
 *
 * extend libsolv pool
 *
 */

#include "ext_pool.h"
#include "common.h"

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

extern void create_considered(Pool *pool, Repo *repoonly, Map *considered, int unorderedrepos);

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

int
ext_pool_is_considered_packages(Pool *pool, Id p)
{
  return (MAPTST(pool->considered, p)) ? 1 : 0;
}

const char *
ext_pool_pkg2reponame(Pool *pool, Id p)
{
  Repo *repo = pool->solvables[p].repo;
  return repo ? repo->name : 0;
}

const char *
ext_pool_pkg2name(Pool *pool, Id p)
{
  return pool_id2str(pool, pool->solvables[p].name);
}

const char *
ext_pool_pkg2srcname(Pool *pool, Id p)
{
  if (solvable_lookup_void(pool->solvables + p, SOLVABLE_SOURCENAME))
    return pool_id2str(pool, pool->solvables[p].name);

  return solvable_lookup_str(pool->solvables + p, SOLVABLE_SOURCENAME);
}
