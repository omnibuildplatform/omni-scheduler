/*
 * expander_ctx.c
 *
 * expander context
 *
 */

#include "expander_ctx.h"
#include "common.h"

#define CPLXDEPS_TODNF (1 << 0)
#define MAKECPLX(pool, d) (MAKERELDEP(pool->nrels + d))

static int
invert_depblocks(ExpanderCtx *xpctx, Queue *bq, int start, int r)
{
  int i, j, end;
  if (r == 0 || r == 1)
    return r ? 0 : 1;
  end = bq->count;
  for (i = j = start; i < end; i++)
    {
      if (bq->elements[i])
        {
          bq->elements[i] = -bq->elements[i];
          continue;
        }
      /* end of block reached, reverse */
      if (i - 1 > j)
        {
          int k;
          for (k = i - 1; j < k; j++, k--)
            {
              Id t = bq->elements[j];
              bq->elements[j] = bq->elements[k];
              bq->elements[k] = t;
            }
        }
      j = i + 1;
    }
  return -1;
}

static int
distribute_depblocks(ExpanderCtx *xpctx, Queue *bq, int start, int start2, int flags)
{
  int i, j, end2 = bq->count;
  for (i = start; i < start2; i++)
    {
      for (j = start2; j < end2; j++)
	{
	  int a, b;
	  int bqcnt4 = bq->count;
	  int k = i;

	  /* distribute i block with j block, both blocks are sorted */
	  while (bq->elements[k] && bq->elements[j])
	    {
	      if (bq->elements[k] < bq->elements[j])
		queue_push(bq, bq->elements[k++]);
	      else
		{
		  if (bq->elements[k] == bq->elements[j])
		    k++;
		  queue_push(bq, bq->elements[j++]);
		}
	    }
	  while (bq->elements[j])
	    queue_push(bq, bq->elements[j++]);
	  while (bq->elements[k])
	    queue_push(bq, bq->elements[k++]);

	  /* block is finished, check for A + -A */
	  for (a = bqcnt4, b = bq->count - 1; a < b; )
	    {
	      if (-bq->elements[a] == bq->elements[b])
		break;
	      if (-bq->elements[a] > bq->elements[b])
		a++;
	      else
		b--;
	    }
	  if (a < b)
	    queue_truncate(bq, bqcnt4);     /* ignore this block */
	  else
	    queue_push(bq, 0);      /* finish block */
	}
      /* advance to next block */
      while (bq->elements[i])
	i++;
    }
  queue_deleten(bq, start, end2 - start);
  if (start == bq->count)
    return flags & CPLXDEPS_TODNF ? 0 : 1;
  return -1;
}

static int
pool_is_complex_dep_rd(Pool *pool, Reldep *rd)
{
  for (;;)
    {
      if (rd->flags == REL_AND || rd->flags == REL_COND || rd->flags == REL_UNLESS)        /* those are the complex ones */
        return 1;
      if (rd->flags != REL_OR)
        return 0;
      if (ISRELDEP(rd->name) && pool_is_complex_dep_rd(pool, GETRELDEP(pool, rd->name)))
        return 1;
      if (!ISRELDEP(rd->evr))
        return 0;
      rd = GETRELDEP(pool, rd->evr);
    }
}

int
pool_is_complex_dep(Pool *pool, Id dep)
{
  if (ISRELDEP(dep))
    {
      Reldep *rd = GETRELDEP(pool, dep);
      if (rd->flags >= 8 && pool_is_complex_dep_rd(pool, rd))
        return 1;
    }
  return 0;
}

static int normalize_dep(ExpanderCtx *xpctx, Id dep, Queue *bq, int flags);

static int
normalize_dep_or(ExpanderCtx *xpctx, Id dep1, Id dep2, Queue *bq, int flags, int invflags)
{
  int r1, r2, bqcnt2, bqcnt = bq->count;
  r1 = normalize_dep(xpctx, dep1, bq, flags);
  if (r1 == 1)
    return 1;		/* early exit */
  bqcnt2 = bq->count;
  r2 = normalize_dep(xpctx, dep2, bq, flags ^ invflags);
  if (invflags)
    r2 = invert_depblocks(xpctx, bq, bqcnt2, r2);
  if (r1 == 1 || r2 == 1)
    {
      queue_truncate(bq, bqcnt);
      return 1;
    }
  if (r1 == 0)
    return r2;
  if (r2 == 0)
    return r1;
  if ((flags & CPLXDEPS_TODNF) == 0)
    return distribute_depblocks(xpctx, bq, bqcnt, bqcnt2, flags);
  return -1;
}

