/*
 * transitive_deps.c
 *
 * fetch transitive dependencies
 *
 */

#include "transitive_deps.h"
#include "util.h"

Transitive_Deps *
new_Transitive_Deps(Expander *xp)
{
  Transitive_Deps *td;

  td = (Transitive_Deps *)solv_calloc(1, sizeof(*td));

  td->xp = xp;

  queue_init(&td->in);
  queue_init(&td->out);
  queue_init(&td->indep);
  queue_init(&td->ignoreq);

  td->options = 0;
  td->directdepsend = 0;

  return td;
}

void
transitive_deps_free(Transitive_Deps *td)
{
  if (!td)
    return;

  queue_free(&td->in);
  queue_free(&td->out);
  queue_free(&td->indep);
  queue_free(&td->ignoreq);

  solv_free(td);
}

static Id
dep2id_rec(Pool *pool, char *s)
{
  char *n;
  Id id;
  int flags;

  if ((n = strchr(s, '|')) != 0)
    {
      id = dep2id_rec(pool, n + 1);
      *n = 0;
      id = pool_rel2id(pool, dep2id_rec(pool, s), id, REL_OR, 1);
      *n = '|';
      return id;
    }
  while (*s == ' ' || *s == '\t')
    s++;
  n = s;
  if (pool->disttype == DISTTYPE_RPM)
    {
      /* rpm delimits the name by whitespace only */
      while (*s && *s != ' ' && *s != '\t')
        s++;
    }
  else
    {
      while (*s && *s != ' ' && *s != '\t' && *s != '<' && *s != '=' && *s != '>')
        s++;
    }
#ifdef REL_MULTIARCH
  if (s - n > 4 && s[-4] == ':' && !strncmp(s - 4, ":any", 4))
    {
      id = pool_strn2id(pool, n, s - n - 4, 1);
      id = pool_rel2id(pool, id, ARCH_ANY, REL_MULTIARCH, 1);
    }
  else
#endif
    id = pool_strn2id(pool, n, s - n, 1);
  if (!*s)
    return id;
  while (*s == ' ' || *s == '\t')
    s++;
  flags = 0;
  for (;;s++)
    {
      if (*s == '<')
       flags |= REL_LT;
      else if (*s == '=')
       flags |= REL_EQ;
      else if (*s == '>')
       flags |= REL_GT;
      else
       break;
    }
  if (!flags)
    return id;
  while (*s == ' ' || *s == '\t')
    s++;
  n = s;
  while (*s && *s != ' ' && *s != '\t')
    s++;
  return pool_rel2id(pool, id, pool_strn2id(pool, n, s - n, 1), flags, 1);
}

static Id
parsedep_error(Pool *pool, const char *s)
{
  Id id;
  id = pool_str2id(pool, s, 1);
  return pool_rel2id(pool, pool_str2id(pool, "dependency parse error", 1), id, REL_ERROR, 1);
}

static Id
dep2id(Pool *pool, char *s)
{
  Id id;
  if (pool->disttype == DISTTYPE_RPM && *s == '(')
    {
#if defined(LIBSOLV_FEATURE_COMPLEX_DEPS)
      id = pool_parserpmrichdep(pool, s);
#else
      id = 0;
#endif
    }
  else
    id = dep2id_rec(pool, s);
  if (!id)
    id = parsedep_error(pool, s);
  return id;
}

void
transitive_deps_pre_expand(Transitive_Deps *td, char *dep)
{
  char *s = dep;
  if (*s == '-' && s[1] == '-')
  {
    /* expand option */
    if (!strcmp(s, "--ignoreignore--"))
      td->options |= EXPANDER_OPTION_IGNOREIGNORE;
    else if (!strcmp(s, "--directdepsend--"))
      td->directdepsend = 1;
    else if (!strcmp(s, "--dorecommends--"))
      td->options |= EXPANDER_OPTION_DORECOMMENDS;
    else if (!strcmp(s, "--dosupplements--"))
      td->options |= EXPANDER_OPTION_DOSUPPLEMENTS | EXPANDER_OPTION_USESUPPLEMENTSFORCHOICES;
    else if (!strcmp(s, "--ignoreconflicts--"))
      td->options |= EXPANDER_OPTION_IGNORECONFLICTS;

    return;
  }

  Pool *pool = td->xp->pool;
  Id id = 0;

  if (*s == '-')
  {
    /* ignore dependency */
    id = pool_str2id(pool, s + 1, 1);
    queue_push(&td->ignoreq, id);

    return;
  }

  int deptype = DEPTYPE_REQUIRES;
  if (*s == '!')
  {
    deptype = DEPTYPE_CONFLICTS;
    s++;
    if (*s == '!')
    {
      deptype = DEPTYPE_OBSOLETES;
      s++;
    }
  }

  id = dep2id(pool, s);

  if (deptype == DEPTYPE_REQUIRES && !td->directdepsend)
    queue_push(&td->in, id);
  else
    queue_push2(&td->indep, deptype, id);

  return;
}

extern int expander_expand(Expander *xp, Queue *in, Queue *indep, Queue *out, Queue *ignoreq, int options);

int
transitive_deps_expand(Transitive_Deps *td)
{
  return expander_expand(td->xp, &td->in, &td->indep, &td->out, &td->ignoreq, td->options);
}

const char *
transitive_deps_get_dep(Transitive_Deps *td, int i)
{
  Pool *pool = td->xp->pool;
  Solvable *s = pool->solvables + td->out.elements[i];
  return pool_id2str(pool, s->name);
}

static inline const char *
solvid2name(Pool *pool, Id p)
{
  return pool_id2str(pool, pool->solvables[p].name);
}

