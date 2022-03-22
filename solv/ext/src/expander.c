/*
 * expander.c
 *
 * expander of pool
 *
 */

#include "expander.h"
#include "common.h"
#include "util.h"

Expander *
new_expander(Pool *pool, int debug, int options)
{
  Expander *xp;

  xp = (Expander *)solv_calloc(1, sizeof(*xp));

  xp->pool = pool;
  xp->debug = debug;
  xp->ignoreignore = options & EXPANDER_OPTION_IGNOREIGNORE ? 1 : 0;
  xp->ignoreconflicts = options & EXPANDER_OPTION_IGNORECONFLICTS ? 1 : 0;
  xp->userecommendsforchoices = options & EXPANDER_OPTION_USERECOMMENDSFORCHOICES ? 1 : 0;
  xp->usesupplementsforchoices = options & EXPANDER_OPTION_USESUPPLEMENTSFORCHOICES ? 1 : 0;
  xp->dorecommends = options & EXPANDER_OPTION_DORECOMMENDS ? 1 : 0;
  xp->dosupplements = options & EXPANDER_OPTION_DOSUPPLEMENTS ? 1 : 0;

  queue_init(&xp->preferposq);
  queue_init(&xp->conflictsq);

  return xp;
}

/*
 * initialize the prefer of expander
 */
void
expander_init_prefer(Expander *xp, char *pkg)
{
  if (!pkg)
    return;

  Id id = 0;
  char *str = 0;
  Pool *pool = xp->pool;

  if (*pkg == '-')
  {
    id = pool_str2id(pool, pkg + 1, 1);
    MAPEXP(&xp->preferneg, id);
    MAPSET(&xp->preferneg, id);

    if ((str = strchr(pkg, ':')) != 0)
      {
        id = str2id_dup(pool, str + 1);
        MAPEXP(&xp->prefernegx, id);
        MAPSET(&xp->prefernegx, id);
      }
  }
  else
  {
    id = pool_str2id(pool, pkg, 1);

    queue_push(&xp->preferposq, id);

    MAPEXP(&xp->preferpos, id);
    MAPSET(&xp->preferpos, id);

    if ((str = strchr(pkg, ':')) != 0)
      {
        id = str2id_dup(pool, str + 1);
        MAPEXP(&xp->preferposx, id);
        MAPSET(&xp->preferposx, id);
      }
  }
}

/*
 * initialize the ignore of expander
 */
void
expander_init_ignore(Expander *xp, char *pkg)
{
  if (!pkg)
    return;

  Pool *pool = xp->pool;

  Id id = pool_str2id(pool, pkg, 1);
  MAPEXP(&xp->ignored, id);
  MAPSET(&xp->ignored, id);

  char *str = 0;
  if ((str = strchr(pkg, ':')) != 0)
  {
    id = str2id_dup(pool, str + 1);
    MAPEXP(&xp->ignoredx, id);
    MAPSET(&xp->ignoredx, id);
  }
}

static void
expander_set_conflict(Expander *xp, Id id, Id id2) {
  queue_push2(&xp->conflictsq, id, id2);

  MAPEXP(&xp->conflicts, id);
  MAPSET(&xp->conflicts, id);
  MAPEXP(&xp->conflicts, id2);
  MAPSET(&xp->conflicts, id2);
}
/*
 * initialize the conflict of expander
 */
void
expander_init_conflict(Expander *xp, char *pkg)
{
  if (!pkg)
    return;

  Pool *pool = xp->pool;

  char *str = pkg;
  char *p = strchr(str, ':');
  if (!p)
    return;

  Id id = pool_strn2id(pool, str, p - str, 1);
  str = p + 1;

  while ((p = strchr(str, ',')) != 0)
  {
    expander_set_conflict(xp, id, pool_strn2id(pool, str, p - str, 1));

    str = p + 1;
  }

  expander_set_conflict(xp, id, pool_str2id(pool, str, 1));
}

static void
expander_set_file_provides(Expander *xp, Id fileId, Queue *fileprovides)
{
  if (!fileprovides->count)
    return;

  xp->havefileprovides = 1;

  Queue q;
  queue_init(&q);

  Id p, pp;
  Pool *pool = xp->pool; // must delcare here, because FOR_PROVIDES will reference it.

  FOR_PROVIDES(p, pp, fileId)
    queue_push(&q, p);

  int havenew = 0;
  Id id;

  for (int i = 0; i < fileprovides->count; i++)
  {
    id = fileprovides->elements[i];
    FOR_PROVIDES(p, pp, id)
      {
        if (pool->solvables[p].name != id)
          continue;

        for (int k = 0; ; k++)
        {
          if (k == q.count || q.elements[k] > p)
          {
            queue_insert(&q, k, p);
            havenew = 1;
            break;
          }
          if (q.elements[k] == p)
            break;
        }
      }
  }

  if (havenew)
    pool->whatprovides[fileId] = pool_queuetowhatprovides(pool, &q);

  queue_free(&q);
}

/*
 * initialize the file provides of expander
 */
void
expander_init_file_provides(Expander *xp, char *file, char *provides)
{
  if (!file || !provides)
    return;

  Pool *pool = xp->pool;
  Id fileId = pool_str2id(pool, file, 1);

  Queue fileprovides;
  queue_init(&fileprovides);

  char *str = provides;
  char *p = 0;
  Id id = 0;

  while ((p = strchr(str, ',')) != 0)
  {
    id = pool_strn2id(pool, str, p - str, 0);
    if (id)
      queue_push(&fileprovides, id);
    
    str = p + 1;
  }

  id = pool_str2id(pool, str, 0);
  if (id)
    queue_push(&fileprovides, id);

  expander_set_file_provides(xp, fileId, &fileprovides);

  queue_free(&fileprovides);
}