static int
normalize_dep_and(ExpanderCtx *xpctx, Id dep1, Id dep2, Queue *bq, int flags, int invflags)
{
  int r1, r2, bqcnt2, bqcnt = bq->count;
  r1 = normalize_dep(xpctx, dep1, bq, flags);
  if (r1 == 0)
    return 0;		/* early exit */
  bqcnt2 = bq->count;
  r2 = normalize_dep(xpctx, dep2, bq, flags ^ invflags);
  if (invflags)
    r2 = invert_depblocks(xpctx, bq, bqcnt2, r2);
  if (r1 == 0 || r2 == 0)
    {
      queue_truncate(bq, bqcnt);
      return 0;
    }
  if (r1 == 1)
    return r2;
  if (r2 == 1)
    return r1;
  if ((flags & CPLXDEPS_TODNF) != 0)
    return distribute_depblocks(xpctx, bq, bqcnt, bqcnt2, flags);
  return -1;
}

static int
normalize_dep_if_else(ExpanderCtx *xpctx, Id dep1, Id dep2, Id dep3, Queue *bq, int flags)
{
  /* A IF (B ELSE C) -> (A OR ~B) AND (C OR B) */
  int r1, r2, bqcnt2, bqcnt = bq->count;
  r1 = normalize_dep_or(xpctx, dep1, dep2, bq, flags, CPLXDEPS_TODNF);
  if (r1 == 0)
    return 0;		/* early exit */
  bqcnt2 = bq->count;
  r2 = normalize_dep_or(xpctx, dep2, dep3, bq, flags, 0);
  if (r1 == 0 || r2 == 0)
    {
      queue_truncate(bq, bqcnt);
      return 0;
    }
  if (r1 == 1)
    return r2;
  if (r2 == 1)
    return r1;
  if ((flags & CPLXDEPS_TODNF) != 0)
    return distribute_depblocks(xpctx, bq, bqcnt, bqcnt2, flags);
  return -1;
}

static int
normalize_dep_unless_else(ExpanderCtx *xpctx, Id dep1, Id dep2, Id dep3, Queue *bq, int flags)
{
  /* A UNLESS (B ELSE C) -> (A AND ~B) OR (C AND B) */
  int r1, r2, bqcnt2, bqcnt = bq->count;
  r1 = normalize_dep_and(xpctx, dep1, dep2, bq, flags, CPLXDEPS_TODNF);
  if (r1 == 1)
    return 1;		/* early exit */
  bqcnt2 = bq->count;
  r2 = normalize_dep_and(xpctx, dep2, dep3, bq, flags, 0);
  if (r1 == 1 || r2 == 1)
    {
      queue_truncate(bq, bqcnt);
      return 1;
    }
  if (r1 == 0)
    return r2;
  if (r2 == 0)
    return r1;
  if ((flags & CPLXDEPS_TODNF) == 0)
    return distribute_depblocks(xpctx, bq, bqcnt, bqcnt2, flags);
  return -1;
}

static int expander_isignored(Expander *xp, Solvable *s, Id req);

static int
normalize_dep(ExpanderCtx *xpctx, Id dep, Queue *bq, int flags)
{
  Pool *pool = xpctx->pool;
  Id p, dp;
  
  if (pool_is_complex_dep(pool, dep))
    {
      Reldep *rd = GETRELDEP(pool, dep);
      if (rd->flags == REL_COND)
	{
	  Id evr = rd->evr;
	  if (ISRELDEP(evr))
	    {
	      Reldep *rd2 = GETRELDEP(pool, evr);
	      if (rd2->flags == REL_ELSE)
		return normalize_dep_if_else(xpctx, rd->name, rd2->name, rd2->evr, bq, flags);
	    }
	  return normalize_dep_or(xpctx, rd->name, rd->evr, bq, flags, CPLXDEPS_TODNF);
	}
      if (rd->flags == REL_UNLESS)
	{
	  Id evr = rd->evr;
	  if (ISRELDEP(evr))
	    {
	      Reldep *rd2 = GETRELDEP(pool, evr);
	      if (rd2->flags == REL_ELSE)
		return normalize_dep_unless_else(xpctx, rd->name, rd2->name, rd2->evr, bq, flags);
	    }
	  return normalize_dep_and(xpctx, rd->name, rd->evr, bq, flags, CPLXDEPS_TODNF);
	}
      if (rd->flags == REL_OR)
	return normalize_dep_or(xpctx, rd->name, rd->evr, bq, flags, 0);
      if (rd->flags == REL_AND)
	return normalize_dep_and(xpctx, rd->name, rd->evr, bq, flags, 0);
    }

  if (xpctx->ignore_s && (flags & CPLXDEPS_TODNF) == 0)
    {
      if (expander_isignored(xpctx->xp, xpctx->ignore_s, dep))
	return 1;
    }

  dp = pool_whatprovides(pool, dep);
  if (dp == 2)
    return 1;
  if (dp < 2 || !pool->whatprovidesdata[dp])
    return 0;
  if (pool->whatprovidesdata[dp] == SYSTEMSOLVABLE)
    return 1;
  if ((flags & CPLXDEPS_TODNF) != 0)
    {
      while ((p = pool->whatprovidesdata[dp++]) != 0)
        queue_push2(bq, p, 0);
    }
  else
    {
      while ((p = pool->whatprovidesdata[dp++]) != 0)
        queue_push(bq, p);
      queue_push(bq, 0);
    }
  return -1;
}

