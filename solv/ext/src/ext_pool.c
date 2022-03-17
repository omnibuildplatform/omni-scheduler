/*
 * repo_solv.c
 *
 * Add a repo in solv format
 *
 */

#include "ext_pool.h"

/*
 * create what providers
 */
void
ext_pool_createwhatprovides(Pool *pool, int unorderedrepos)
{
  pool_createwhatprovides(pool);
}
