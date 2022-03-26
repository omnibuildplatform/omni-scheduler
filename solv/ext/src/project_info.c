
/*
 * project_info.c
 *
 * analysis project info
 *
 */

#include "project_info.h"
#include "repo.h"


Project_Info *
new_Project_Info(Pool *pool)
{
  Project_Info *pi;

  pi = (Project_Info *)solv_calloc(1, sizeof(*pi));

  pi->pool = pool;

  queue_init(&pi->dep2src);
  queue_init(&pi->dep2pkg);
  queue_init(&pi->subpacks);
  queue_init(&pi->localdeps);
  queue_init(&pi->nonlocal_depsrcs);

  return pi;
}

void
project_info_free(Project_Info *pi)
{
  if (!pi)
    return;

  queue_free(&pi->dep2src);
  queue_free(&pi->dep2pkg);
  queue_free(&pi->subpacks);
  queue_free(&pi->localdeps);
  queue_free(&pi->nonlocal_depsrcs);


  solv_free(pi);
}

static int
subpack_sort_cmp(const void *ap, const void *bp, void *dp)
{
  Pool *pool = (Pool *)dp;
  const Id *a = ap;
  const Id *b = bp;
  int r = a[1] - b[1];
  if (r)
    return r;
  r = strcmp(pool_id2str(pool, a[0]), pool_id2str(pool, b[0]));
  return r ? r : a[0] - b[0];
}

void
project_info_parse(Project_Info *pi, char *prp)
{
  Queue subq;
  queue_init(&subq);

  Id p, srcname, srctype;
  Solvable *s;
  const char *srcstr;

  int ridx, n;
  Repo *repo;
  Pool *pool = pi->pool;
  FOR_REPOS(ridx, repo)
    {
      int islocal = repo->name && !strcmp(repo->name, prp);
      n = 0;

      FOR_REPO_SOLVABLES(repo, p, s)
        {
          if (!MAPTST(pool->considered, p))
            continue;

          srctype = solvable_lookup_type(pool->solvables + p, SOLVABLE_SOURCENAME);

          if (srctype == REPOKEY_TYPE_VOID)
            srcname = s->name;
          else if (srctype == REPOKEY_TYPE_ID)
            srcname = solvable_lookup_id(pool->solvables + p, SOLVABLE_SOURCENAME);
          else
            {
              srcstr = solvable_lookup_str(pool->solvables + p, SOLVABLE_SOURCENAME);
              srcname = srcstr ? pool_str2id(pool, srcstr, 1) : 0;
            }

          if (!srcname || srcname == 1)
            srcname = s->name;

          queue_push2(&subq, s->name, srcname);
          queue_push2(&pi->dep2src, s->name, srcname);
          queue_push2(&pi->dep2pkg, s->name, p);

          if (islocal)
            queue_push(&pi->localdeps, s->name);
          else
	  {
            n++;
            queue_push(&pi->nonlocal_depsrcs, srcname);
	  }
        }

      if (n > 0)
        queue_push2(&pi->nonlocal_depsrcs, ridx, n+1);
    }

  solv_sort(subq.elements, subq.count / 2, sizeof(Id) * 2, subpack_sort_cmp, pool);

  queue_push2(&subq, 0, 0);

  Id lastsrc = 0;
  int i, j;
  for (i = j = 0; i < subq.count; i += 2)
  {
    if (subq.elements[i + 1] != lastsrc)
    {
      if (j < i)
      {
        for (n = 0; j < i; j += 2)
	{
          queue_push(&pi->subpacks, subq.elements[j]);
	  n++;
	}

        queue_push2(&pi->subpacks, lastsrc, n + 1);
      }

      lastsrc = subq.elements[i + 1];
    }
  }

  queue_free(&subq);
}

int
project_info_get_dep2src_num(Project_Info *pi)
{
  return pi->dep2src.count / 2;
}

const char *
project_info_get_dep2src(Project_Info *pi)
{
  Id id = queue_shift(&pi->dep2src);
  if (id == 0)
    return 0;

  return pool_id2str(pi->pool, id);
}

const char *
project_info_get_dep2pkg_dep(Project_Info *pi)
{
  Id id = queue_shift(&pi->dep2pkg);
  if (id == 0)
    return 0;

  return pool_id2str(pi->pool, id);
}

int
project_info_get_dep2pkg_pkg(Project_Info *pi)
{
  return queue_shift(&pi->dep2pkg);
}

int
project_info_get_dep2pkg_num(Project_Info *pi)
{
  return pi->dep2pkg.count / 2;
}

int
project_info_get_localdeps_num(Project_Info *pi)
{
  return pi->localdeps.count;
}

const char *
project_info_get_localdeps_dep(Project_Info *pi)
{
  Id id = queue_shift(&pi->localdeps);
  if (id == 0)
    return 0;

  return pool_id2str(pi->pool, id);
}

int
project_info_get_subpacks_next_group_num(Project_Info *pi)
{
  return queue_pop(&pi->subpacks);
}

const char *
project_info_get_subpacks_next_group_member(Project_Info *pi)
{
  Id id = queue_pop(&pi->subpacks);
  if (id == 0)
    return 0;

  return pool_id2str(pi->pool, id);
}

int
project_info_get_nonlocal_depsrcs_next_group_num(Project_Info *pi)
{
  return queue_pop(&pi->nonlocal_depsrcs);
}

const char *
project_info_get_nonlocal_depsrcs_repo_name(Project_Info *pi)
{
  int index = queue_pop(&pi->nonlocal_depsrcs);
  if (index == 0)
    return 0;

  return pi->pool->repos[index]->name;
}

const char *
project_info_get_nonlocal_depsrcs_src(Project_Info *pi)
{
  Id id = queue_pop(&pi->nonlocal_depsrcs);
  if (id == 0)
    return 0;

  return pool_id2str(pi->pool, id);
}