static int
expander_isignored(Expander *xp, Solvable *s, Id req)
{
  Pool *pool = xp->pool;
  Id id = id2name(pool, req);
  const char *n;

  if (!xp->ignoreignore)
    {
      if (MAPTST(&xp->ignored, id))
	return 1;
      if (MAPTST(&xp->ignoredx, id))
	{
	  Id xid = pool_str2id(pool, pool_tmpjoin(pool, pool_id2str(pool, s->name), ":", pool_id2str(pool, id)), 0);
	  if (xid && MAPTST(&xp->ignored, xid))
	    return 1;
	}
    }
  n = pool_id2str(pool, id);
  if (!strncmp(n, "rpmlib(", 7))
    {
      MAPEXP(&xp->ignored, id);
      MAPSET(&xp->ignored, id);
      return 1;
    }
  if (*n == '/')
    {
      if (!xp->havefileprovides || pool->whatprovides[id] <= 1)
	{
	  MAPEXP(&xp->ignored, id);
	  MAPSET(&xp->ignored, id);
	  return 1;
	}
    }
  return 0;
}

static int
expander_to_cplxblks(ExpanderCtx *xpctx, Id p, Id dep, int deptype, Id *ptr)
{
  int blkoff = xpctx->cplxblks.count;
  queue_push(&xpctx->cplxblks, p);
  queue_push(&xpctx->cplxblks, dep);
  queue_push(&xpctx->cplxblks, deptype);
  for (;;)
    {
      Id pp = *ptr++;
      queue_push(&xpctx->cplxblks, pp);
      if (!pp)
	break;
    }
  return blkoff;
}

static int
expander_check_cplxblock(ExpanderCtx *xpctx, Id p, Id dep, int deptype, Id *ptr, int blkoff)
{
  Pool *pool = xpctx->pool;
  int posn = 0, posi = 0, negn = 0, negi = 0;
  Id pp, *ptr2 = ptr;
  Id lastcon = 0;

  while ((pp = *ptr2++) != 0)
    {
      if (pp > 0)
	{
	  posn++;
	  if (MAPTST(&xpctx->installed, pp))
	    posi++;
	}
      else
	{
	  if (p == -pp)
	    continue;	/* ignore redundant self-entry */
	  negn++;
	  if (MAPTST(&xpctx->installed, -pp))
	    negi++;
	  else
	    lastcon = -pp;
	}
    }
#if 0
  printf("expander_check_cplxblock pos: %d,%d neg: %d,%d\n", posn, posi, negn, negi);
#endif
  if (posi)
    return -1;
  if (!posn && deptype == DEPTYPE_RECOMMENDS)
    return -1;
  if (negi == negn)
    {
      /* all neg installed */
      if (posn)
	{
	  /* posn > 0 and all neg installed, add to todo */
	  if (blkoff < 0)
	    blkoff = expander_to_cplxblks(xpctx, p, dep, deptype, ptr);
#if 0
	  printf("put on todo, blkoff = %d\n", blkoff);
#endif
	  queue_push2(&xpctx->todo, MAKECPLX(pool, blkoff), p);
	}
      else
	{
	  /* no posn, conflict */
	  for (ptr2 = ptr; (pp = *ptr2++) != 0; )
	    {
	      if (p == -pp)
		continue;	/* ignore redundant self-entry */
	      if (deptype == DEPTYPE_REQUIRES)
		{
		  /* do not report a requires as conflicts */
		  queue_push(&xpctx->errors, ERROR_NOPROVIDER);
		  queue_push2(&xpctx->errors, dep, p);
		  break;
		}
	      queue_push(&xpctx->errors, ERROR_CONFLICT);
	      queue_push2(&xpctx->errors, p, -pp);
	    }
	}
      return -1;
    }
  else if (!posn && negn && negi == negn - 1)
    {
      /* add conflict */
#if 0
      printf("add conflict %d %d\n", lastcon, p);
#endif
      MAPEXP(&xpctx->conflicts, pool->nsolvables);
      MAPSET(&xpctx->conflicts, lastcon);
      if (p)
        queue_push2(&xpctx->conflictsinfo, lastcon, p);	/* always do this for rich deps */
      return -1;
    }
  else
    {
#ifdef DEBUG_COND
      printf("put/stay on cond queue, blkoff = %d\n", blkoff);
#endif
      /* either posn > 0 and 1 neg not installed or more than 1 neg not installed */
      if (blkoff < 0)
	blkoff = expander_to_cplxblks(xpctx, p, dep, deptype, ptr);
      return blkoff;
    }
}

