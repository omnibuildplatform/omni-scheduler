/*
 * expander_expand.c
 *
 * expander expand function
 *
 */

#include "expander_ctx.h"
#include "common.h"

#define ISCPLX(pool, d) (ISRELDEP(d) && GETRELID(d) >= pool->nrels)
#define GETCPLX(pool, d) (GETRELID(d) - pool->nrels)

static void
expander_growmaps(Expander *xp)
{
  Pool *pool = xp->pool;
  MAPEXP(&xp->ignored, pool->ss.nstrings);
  MAPEXP(&xp->ignoredx, pool->ss.nstrings);
  MAPEXP(&xp->preferpos, pool->ss.nstrings);
  MAPEXP(&xp->preferposx, pool->ss.nstrings);
  MAPEXP(&xp->preferneg, pool->ss.nstrings);
  MAPEXP(&xp->prefernegx, pool->ss.nstrings);
  MAPEXP(&xp->conflicts, pool->ss.nstrings);
}

static int
expander_expand(Expander *xp, Queue *in, Queue *indep, Queue *out, Queue *ignoreq, int options)
{
  ExpanderCtx xpctx;
  Pool *pool = xp->pool;
  Queue toinstall;
  Queue qq, choices;
  Solvable *s;
  Id q, p, pp;
  int i, j, nerrors;
  int ti, tj, tc;
  Id todoid, id, who, whon;
  Id conflprovpc;
  int pass;
  Queue revertignore;
  int oldignoreignore = xp->ignoreignore;
  Map oldignored, oldignoredx;
  int ignoremapssaved = 0;
  int dorecstart = 0;

  memset(&xpctx, 0, sizeof(xpctx));
  xpctx.xp = xp;
  xpctx.pool = pool;
  xpctx.out = out;
  xpctx.ignoreignore = options & EXPANDER_OPTION_IGNOREIGNORE ? 1 : xp->ignoreignore;
  xpctx.ignoreconflicts = options & EXPANDER_OPTION_IGNORECONFLICTS ? 1 : xp->ignoreconflicts;
  xpctx.userecommendsforchoices = options & EXPANDER_OPTION_USERECOMMENDSFORCHOICES ? 1 : xp->userecommendsforchoices;
  xpctx.usesupplementsforchoices = options & EXPANDER_OPTION_USESUPPLEMENTSFORCHOICES ? 1 : xp->usesupplementsforchoices;
  xpctx.dorecommends = options & EXPANDER_OPTION_DORECOMMENDS ? 1 : xp->dorecommends;
  xpctx.dosupplements = options & EXPANDER_OPTION_DOSUPPLEMENTS ? 1 : xp->dosupplements;
  map_init(&xpctx.installed, pool->nsolvables);
  map_init(&xpctx.conflicts, 0);
  map_init(&xpctx.recommended, 0);
  queue_init(&xpctx.conflictsinfo);
  queue_init(&xpctx.todo);
  queue_init(&xpctx.todo_cond);
  map_init(&xpctx.todo_condmap, 0);
  queue_init(&xpctx.errors);
  queue_init(&xpctx.cplxq);
  queue_init(&xpctx.cplxblks);
  queue_init(&xpctx.pruneq);

  queue_init(&toinstall);
  queue_init(&qq);
  queue_init(&choices);
  queue_init(&revertignore);

  queue_empty(out);

  /* process ignored. hack: we mess with the ignore config in xp */
  xp->ignoreignore = 0;
  if (xpctx.ignoreignore && ignoreq->count)
    {
      /* bad: have direct ignores and we need to zero the project config ignores */
      oldignored = xp->ignored;
      oldignoredx = xp->ignoredx;
      ignoremapssaved = 1;
      /* clear project config maps */
      memset(&xp->ignored, 0, sizeof(xp->ignored));
      memset(&xp->ignoredx, 0, sizeof(xp->ignoredx));
    }
  if (ignoreq->count)
    {
      /* mix direct ignores with ignores from project config */
      for (i = 0; i < ignoreq->count; i++)
	{
	  const char *ss;
	  id = ignoreq->elements[i];
	  MAPEXP(&xp->ignored, id);
	  if (MAPTST(&xp->ignored, id))
	    continue;
	  MAPSET(&xp->ignored, id);
	  queue_push(&revertignore, id);
	  if ((ss = strchr(pool_id2str(pool, id), ':')) != 0)
	    {
	      id = str2id_dup(pool, ss + 1);
	      MAPEXP(&xp->ignoredx, id);
	      if (MAPTST(&xp->ignoredx, id))
		continue;
	      MAPSET(&xp->ignoredx, id);
	      queue_push(&revertignore, -id);
	    }
	}
    }
  else if (xpctx.ignoreignore)
    {
      /* no direct ignores, ignore project config ignores.
       * easy: just disable ignore processing */
      xp->ignoreignore = 1;
    }

  /* grow maps to make bit tests cheaper */
  expander_growmaps(xp);

  /* process standard dependencies */
  if (indep)
    {
      for (i = 0; i < indep->count; i += 2)
	{
	  int deptype = indep->elements[i];
	  Id dep = indep->elements[i + 1];
	  if (ISRELDEP(dep) && GETRELDEP(pool, dep)->flags == REL_ERROR)
	    {
	      queue_push(&xpctx.errors, ERROR_BADDEPENDENCY);
	      queue_push2(&xpctx.errors, GETRELDEP(pool, dep)->evr, 0);
	      continue;
	    }
	  if ((deptype == DEPTYPE_REQUIRES || deptype == DEPTYPE_CONFLICTS) && pool_is_complex_dep(pool, dep))
	    {
	      expander_installed_complexdep(&xpctx, 0, dep, deptype);
	      continue;
	    }
	  if (deptype == DEPTYPE_REQUIRES)
	    {
	      queue_push2(&xpctx.todo, dep, 0);
	    }
	  else if (deptype == DEPTYPE_CONFLICTS || deptype == DEPTYPE_OBSOLETES)
	    {
	      FOR_PROVIDES(p, pp, dep)
		{
		  if (deptype == DEPTYPE_OBSOLETES && !pool_match_nevr(pool, pool->solvables + p, dep))
		    continue;
		  MAPEXP(&xpctx.conflicts, pool->nsolvables);
		  MAPSET(&xpctx.conflicts, p);
		}
	    }
	}
    }
  /* process direct dependencies */
  for (i = 0; i < in->count; i++)
    {
      id = in->elements[i];
      if (ISRELDEP(id) && GETRELDEP(pool, id)->flags == REL_ERROR)
	{
	  queue_push(&xpctx.errors, ERROR_BADDEPENDENCY);
	  queue_push2(&xpctx.errors, GETRELDEP(pool, id)->evr, 0);
	  continue;
	}
      if (pool_is_complex_dep(pool, id))
	{
	  expander_installed_complexdep(&xpctx, 0, id, DEPTYPE_REQUIRES);
	  continue;
	}
      q = 0;
      FOR_PROVIDES(p, pp, id)
	{
	  s = pool->solvables + p;
	  if (!pool_match_nevr(pool, s, id))
	    continue;
	  if (q)
	    {
	      q = 0;
	      break;
	    }
	  q = p;
	}
      if (!q)
	{
	  queue_push2(&xpctx.todo, id, 0);	/* unclear, resolve later */
	  continue;
	}
      if (xp->debug)
	expander_dbg(xp, "added %s because of %s (direct dep)\n", expander_solvid2name(xp, q), pool_dep2str(pool, id));
      queue_push(&toinstall, q);
    }

  /* unify toinstall, check against conflicts */
  for (i = 0; i < toinstall.count; i++)
    {
      p = toinstall.elements[i];
      MAPSET(&xpctx.installed, p);
    }
  for (i = j = 0; i < toinstall.count; i++)
    {
      p = toinstall.elements[i];
      if (!MAPTST(&xpctx.installed, p))
	continue;
      MAPCLR(&xpctx.installed, p);
      toinstall.elements[j++] = p;
    }
  queue_truncate(&toinstall, j);
  if (xpctx.conflicts.size)
    {
      for (i = 0; i < toinstall.count; i++)
	{
	  p = toinstall.elements[i];
	  if (MAPTST(&xpctx.conflicts, p))
	    findconflictsinfo(&xpctx, p, 2);
	}
    }

  /* here is the big expansion loop */
  pass = 0;
  while (!xpctx.errors.count)
    {
      if (toinstall.count)
	{
	  expander_installed_multiple(&xpctx, &toinstall);
	  pass = 0;
	  continue;
	}

      if (!xpctx.todo.count)
	{
	  /* almost finished. now do weak deps if requested */
	  pass = 0;
	  if (xpctx.dorecommends)
	    {
	      expander_dbg(xp, "--- now doing recommended packages\n");
	      for (; dorecstart < out->count; dorecstart++)
		{
		  s = pool->solvables + out->elements[dorecstart];
		  if (s->recommends)
		    add_recommended_packages(&xpctx, s);
		}
	      if (xpctx.todo.count)
	        continue;
	    }
	  if (xpctx.dosupplements)
	    {
	      Id sup, *supp;
	      expander_dbg(xp, "--- now doing supplemented packages\n");
	      for (p = 1; p < pool->nsolvables; p++)
		{
		  s = pool->solvables + p;
		  if (!s->supplements || !s->repo)
		    continue;
		  if (MAPTST(&xpctx.installed, p))
		    continue;
		  if (!pool_installable(pool, s))
		    continue;
		  if (xpctx.conflicts.size && MAPTST(&xpctx.conflicts, p))
		    continue;
		  if (s->conflicts && expander_checkconflicts(&xpctx, p, s->repo->idarraydata + s->conflicts, 0, 0) != 0)
		    continue;
		  if (s->obsoletes && expander_checkconflicts(&xpctx, p, s->repo->idarraydata + s->obsoletes, 1, 0) != 0)
		    continue;
		  supp = s->repo->idarraydata + s->supplements;
		  while ((sup = *supp++) != 0)
		    if (expander_dep_fulfilled(&xpctx, sup))
		      break;
		  if (!sup)
		    continue;
		  expander_dbg(xp, "added %s because it supplements %s\n", expander_solvid2name(xp, p), pool_dep2str(pool, sup));
		  queue_push(&toinstall, p);
		}
	      if (toinstall.count)
		continue;
	    }
	  /* no new stuff to do, we're finished! */
	  break;
	}

      expander_dbg(xp, "--- now doing normal dependencies\n");

      if (pass == 1)
	queue_empty(&choices);
	
      for (ti = tj = 0; ti < xpctx.todo.count; ti += 2)
	{
	  int deptype = DEPTYPE_REQUIRES;
	  todoid = id = xpctx.todo.elements[ti];
	  who = xpctx.todo.elements[ti + 1];
	  if (!id)			/* deleted entry? */
	    continue;
	  queue_empty(&qq);
	  if (ISCPLX(pool, id))
	    {
	      pp = GETCPLX(pool, id);	/* p, dep, deptype, ids... */
	      id = xpctx.cplxblks.elements[pp + 1];
	      deptype = xpctx.cplxblks.elements[pp + 2];
	      pp += 3;
	      while ((p = xpctx.cplxblks.elements[pp++]))
		if (p > 0)
		  queue_push(&qq, p);
	    }
	  else
	    {
	      FOR_PROVIDES(p, pp, id)
		queue_push(&qq, p);
	    }

	  if (qq.count == 0)
	    {
	      if (deptype == DEPTYPE_RECOMMENDS)
		continue;
	      queue_push(&xpctx.errors, ERROR_NOPROVIDER);
	      queue_push2(&xpctx.errors, id, who);
	      add_noproviderinfo(&xpctx, id, who);
	      continue;
	    }

	  /* check installed and ignores */
	  whon = who ? pool->solvables[who].name : 0;
	  for (i = 0; i < qq.count; i++)
	    {
	      p = qq.elements[i];
	      if (MAPTST(&xpctx.installed, p))
		break;
	      if (who && deptype == DEPTYPE_REQUIRES && !xp->ignoreignore)
		{
		  Id pn = pool->solvables[p].name;
		  if (MAPTST(&xp->ignored, pn))
		    break;
		  if (MAPTST(&xp->ignoredx, pn))
		    {
		      Id xid = pool_str2id(pool, pool_tmpjoin(pool, pool_id2str(pool, whon), ":", pool_id2str(pool, pn)), 0);
		      if (xid && MAPTST(&xp->ignored, xid))
			break;
		    }
		}
	    }
	  if (i < qq.count)
	    continue;		/* ignored dependency or fulfilled */

	  if (pass == 0 && qq.count > 1)
	    {
	      xpctx.todo.elements[tj++] = todoid;
	      xpctx.todo.elements[tj++] = who;
	      continue;
	    }

	  /* do conflict pruning */
	  conflprovpc = 0;
	  for (i = j = 0; i < qq.count; i++)
	    {
	      Id pc;
	      p = qq.elements[i];
	      if (xpctx.conflicts.size && MAPTST(&xpctx.conflicts, p))
		{
		  if (xp->debug)
		    findconflictsinfo(&xpctx, p, 0);
		  conflprovpc = 0;
		  continue;
		}
	      if (pool->solvables[p].conflicts && (pc = expander_checkconflicts(&xpctx, p, pool->solvables[p].repo->idarraydata + pool->solvables[p].conflicts, 0, 0)) != 0)
		{
		  conflprovpc = pc;
		  continue;
		}
	      if (pool->solvables[p].obsoletes && (pc = expander_checkconflicts(&xpctx, p, pool->solvables[p].repo->idarraydata + pool->solvables[p].obsoletes, 1, 0)) != 0)
		{
		  conflprovpc = -pc;
		  continue;
		}
	      qq.elements[j++] = p;
	    }
	  if (j == 0)
	    {
	      if (deptype == DEPTYPE_RECOMMENDS)
		continue;
	      queue_push(&xpctx.errors, ERROR_CONFLICTINGPROVIDERS);
	      queue_push2(&xpctx.errors, id, who);
	      if (qq.count == 1 && conflprovpc != 1 && conflprovpc != -1)
		{
		  p = qq.elements[0];
		  if (conflprovpc)
		    {
		      queue_push(&xpctx.errors, ERROR_PROVIDERINFO);
		      queue_push2(&xpctx.errors, p, conflprovpc);
		      continue;
		    }
		  findconflictsinfo(&xpctx, p, 1);
		  continue;
		}
	      /* even more work if all providers conflict */
	      for (j = 0; j < qq.count; j++)
		{
		  p = qq.elements[j];
		  if (xpctx.conflicts.size && MAPTST(&xpctx.conflicts, p))
		    findconflictsinfo(&xpctx, p, 1);
		  if (pool->solvables[p].conflicts)
		    expander_checkconflicts(&xpctx, p, pool->solvables[p].repo->idarraydata + pool->solvables[p].conflicts, 0, 1);
		  if (pool->solvables[p].obsoletes)
		    expander_checkconflicts(&xpctx, p, pool->solvables[p].repo->idarraydata + pool->solvables[p].obsoletes, 1, 1);
		}
	      continue;
	    }
	  queue_truncate(&qq, j);
          if (qq.count == 1)
	    {
	      p = qq.elements[0];
	      if (xp->debug)
		expander_dbg(xp, "added %s because of %s:%s\n", expander_solvid2name(xp, p), whon ? pool_id2str(pool, whon) : "(direct)", pool_dep2str(pool, id));
	      queue_push(&toinstall, p);
	      continue;
	    }
	  /* pass is == 1 and we have multiple choices */
	  if (xp->debug)
	    {
	      expander_dbg(xp, "undecided about %s:%s:", whon ? pool_id2str(pool, whon) : "(direct)", pool_dep2str(pool, id));
	      for (i = 0; i < qq.count; i++)
		expander_dbg(xp, " %s", expander_solvid2name(xp, qq.elements[i]));
              expander_dbg(xp, "\n");
	    }
	  queue_push2(&choices, qq.count + 3, id);
	  queue_push(&choices, qq.count);
	  queue_insertn(&choices, choices.count, qq.count, qq.elements);
	  xpctx.todo.elements[tj++] = todoid;
	  xpctx.todo.elements[tj++] = who;
	}
      queue_truncate(&xpctx.todo, tj);

      if (toinstall.count)
	continue;

      if (!xpctx.todo.count)
	continue;

      /* did not find a package to install, only choices left on todo list */
      if (pass == 0)
	{
	  pass = 1;	/* now do conflict pruning */
	  continue;
	}

      expander_dbg(xp, "--- now doing undecided dependencies\n");

      /* prune prefers */
      for (ti = tc = 0; ti < xpctx.todo.count; ti += 2)
	{
	  Id who = xpctx.todo.elements[ti + 1];
	  Id *qe = choices.elements + tc + 3;
	  Id id = choices.elements[tc + 1];
	  int qn = choices.elements[tc + 2];
	  whon = who ? pool->solvables[who].name : 0;
	  if (qn > 1)
	    qn = prune_neg_prefers(&xpctx, who, qe, qn);
	  if (qn > 1)
	    qn = prune_pos_prefers(&xpctx, who, qe, qn, 0);
	  if (qn == 1)
	    {
	      p = qe[0];
	      if (xp->debug)
		expander_dbg(xp, "added %s because of %s:%s\n", expander_solvid2name(xp, p), whon ? pool_id2str(pool, whon) : "(direct)", pool_dep2str(pool, id));
	      queue_push(&toinstall, p);
	      xpctx.todo.elements[ti] = 0;	/* kill entry */
	    }
	  choices.elements[tc + 2] = qn;
	  tc += choices.elements[tc];
	}
      if (toinstall.count)
	continue;

      /* prune pos prefers with domulti and debian or */
      for (ti = tc = 0; ti < xpctx.todo.count; ti += 2)
	{
	  Id who = xpctx.todo.elements[ti + 1];
	  Id *qe = choices.elements + tc + 3;
	  Id id = choices.elements[tc + 1];
	  int qn = choices.elements[tc + 2];
	  whon = who ? pool->solvables[who].name : 0;
	  if (qn > 1)
	    qn = prune_pos_prefers(&xpctx, who, qe, qn, 1);
	  if (qn > 1 && pool->disttype != DISTTYPE_RPM)
	    {
	      if (ISRELDEP(id) && GETRELDEP(pool, id)->flags == REL_OR)
	        qn = prune_or_dep(&xpctx, id, qe, qn);
	    }
	  if (qn == 1)
	    {
	      p = qe[0];
	      if (xp->debug)
		expander_dbg(xp, "added %s because of %s:%s\n", expander_solvid2name(xp, p), whon ? pool_id2str(pool, whon) : "(direct)", pool_dep2str(pool, id));
	      queue_push(&toinstall, p);
	      xpctx.todo.elements[ti] = 0;	/* kill entry */
	    }
	  choices.elements[tc + 2] = qn;
	  tc += choices.elements[tc];
	}
      if (toinstall.count)
	continue;

      /* prune recommended packages */
      if (xpctx.userecommendsforchoices)
        expander_updaterecommendedmap(&xpctx);
      if (xpctx.recommended.size)
	{
	  expander_dbg(xp, "now doing undecided dependencies with recommends\n");
	  for (ti = tc = 0; ti < xpctx.todo.count; ti += 2)
	    {
	      Id who = xpctx.todo.elements[ti + 1];
	      Id *qe = choices.elements + tc + 3;
	      Id id = choices.elements[tc + 1];
	      int qn = choices.elements[tc + 2];
	      whon = who ? pool->solvables[who].name : 0;
	      for (i = j = 0; i < qn; i++)
		if (MAPTST(&xpctx.recommended, qe[i]))
		  qe[j++] = qe[i];
	      if (j)
		qn = j;
	      if (qn == 1)
		{
		  p = qe[0];
		  if (xp->debug)
		    expander_dbg(xp, "added %s because of %s:%s\n", expander_solvid2name(xp, p), whon ? pool_id2str(pool, whon) : "(direct)", pool_dep2str(pool, id));
		  queue_push(&toinstall, p);
		  xpctx.todo.elements[ti] = 0;	/* kill entry */
		}
	      choices.elements[tc + 2] = qn;
	      tc += choices.elements[tc];
	    }
	  if (toinstall.count)
	    continue;
	}
      if (xpctx.usesupplementsforchoices)
	{
	  expander_dbg(xp, "now doing undecided dependencies with supplements\n");
	  for (ti = tc = 0; ti < xpctx.todo.count; ti += 2)
	    {
	      Id who = xpctx.todo.elements[ti + 1];
	      Id *qe = choices.elements + tc + 3;
	      Id id = choices.elements[tc + 1];
	      int qn = choices.elements[tc + 2];
	      whon = who ? pool->solvables[who].name : 0;
	      qn = prune_supplemented(&xpctx, qe, qn);
	      if (qn == 1)
		{
		  p = qe[0];
		  if (xp->debug)
		    expander_dbg(xp, "added %s because of %s:%s\n", expander_solvid2name(xp, p), whon ? pool_id2str(pool, whon) : "(direct)", pool_dep2str(pool, id));
		  queue_push(&toinstall, p);
		  xpctx.todo.elements[ti] = 0;	/* kill entry */
		}
	      choices.elements[tc + 2] = qn;
	      tc += choices.elements[tc];
	    }
	  if (toinstall.count)
	    continue;
	}

      /* nothing more to prune. record errors. */
      for (ti = tc = 0; ti < xpctx.todo.count; ti += 2)
	{
	  Id who = xpctx.todo.elements[ti + 1];
	  Id *qe = choices.elements + tc + 3;
	  Id id = choices.elements[tc + 1];
	  int qn = choices.elements[tc + 2];
	  queue_push(&xpctx.errors, ERROR_CHOICE);
	  queue_push2(&xpctx.errors, id, who);
	  for (i = 0; i < qn; i++)
	    queue_push(&xpctx.errors, qe[i]);
	  queue_push(&xpctx.errors, 0);
	  tc += choices.elements[tc];
	}
    }

  /* free data */
  map_free(&xpctx.installed);
  map_free(&xpctx.conflicts);
  map_free(&xpctx.recommended);
  map_free(&xpctx.todo_condmap);
  queue_free(&xpctx.conflictsinfo);
  queue_free(&xpctx.todo_cond);
  queue_free(&xpctx.todo);
  queue_free(&toinstall);
  queue_free(&qq);
  queue_free(&choices);
  queue_free(&xpctx.pruneq);
  queue_free(&xpctx.cplxq);
  queue_free(&xpctx.cplxblks);

  /* revert ignores */
  xp->ignoreignore = oldignoreignore;
  if (ignoremapssaved)
    {
      map_free(&xp->ignored);
      map_free(&xp->ignoredx);
      xp->ignored = oldignored;
      xp->ignoredx = oldignoredx;
    }
  else
    {
      for (i = 0; i < revertignore.count; i++)
	{
	  id = revertignore.elements[i];
	  if (id > 0)
	    MAPCLR(&xp->ignored, id);
	  else
	    MAPCLR(&xp->ignoredx, -id);
	}
    }
  queue_free(&revertignore);

  /* finish return queue, count errors */
  nerrors = 0;
  if (xpctx.errors.count)
    {
      queue_empty(out);
      queue_insertn(out, 0, xpctx.errors.count, xpctx.errors.elements);
      for (i = 0; i < out->count; i += 3)
	{
	  nerrors++;
	  if (out->elements[i] == ERROR_CHOICE)
	    while (out->elements[i + 3])
	      i++;
	}
    }
  queue_free(&xpctx.errors);
  return nerrors;
}