static int
pkgname_sort_cmp(const void *ap, const void *bp, void *dp)
{
  Pool *pool = (Pool *)dp;
  Id a = *(Id *)ap;
  Id b = *(Id *)bp;
  return strcmp(pool_id2str(pool, pool->solvables[a].name), pool_id2str(pool, pool->solvables[b].name));
}

int
transitive_deps_get_expand_error(Transitive_Deps *td, int i, char *buf)
{
  Queue *out = &td->out;
  Pool *pool = td->xp->pool;

  if (i >= out->count)
    return 0;

  Id id, who;

  Id type = out->elements[i];
  if (type == ERROR_NOPROVIDER)
    {
      id = out->elements[i + 1];
      who = out->elements[i + 2];
      if (who)
        sprintf(buf, "nothing provides %s needed by %s", pool_dep2str(pool, id), solvid2name(pool, who));
      else
        sprintf(buf, "nothing provides %s", pool_dep2str(pool, id));
      return i + 3;
    }
  else if (type == ERROR_ALLCONFLICT)
    {
      id = out->elements[i + 1];
      who = out->elements[i + 2];
      if (who)
        sprintf(buf, "%s conflicts with always true %s", solvid2name(pool, who), pool_dep2str(pool, id));
      else
        sprintf(buf, "conflict with always true %s", pool_dep2str(pool, id));
      return i + 3;
    }
  else if (type == ERROR_CONFLICT)
    {
      who = out->elements[i + 1];
      Id who2 = out->elements[i + 2];
      if (!who && who2 >= 0)
        sprintf(buf, "conflicts with %s", solvid2name(pool, who2));
      else if (who2 < 0)
        sprintf(buf, "%s obsoletes %s", solvid2name(pool, who), solvid2name(pool, -who2));
      else
        sprintf(buf, "%s conflicts with %s", solvid2name(pool, who), solvid2name(pool, who2));
      return i + 3;
    }
  else if (type == ERROR_CONFLICT2)
    {
      who = out->elements[i + 1];
      Id who2 = out->elements[i + 2];
      if (who2 < 0)
        sprintf(buf, "%s is obsoleted by %s", solvid2name(pool, who), solvid2name(pool, -who2));
      else if (who2 > 0)
        sprintf(buf, "%s is in conflict with %s", solvid2name(pool, who), solvid2name(pool, who2));
      else
        sprintf(buf, "%s is in conflict", solvid2name(pool, who));
      return i + 3;
    }
  else if (type == ERROR_CONFLICTINGPROVIDERS)
    {
      id = out->elements[i + 1];
      who = out->elements[i + 2];
      if (who)
        sprintf(buf, "conflict for providers of %s needed by %s", pool_dep2str(pool, id), solvid2name(pool, who));
      else
        sprintf(buf, "conflict for providers of %s", pool_dep2str(pool, id));
      return i + 3;
    }
  else if (type == ERROR_PROVIDERINFO)
    {
      who = out->elements[i + 1];
      Id who2 = out->elements[i + 2];
      if (who2 < 0)
        sprintf(buf, "(provider %s obsoletes %s)", solvid2name(pool, who), solvid2name(pool, -who2));
      else
        sprintf(buf, "(provider %s conflicts with %s)", solvid2name(pool, who), solvid2name(pool, who2));
      return i + 3;
    }
  else if (type == ERROR_PROVIDERINFO2)
    {
      who = out->elements[i + 1];
      Id who2 = out->elements[i + 2];
      if (who2 < 0)
        sprintf(buf, "(provider %s is obsoleted by %s)", solvid2name(pool, who), solvid2name(pool, -who2));
      else if (who2 > 0)
        sprintf(buf, "(provider %s is in conflict with %s)", solvid2name(pool, who), solvid2name(pool, who2));
      else
        sprintf(buf, "(provider %s is in conflict)", solvid2name(pool, who));
      return i + 3;
    }
  else if (type == ERROR_CHOICE)
    {
      int j;
      char *str = "";
      for (j = i + 3; out->elements[j]; j++)
        ;
      solv_sort(out->elements + i + 3, j - (i + 3), sizeof(Id), pkgname_sort_cmp, pool);
      for (j = i + 3; out->elements[j]; j++)
        {
          Solvable *s = pool->solvables + out->elements[j];
          str = pool_tmpjoin(pool, str, " ", pool_id2str(pool, s->name));
        }
      if (*str)
        str++;        /* skip starting ' ' */
      id = out->elements[i + 1];
      who = out->elements[i + 2];
      if (who)
        sprintf(buf, "have choice for %s needed by %s: %s", pool_dep2str(pool, id), solvid2name(pool, who), str);
      else
        sprintf(buf, "have choice for %s: %s", pool_dep2str(pool, id), str);

      return j + 1;
    }
  else if (type == ERROR_BADDEPENDENCY)
    {
      id = out->elements[i + 1];
      who = out->elements[i + 2];
      if (who)
        sprintf(buf, "cannot parse dependency %s from %s", pool_dep2str(pool, id), solvid2name(pool, who));
      else
        sprintf(buf, "cannot parse dependency %s", pool_dep2str(pool, id));
      return i + 3;
    }
  else if (type == ERROR_NOPROVIDERINFO)
    {
      id = out->elements[i + 1];
      who = out->elements[i + 2];
      if (who)
        sprintf(buf, "(got version %s provided by %s)", pool_id2str(pool, id), solvid2name(pool, who));
      else
        sprintf(buf, "(got version %s)", pool_id2str(pool, id));
      return i + 3;
    }

  return 0;
}