void
expander_installed_complexdep(ExpanderCtx *xpctx, Id p, Id dep, int deptype)
{
  Queue *cplxq = &xpctx->cplxq;
  int r, i, start = cplxq->count, blkoff;

#if 0
  printf("expander_installed_complexdep %s type %d\n", pool_dep2str(xpctx->pool, dep), deptype);
#endif
  if (deptype == DEPTYPE_CONFLICTS)
    {
      r = normalize_dep(xpctx, dep, cplxq, CPLXDEPS_TODNF);
      r = invert_depblocks(xpctx, cplxq, start, r);
    }
  else
    r = normalize_dep(xpctx, dep, cplxq, 0);
#if 0
  print_depblocks(xpctx, cplxq, start, r);
#endif
  if (r == 1)
    return;
  if (r == 0)
    {
      if (deptype == DEPTYPE_CONFLICTS)
	{
	  queue_push(&xpctx->errors, ERROR_ALLCONFLICT);
	  queue_push2(&xpctx->errors, dep, p);
	}
      else if (deptype != DEPTYPE_RECOMMENDS)
	{
	  queue_push(&xpctx->errors, ERROR_NOPROVIDER);
	  queue_push2(&xpctx->errors, dep, p);
	}
      return;
    }
  while (start < cplxq->count)
    {
      /* find end */
      for (i = start; cplxq->elements[i] != 0; i++)
	;
      blkoff = expander_check_cplxblock(xpctx, p, dep, deptype, cplxq->elements + start, -1);
      if (blkoff >= 0)
	{
	  Pool *pool = xpctx->pool;
	  Id p;
	  MAPEXP(&xpctx->todo_condmap, pool->nsolvables);
	  for (i = start; (p = cplxq->elements[i]) != 0; i++)
	    if (p < 0)
	      MAPSET(&xpctx->todo_condmap, -p);
	  queue_push(&xpctx->todo_cond, blkoff);
	}
      start = i + 1;
    }
  queue_truncate(cplxq, start);
}

static int
expander_checkconflicts_complexdep(ExpanderCtx *xpctx, Id p, Id dep, int deptype, int recorderrors)
{
  Queue *cplxq = &xpctx->cplxq;
  int r, i, start = cplxq->count;
  Id pp;
  int ret = 0;

#if 0
  printf("expander_checkconflicts_complexdep %s type %d\n", pool_dep2str(xpctx->pool, dep), deptype);
#endif
  if (deptype == DEPTYPE_CONFLICTS)
    {
      r = normalize_dep(xpctx, dep, cplxq, CPLXDEPS_TODNF);
      r = invert_depblocks(xpctx, cplxq, start, r);
    }
  else
    r = normalize_dep(xpctx, dep, cplxq, 0);
#if 0
  print_depblocks(xpctx, cplxq, start, r);
#endif
  /* r == 0: conflict with everything. Ignore here, pick error up when package gets installed */
  if (r == 0 || r == 1)
    return 0;
  while (start < cplxq->count)
    {
      for (i = start; (pp = cplxq->elements[i]) != 0; i++)
        if (pp > 0 || (pp < 0 && !MAPTST(&xpctx->installed, -pp)))
	  break;
      if (pp == 0)
	{
	  /* no pos and all neg installed -> conflict */
	  for (i = start; (pp = cplxq->elements[i]) != 0; i++)
	    {
	      pp = -cplxq->elements[i];
	      if (recorderrors)
		{
		  queue_push(&xpctx->errors, recorderrors == 2 ? ERROR_CONFLICT : ERROR_PROVIDERINFO);
		  queue_push2(&xpctx->errors, p, pp);
		}
	      else if (xpctx->xp->debug)
		{
		  Pool *pool = xpctx->pool;
		  expander_dbg(xpctx->xp, "ignoring provider %s because it conflicts with installed %s\n", pool_solvid2str(pool, p), pool_solvid2str(pool, pp));
		}
	      ret = ret ? 1 : pp;
	    }
	}
      for (; cplxq->elements[i] != 0; i++)
	;
      start = i + 1;
    }
  queue_truncate(cplxq, start);
  return ret;
}

static void
updateconflictsinfo(ExpanderCtx *xpctx)
{
  int i;
  Pool *pool = xpctx->pool;
  Queue *out = xpctx->out;
  Queue *conflictsinfo = &xpctx->conflictsinfo;

  if (xpctx->ignoreconflicts)
    return;
  for (i = xpctx->cidone; i < out->count; i++)
    {
      Id p, p2, pp2;
      Id con, *conp;
      Solvable *s;
      p = out->elements[i];
      s = pool->solvables + p;
      /* keep in sync with expander_installed! */
      if (s->conflicts)
	{
	  conp = s->repo->idarraydata + s->conflicts;
	  while ((con = *conp++) != 0)
	    {
	      if (pool_is_complex_dep(pool, con))
		continue;	/* already pushed */
	      FOR_PROVIDES(p2, pp2, con)
		{
		  if (p2 == p)
		    continue;
		  queue_push2(conflictsinfo, p2, p);
		}
	    }
	}
      if (s->obsoletes)
	{
	  conp = s->repo->idarraydata + s->obsoletes;
	  while ((con = *conp++) != 0)
	    {
	      FOR_PROVIDES(p2, pp2, con)
		{
		  if (p2 == p || !pool_match_nevr(pool, pool->solvables + p2, con))
		    continue;
		  queue_push2(conflictsinfo, p2, -p);
		}
	    }
	}
    }
  xpctx->cidone = out->count;
}

