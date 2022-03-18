/*
 * pool_considered.c
 *
 * rebuild pool->considered
 *
 */

#include "evr.h"
#include "common.h"

static void
set_disttype_from_location(Pool *pool, Solvable *so)
{
  unsigned int medianr;
  const char *s = solvable_get_location(so, &medianr);
  int disttype = -1;
  int sl;
  if (!s)
    return;
  sl = strlen(s);
  if (disttype < 0 && sl >= 4 && !strcmp(s + sl - 4, ".rpm"))
    disttype = DISTTYPE_RPM;
#ifdef DISTTYPE_DEB
  if (disttype < 0 && sl >= 4 && !strcmp(s + sl - 4, ".deb"))
    disttype = DISTTYPE_DEB;
#endif
#ifdef DISTTYPE_ARCH
  if (disttype < 0 && sl >= 12 && (!strcmp(s + sl - 11, ".pkg.tar.gz") || !strcmp(s + sl - 11, ".pkg.tar.xz") || !strcmp(s + sl - 12, ".pkg.tar.zst")))
    disttype = DISTTYPE_ARCH;
#endif
  if (disttype >= 0 && pool->disttype != disttype)
    set_disttype(pool, disttype);
}

static void
create_module_map(Repo *repo, Map *modulemap, Queue *modulemapq, Id buildservice_modules)
{
  Pool *pool = repo->pool;
  Id *modules = pool->appdata;
  int i, have_moduleinfo = 0;
  Id id, p, *pp;
  Solvable *s;

  if (!modulemap->size)
    map_grow(modulemap, pool->ss.nstrings);
  if (!modules)
    return;
  if (!*modules)
  {
    map_setall(modulemap);
    return;
  }
  /* clear old bits */
  if (modulemapq->count)
  {
    for (i = 0; i < modulemapq->count; i++)
      MAPCLR(modulemap, modulemapq->elements[i]);
    queue_empty(modulemapq);
  }
  for (modules = pool->appdata; *modules; modules++)
    MAPSET(modulemap, *modules);
  /* look for module information stored in "buildservice:modules" solvables */
  FOR_REPO_SOLVABLES(repo, p, s)
    {
      if (s->name != buildservice_modules || s->arch != ARCH_SRC)
        continue;
      have_moduleinfo = 1;
      if (s->evr >= 1 && s->evr < pool->ss.nstrings && MAPTST(modulemap, s->evr))
      {
        queue_push(modulemapq, s->evr);        /* directly addressed */
        continue;
      }
      id = s->repo->idarraydata[s->provides];
      if (id < 1 || id >= pool->ss.nstrings || !MAPTST(modulemap, id))
        continue;        /* not what we're looking for */
      for (pp = s->repo->idarraydata + s->requires; (id = *pp) != 0; pp++)
      {
        /* check if the dep is fulfilled by any module in the list */
        if (id < 1 || id >= pool->ss.nstrings)
          break;        /* hey! */
        if (!MAPTST(modulemap, id) && !match_modules_req(pool, id))
          break;        /* could not fulfil requires */
      }
      if (id)
        continue;        /* could not fulfil one of the requires, ignore module */
      queue_push(modulemapq, s->evr);
    }
  if (!have_moduleinfo)
  {
    /* old style repo with no moduleinfo at all. simple use the unexpanded ids */
    for (modules = pool->appdata; *modules; modules++)
      queue_push(modulemapq, *modules);
    return;
  }
  for (modules = pool->appdata; *modules; modules++)
    MAPCLR(modulemap, *modules);
  for (i = 0; i < modulemapq->count; i++)
    MAPSET(modulemap, modulemapq->elements[i]);
}

static int
in_module_map(Pool *pool, Map *modulemap, Queue *modules)
{
  int i;
  for (i = 0; i < modules->count; i++)
  { 
    Id id = modules->elements[i];
    if (id > 1 && id < pool->ss.nstrings && MAPTST(modulemap, id))
      return 1;
  }
  return 0;
}