int
findconflictsinfo(ExpanderCtx *xpctx, Id p, int recorderrors)
{
  Queue *conflictsinfo = &xpctx->conflictsinfo;
  int i, ret = 0;

  if (xpctx->cidone < xpctx->out->count)
    updateconflictsinfo(xpctx);

  for (i = 0; i < conflictsinfo->count; i++)
    if (conflictsinfo->elements[i] == p)
      {
	ret = conflictsinfo->elements[i + 1];
	if (recorderrors)
	  {
	    queue_push(&xpctx->errors, recorderrors == 2 ? ERROR_CONFLICT2 : ERROR_PROVIDERINFO2);
	    queue_push2(&xpctx->errors, p, ret);
	  }
	else if (xpctx->xp->debug)
	  {
	    Pool *pool = xpctx->pool;
	    expander_dbg(xpctx->xp, "ignoring provider %s because installed %s %s it\n", pool_solvid2str(pool, p), pool_solvid2str(pool, ret > 0 ? ret : -ret), ret > 0 ? "conflicts with" : "obsoletes");
	  }
      }
  if (!ret)
    {
      /* conflict from our job, i.e. a !xxx dep */
      if (recorderrors)
	{
	  queue_push(&xpctx->errors, recorderrors == 2 ? ERROR_CONFLICT2 : ERROR_PROVIDERINFO2);
	  queue_push2(&xpctx->errors, p, 0);
	}
      else if (xpctx->xp->debug)
	{
	  Pool *pool = xpctx->pool;
	  expander_dbg(xpctx->xp, "ignoring conflicted provider %s\n", pool_solvid2str(pool, p));
	}
    }
  return ret;
}

static void
recheck_conddeps(ExpanderCtx *xpctx)
{
  int i;
  for (i = 0; i < xpctx->todo_cond.count; i++)
    {
      int blkoff = xpctx->todo_cond.elements[i];
#ifdef DEBUG_COND
      printf("todo_cond %d\n", blkoff);
#endif
      Id *ptr = xpctx->cplxblks.elements + blkoff;
      if (expander_check_cplxblock(xpctx, ptr[0], ptr[1], ptr[2], ptr + 3, blkoff) < 0)
	{
#ifdef DEBUG_COND
	  printf("remove no longer needed cond entry\n");
#endif
	  queue_delete(&xpctx->todo_cond, i);
	  i--;
	}
    }
}

/* install a single package */
static void
expander_installed(ExpanderCtx *xpctx, Id p)
{
  Pool *pool = xpctx->pool;
  Expander *xp = xpctx->xp;
  Solvable *s = pool->solvables + p;
  Id req, *reqp, con, *conp;

#if 0
printf("expander_installed %s\n", pool_solvid2str(pool, p));
#endif
  MAPSET(&xpctx->installed, p);
  queue_push(xpctx->out, p);

  if (xpctx->conflicts.size && MAPTST(&xpctx->conflicts, p))
    findconflictsinfo(xpctx, p, 2);

  /* add synthetic conflicts from the project config */
  if (MAPTST(&xp->conflicts, s->name))
    {
      int i;
      for (i = 0; i < xp->conflictsq.count; i++)
	{
	  Id p2, pp2;
	  Id id = xp->conflictsq.elements[i];
	  if (id != s->name)
	    continue;
	  id = xp->conflictsq.elements[i ^ 1];
	  FOR_PROVIDES(p2, pp2, id)
	    {
	      if (pool->solvables[p2].name != id)
		continue;
	      if (MAPTST(&xpctx->installed, p2))
		{
		  queue_push(&xpctx->errors, ERROR_CONFLICT);
		  queue_push2(&xpctx->errors, p, p2);
		  continue;
		}
	      MAPEXP(&xpctx->conflicts, pool->nsolvables);
	      MAPSET(&xpctx->conflicts, p2);
	      queue_push2(&xpctx->conflictsinfo, p2, p);
	    }
	}
    }

  if (s->requires)
    {
      reqp = s->repo->idarraydata + s->requires;
      while ((req = *reqp++) != 0)
	{
	  if (req == SOLVABLE_PREREQMARKER)
	    continue;
	  if (ISRELDEP(req) && GETRELDEP(pool, req)->flags == REL_ERROR)
	    {
	      queue_push(&xpctx->errors, ERROR_BADDEPENDENCY);
	      queue_push2(&xpctx->errors, GETRELDEP(pool, req)->evr, p);
	      continue;
	    }
	  if (pool_is_complex_dep(pool, req))
	    {
	      xpctx->ignore_s = s;
	      expander_installed_complexdep(xpctx, p, req, DEPTYPE_REQUIRES);
	      xpctx->ignore_s = 0;
	      continue;
	    }
	  if (expander_isignored(xp, s, req))
	    continue;
	  queue_push2(&xpctx->todo, req, p);
	}
    }
  if (!xpctx->ignoreconflicts)
    {
      if (s->conflicts)
	{
	  conp = s->repo->idarraydata + s->conflicts;
	  while ((con = *conp++) != 0)
	    {
	      Id p2, pp2;
	      if (ISRELDEP(con) && GETRELDEP(pool, con)->flags == REL_ERROR)
		{
		  queue_push(&xpctx->errors, ERROR_BADDEPENDENCY);
		  queue_push2(&xpctx->errors, GETRELDEP(pool, con)->evr, p);
		  continue;
		}
	      if (pool_is_complex_dep(pool, con))
		{
		  expander_installed_complexdep(xpctx, p, con, DEPTYPE_CONFLICTS);
		  continue;
		}
	      FOR_PROVIDES(p2, pp2, con)
		{
		  if (p2 == p)
		    continue;
		  if (MAPTST(&xpctx->installed, p2))
		    {
		      queue_push(&xpctx->errors, ERROR_CONFLICT);
		      queue_push2(&xpctx->errors, p, p2);
		      continue;
		    }
		  MAPEXP(&xpctx->conflicts, pool->nsolvables);
		  MAPSET(&xpctx->conflicts, p2);
		  if (xp->debug)
		    queue_push2(&xpctx->conflictsinfo, p2, p);
		}
	    }
	}
      if (s->obsoletes)
	{
	  conp = s->repo->idarraydata + s->obsoletes;
	  while ((con = *conp++) != 0)
	    {
	      Id p2, pp2;
	      FOR_PROVIDES(p2, pp2, con)
		{
		  if (p2 == p || !pool_match_nevr(pool, pool->solvables + p2, con))
		    continue;
		  if (MAPTST(&xpctx->installed, p2))
		    {
		      queue_push(&xpctx->errors, ERROR_CONFLICT);
		      queue_push2(&xpctx->errors, p, -p2);
		      continue;
		    }
		  MAPEXP(&xpctx->conflicts, pool->nsolvables);
		  MAPSET(&xpctx->conflicts, p2);
		  if (xp->debug)
		    queue_push2(&xpctx->conflictsinfo, p2, -p);
		}
	    }
	}
      if (xp->debug)
	xpctx->cidone = xpctx->out->count;
    }
  if (xpctx->todo_condmap.size && MAPTST(&xpctx->todo_condmap, p))
    recheck_conddeps(xpctx);
}

/* same as expander_installed, but install multiple packages
 * in one block */
void
expander_installed_multiple(ExpanderCtx *xpctx, Queue *toinstall)
{
  int i, j, havecond = 0;

  /* unify */
  for (i = j = 0; i < toinstall->count; i++)
    {
      Id p = toinstall->elements[i];
      if (MAPTST(&xpctx->installed, p))
	continue;	/* already seen */
      MAPSET(&xpctx->installed, p);
      toinstall->elements[j++] = p;
      if (xpctx->todo_condmap.size && MAPTST(&xpctx->todo_condmap, p))
	{
	  havecond = 1;
	  MAPCLR(&xpctx->todo_condmap, p);	/* no longer needed */
	}
    }
  queue_truncate(toinstall, j);
  
  /* run conditionals first */
  if (havecond)
    recheck_conddeps(xpctx);

  if (!xpctx->errors.count)
    for (i = 0; i < toinstall->count; i++)
      expander_installed(xpctx, toinstall->elements[i]);
  queue_empty(toinstall);
}

int
expander_checkconflicts(ExpanderCtx *xpctx, Id p, Id *conflicts, int isobsoletes, int recorderrors)
{
  Map *installed = &xpctx->installed;
  Pool *pool = xpctx->pool;
  Id con, p2, pp2;
  int ret = 0;

  if (xpctx->ignoreconflicts)
    return 0;
  while ((con = *conflicts++) != 0)
    {
      if (!isobsoletes && pool_is_complex_dep(pool, con))
	{
	  p2 = expander_checkconflicts_complexdep(xpctx, p, con, DEPTYPE_CONFLICTS, recorderrors);
	  ret = ret ? 1 : p2;
	  continue;
	}
      FOR_PROVIDES(p2, pp2, con)
	{
	  if (p == p2)
	    continue;
	  if (isobsoletes && !pool_match_nevr(pool, pool->solvables + p2, con))
	    continue;
	  if (MAPTST(installed, p2))
	    {
	      if (recorderrors)
		{
		  queue_push(&xpctx->errors, recorderrors == 2 ? ERROR_CONFLICT : ERROR_PROVIDERINFO);
		  queue_push2(&xpctx->errors, p, isobsoletes ? -p2 : p2);
		}
	      else if (xpctx->xp->debug)
		{
		  if (isobsoletes)
		    expander_dbg(xpctx->xp, "ignoring provider %s because it obsoletes installed %s\n", pool_solvid2str(pool, p), pool_solvid2str(pool, p2));
		  else
		    expander_dbg(xpctx->xp, "ignoring provider %s because it conflicts with installed %s\n", pool_solvid2str(pool, p), pool_solvid2str(pool, p2));
		}
	      ret = ret ? 1 : p2;
	    }
	}
    }
  return ret;
}