void
create_considered(Pool *pool, Repo *repoonly, Map *considered, int unorderedrepos)
{
  Id p, pb,*best;
  Solvable *s, *sb;
  int ridx;
  Repo *repo;
  int olddisttype = -1;
  int dodrepo;
  int mayhave_modules;
  Queue modules;
  Map modulemap;
  Queue modulemapq;
  int modulemap_uptodate;

  map_init(considered, pool->nsolvables);
  best = solv_calloc(sizeof(Id), pool->ss.nstrings);
  
  queue_init(&modules);
  map_init(&modulemap, 0);
  queue_init(&modulemapq);

  Id buildservice_id = pool_str2id(pool, ID_BUILDSERVICE_ID, 0);
  Id buildservice_dodurl = pool_str2id(pool, ID_BUILDSERVICE_DODURL, 0);
  Id buildservice_modules = pool_str2id(pool, ID_BUILDSERVICE_MODULES, 0);

  FOR_REPOS(ridx, repo)
    {
      if (repoonly && repo != repoonly)
        continue;
      dodrepo = repo_lookup_str(repo, SOLVID_META, buildservice_dodurl) != 0;
      mayhave_modules = has_keyname(repo, buildservice_modules) ? 1 : 0;
      modulemap_uptodate = 0;
      FOR_REPO_SOLVABLES(repo, p, s)
        {
          int inmodule = 0;
          if (s->arch == ARCH_SRC || s->arch == ARCH_NOSRC)
            continue;
          pb = best[s->name];
          sb = pb ? pool->solvables + pb : 0;
          if (mayhave_modules)
          {
            solvable_lookup_idarray(s, buildservice_modules, &modules);
            inmodule = modules.count ? 1 : 0;
            if (inmodule)
            {
              if (!modulemap_uptodate)
              {
                create_module_map(repo, &modulemap, &modulemapq, buildservice_modules);
                modulemap_uptodate = 1;
              }
              if (!in_module_map(pool, &modulemap, &modules))
                continue;                /* nope, ignore package */
            }
          }
          if (unorderedrepos && sb && s->repo->priority != sb->repo->priority)
          {
            if (s->repo->priority < sb->repo->priority)
              continue;        /* lower prio, ignore */
          }
          else if (sb)
          {
            int sbinmodule = 0;
            /* we already have that name. decide which one to take */
            if (!unorderedrepos && s->repo != sb->repo)
              continue;        /* first repo wins */

            if (s->repo == sb->repo && mayhave_modules)
              sbinmodule = solvable_lookup_type(sb, buildservice_modules) ? 1 : 0;

            if (inmodule != sbinmodule)
            {
              if (inmodule < sbinmodule)
                continue;
            }
            else if (s->evr != sb->evr)
            {
              /* check versions */
              int r;
              if (olddisttype < 0)
              {
                olddisttype = pool->disttype;
                set_disttype_from_location(pool, s);
              }
              r = pool_evrcmp(pool, sb->evr, s->evr, EVRCMP_COMPARE);
              if (r == 0)
                r = strcmp(pool_id2str(pool, sb->evr), pool_id2str(pool, s->evr));
              if (r >= 0)
                continue;
            }
            else if (s->arch != sb->arch)
            {
              /* same versions, check arch */
              if (ISNOARCH(sb->arch) && !ISNOARCH(s->arch))
                continue;
              if (ISNOARCH(sb->arch) || !ISNOARCH(s->arch))
              {
                int r;
                /* the strcmp is kind of silly, but works for most archs */
                r = strcmp(pool_id2str(pool, sb->arch), pool_id2str(pool, s->arch));
                if (r >= 0)
                  continue;
              }
            }
          }

          if (dodrepo)
          {
            /* we only consider dod packages */
            const char *bsid = solvable_lookup_str(s, buildservice_id);
            if (!bsid || strcmp(bsid, "dod") != 0)
              continue;
          }
          if (pb)
            MAPCLR(considered, pb);
          best[s->name] = p;
          MAPSET(considered, p);
        }
      /* dodrepos have a second pass: replace dod entries with identical downloaded ones */
      if (dodrepo)
      {
        const char *bsid;
        FOR_REPO_SOLVABLES(repo, p, s)
          {
            if (s->arch == ARCH_SRC || s->arch == ARCH_NOSRC)
              continue;
            pb = best[s->name];
            if (!pb || pb == p)
              continue;
            sb = pool->solvables + pb;
            if (sb->repo != s->repo || sb->name != s->name || sb->arch != s->arch || sb->evr != s->evr)
              continue;
            bsid = solvable_lookup_str(s, buildservice_id);
            if (bsid && strcmp(bsid, "dod") == 0)
              continue;        /* not downloaded */
            MAPCLR(considered, pb);
            best[s->name] = p;
            MAPSET(considered, p);
          }
      }
    }
  solv_free(best);
  queue_free(&modules);
  map_free(&modulemap);
  queue_free(&modulemapq);
  if (olddisttype >= 0 && pool->disttype != olddisttype)
    set_disttype(pool, olddisttype);
}