void
expander_updaterecommendedmap(ExpanderCtx *xpctx)
{
  Pool *pool = xpctx->pool;
  Queue *out = xpctx->out;
  Map *recommended = &xpctx->recommended;

  int i;
  Id p, pp, rec, *recp;
  for (i = xpctx->recdone; i < out->count; i++)
    {
      Solvable *s;
      s = pool->solvables + out->elements[i];
      if (s->recommends)
	{
          MAPEXP(recommended, pool->nsolvables);
          for (recp = s->repo->idarraydata + s->recommends; (rec = *recp++) != 0; )
	    FOR_PROVIDES(p, pp, rec)
	      MAPSET(recommended, p);
	}
    }
  xpctx->recdone = out->count;
}

int
expander_dep_fulfilled(ExpanderCtx *xpctx, Id dep)
{
  Pool *pool = xpctx->pool;
  Id p, pp; 

  if (ISRELDEP(dep))
    {   
      Reldep *rd = GETRELDEP(pool, dep);
      if (rd->flags == REL_COND)
	{
	  if (ISRELDEP(rd->evr))
	    {
	      Reldep *rd2 = GETRELDEP(pool, rd->evr);
	      if (rd2->flags == REL_ELSE)
		{
		  if (expander_dep_fulfilled(xpctx, rd2->name))
		    return expander_dep_fulfilled(xpctx, rd->name);
		  return expander_dep_fulfilled(xpctx, rd2->evr);
		}
	    }
	  if (expander_dep_fulfilled(xpctx, rd->name))	/* A OR ~B */
	    return 1;
	  return !expander_dep_fulfilled(xpctx, rd->evr);
	}
      if (rd->flags == REL_UNLESS)
	{
	  if (ISRELDEP(rd->evr))
	    {
	      Reldep *rd2 = GETRELDEP(pool, rd->evr);
	      if (rd2->flags == REL_ELSE)
		{
		  if (!expander_dep_fulfilled(xpctx, rd2->name))
		    return expander_dep_fulfilled(xpctx, rd->name);
		  return expander_dep_fulfilled(xpctx, rd2->evr);
		}
	    }
	  if (!expander_dep_fulfilled(xpctx, rd->name))	/* A AND ~B */
	    return 0;
	  return !expander_dep_fulfilled(xpctx, rd->evr);
	}
      if (rd->flags == REL_AND)
	{
	  if (!expander_dep_fulfilled(xpctx, rd->name))
	    return 0;
	  return expander_dep_fulfilled(xpctx, rd->evr);
	}
      if (rd->flags == REL_OR)
	{
	  if (expander_dep_fulfilled(xpctx, rd->name))
	    return 1;
	  return expander_dep_fulfilled(xpctx, rd->evr);
	}
    }
  FOR_PROVIDES(p, pp, dep)
    {
      if (MAPTST(&xpctx->installed, p))
	return 1;
    }
  return 0;
}

int
prune_neg_prefers(ExpanderCtx *xpctx, Id who, Id *e, int n)
{
  Expander *xp = xpctx->xp;
  Pool *pool = xpctx->pool;
  Id whon = who ? pool->solvables[who].name : 0;
  int i, j;
  for (i = j = 0; i < n; i++)
    {
      Id p = e[i];
      Id pn = pool->solvables[p].name;
      if (MAPTST(&xp->preferneg, pn))
	continue;
      if (who && MAPTST(&xp->prefernegx, pn))
	{
	  Id xid = pool_str2id(pool, pool_tmpjoin(pool, pool_id2str(pool, whon), ":", pool_id2str(pool, pn)), 0);
	  if (xid && MAPTST(&xp->preferneg, xid))
	    continue;
	}
      e[j++] = p;
    }
  return j ? j : n;
}

int
prune_pos_prefers(ExpanderCtx *xpctx, Id who, Id *e, int n, int domulti)
{
  Expander *xp = xpctx->xp;
  Queue *pruneq = &xpctx->pruneq;
  Pool *pool = xpctx->pool;
  Id whon = who ? pool->solvables[who].name : 0;
  int i, j;

  if (pruneq->count)
    queue_empty(pruneq);
  for (i = j = 0; i < n; i++)
    {
      Id p = e[i];
      Id pn = pool->solvables[p].name;
      if (MAPTST(&xp->preferpos, pn))
	queue_push2(pruneq, pn, p);
      else if (who && MAPTST(&xp->preferposx, pn))
	{
	  Id xid = pool_str2id(pool, pool_tmpjoin(pool, pool_id2str(pool, whon), ":", pool_id2str(pool, pn)), 0);
	  if (xid && MAPTST(&xp->preferpos, xid))
	    queue_push2(pruneq, xid, p);
	}
    }
  if (!pruneq->count)
    return n;
  if (pruneq->count > 2)
    {
      if (!domulti)
	return n;
      /* pos prefers are ordered, the first one wins */
      for (i = 0; i < xp->preferposq.count; i++)
	{
	  Id xid = xp->preferposq.elements[i];
	  for (j = 0; j < pruneq->count; j += 2)
	    if (pruneq->elements[j] == xid)
	      {
		e[0] = pruneq->elements[j + 1];
		return 1;
	      }
	}
    }
  e[0] = pruneq->elements[1];	/* simple case, just one prefer */
  return 1;
}

int
prune_or_dep(ExpanderCtx *xpctx, Id dep, Id *e, int n)
{
  Pool *pool = xpctx->pool;
  int i, j;
  Id p, pp;

  for (;;)
    {
      Reldep *rd = 0;
      if (ISRELDEP(dep))
	{
	  rd = GETRELDEP(pool, dep);
	  if (rd->flags != REL_OR)
	    rd = 0;
	}
      if (rd)
	dep = rd->name;
      i = j = 0;
      /* both sets are ordered */
      FOR_PROVIDES(p, pp, dep)
	{
	  if (p < e[i])
	    continue;
	  while (i < n && p > e[i])
	    i++;
	  if (i == n)
	    break;
	  if (p == e[i])
	    e[j++] = p;
	}
      if (j)
	return j;
      if (rd)
	dep = rd->evr;
      else
	break;
    }
  return n;
}

int
prune_supplemented(ExpanderCtx *xpctx, Id *e, int n)
{
  Pool *pool = xpctx->pool;
  int i, j;
  Id sup, *supp;

  for (i = j = 0; i < n; i++)
    {
      Id p = e[i];
      Solvable *s = pool->solvables + p;
      if (!s->supplements)
	continue;
      supp = s->repo->idarraydata + s->supplements;
      while ((sup = *supp++) != 0)
	if (expander_dep_fulfilled(xpctx, sup))
	  break;
      if (sup)
        e[j++] = p;
    }
  return j ? j : n;
}

void
add_recommended_packages(ExpanderCtx *xpctx, Solvable *s)
{
  Pool *pool = xpctx->pool;
  Id p, pp, rec, *recp;
  for (recp = s->repo->idarraydata + s->recommends; (rec = *recp++) != 0; )
    {
      int haveone = 0;
      if (pool_is_complex_dep(pool, rec))
	{
	  expander_installed_complexdep(xpctx, s - pool->solvables, rec, DEPTYPE_RECOMMENDS);
	  continue;
	}
      FOR_PROVIDES(p, pp, rec)
	{
	  if (MAPTST(&xpctx->installed, p))
	    break;
	  if (haveone)
	    continue;
	  if (xpctx->conflicts.size && MAPTST(&xpctx->conflicts, p))
	    continue;
	  if (pool->solvables[p].conflicts && expander_checkconflicts(xpctx, p, pool->solvables[p].repo->idarraydata + pool->solvables[p].conflicts, 0, 0) != 0)
	    continue;
	  if (pool->solvables[p].obsoletes && expander_checkconflicts(xpctx, p, pool->solvables[p].repo->idarraydata + pool->solvables[p].obsoletes, 1, 0) != 0)
	    continue;
	  haveone = 1;
	}
      if (p)
	continue;	/* already fulfilled */
      if (haveone)
	queue_push2(&xpctx->todo, rec, s - pool->solvables);
    }
}


void
add_noproviderinfo(ExpanderCtx *xpctx, Id dep, Id who)
{
  Pool *pool = xpctx->pool;
  Reldep *rd, *prd;
  Id p, pp, prov, *provp;
  int nprovinfo;

  if (xpctx->xp->debug)
    {
      if (who)
        expander_dbg(xpctx->xp, "nothing provides %s needed by %s\n", pool_dep2str(pool, dep), expander_solvid2str(xpctx->xp, who));
      else
        expander_dbg(xpctx->xp, "nothing provides %s\n", pool_dep2str(pool, dep));
    }
  if (!ISRELDEP(dep))
    return;
  rd = GETRELDEP(pool, dep);
  if (rd->flags >= 8 || ISRELDEP(rd->name) || ISRELDEP(rd->evr))
    return;
  nprovinfo = 0;
  FOR_PROVIDES(p, pp, rd->name)
    {
      Solvable *s = pool->solvables + p;
      if (!s->repo || !s->provides)
	continue;
      for (provp = s->repo->idarraydata + s->provides; (prov = *provp++) != 0; )
	{
	  if (!ISRELDEP(prov))
	    continue;
	  prd = GETRELDEP(pool, prov);
	  if (prd->name != rd->name || ISRELDEP(prd->evr))
	    continue;
	  queue_push(&xpctx->errors, ERROR_NOPROVIDERINFO);
	  if (prd->name == s->name && prd->evr == s->evr)
	    {
	      if (xpctx->xp->debug)
		expander_dbg(xpctx->xp, "%s has version %s\n", expander_solvid2str(xpctx->xp, p), pool_id2str(pool, prd->evr));
	      queue_push2(&xpctx->errors, prd->evr, 0);
	    }
	  else
	    {
	      if (xpctx->xp->debug)
		expander_dbg(xpctx->xp, "%s provides version %s\n", expander_solvid2str(xpctx->xp, p), pool_id2str(pool, prd->evr));
	      queue_push2(&xpctx->errors, prd->evr, p);
	    }
	  if (++nprovinfo >= 4)
	    return;		/* only show the first 4 providers */
	}
    }
}

