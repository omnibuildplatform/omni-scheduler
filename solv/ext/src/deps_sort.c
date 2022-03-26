/*
 * deps_sort.c
 *
 * sort the dependencies
 *
 */

#include "deps_sort.h"
#include "util.h"

#define HASHCHAIN_START 7
#define HASHCHAIN_NEXT(h, hh, mask) (((h) + (hh)++) & (mask))

Deps_Sort *
deps_sort_create()
{
  Deps_Sort *ds;

  ds = (Deps_Sort *)solv_calloc(1, sizeof(*ds));

  queue_init(&ds->out);
  queue_init(&ds->cycles);

  return ds;
}

void
deps_sort_free(Deps_Sort *ds)
{
  if (!ds)
    return;

  queue_free(&ds->out);
  queue_free(&ds->cycles);

  queue_free(&ds->edata);
  queue_free(&ds->vedge);

  map_free(&ds->edgeunifymap);

  solv_free(ds->ht);
  solv_free(ds->names);
  solv_free(ds->depnames);

  solv_free(ds);
}

void
deps_sort_add_pkg_init(Deps_Sort *ds, int packs_num)
{
  ds->hashmask = mkmask(packs_num + 4); // maybe pass a bigger number to make mask?
  ds->ht = solv_calloc(ds->hashmask + 1, sizeof(*ds->ht));
  ds->names = solv_calloc(packs_num + 1, sizeof(char *));
  ds->depnames = solv_calloc(packs_num + 1, sizeof(char *));
  ds->packs_num = packs_num;
  ds->nnames = 1; // 0 is reserved.
}

int
deps_sort_add_pkg_src(Deps_Sort *ds, const char *pkg, const char *src)
{
  Id id = ds->nnames;
  if (id > ds->packs_num)
    return 0;

  ds->nnames++;
  ds->names[id] = solv_strdup(pkg);
  ds->depnames[id] = solv_strdup(src);

  Hashtable ht = ds->ht;
  Hashval hm = ds->hashmask;

  Hashval h = strhash(src) & hm;
  Hashval hh = HASHCHAIN_START;
  while (ht[h] != 0)
    h = HASHCHAIN_NEXT(h, hh, hm);
  ht[h] = id;

  return id;
}

void
deps_sort_add_dep_init(Deps_Sort *ds)
{
  queue_init(&ds->edata);
  queue_init(&ds->vedge);

  queue_push(&ds->vedge, 0);
  queue_push(&ds->edata, 0);
  map_init(&ds->edgeunifymap, ds->nnames - 1);
}

int
deps_sort_pre_add_dep_of_pkg(Deps_Sort *ds)
{
  int edgestart = ds->edata.count;
  queue_push(&ds->vedge, edgestart);
  return edgestart;
}

void
deps_sort_add_dep_of_pkg(Deps_Sort *ds, int pkgid, const char *depsrc)
{
  Hashtable ht = ds->ht;
  Hashval hm = ds->hashmask;

  Hashval h = strhash(depsrc) & hm;
  Hashval hh = HASHCHAIN_START;

  h = strhash(depsrc) & hm;
  hh = HASHCHAIN_START;
  Id id;
  while ((id = ht[h]) != 0)
  {
    if (!strcmp(ds->depnames[id], depsrc))
    {
      if (id != pkgid && !MAPTST(&ds->edgeunifymap, id))
      {
        MAPSET(&ds->edgeunifymap, id);
        queue_push(&ds->edata, id);
      }
    }
    h = HASHCHAIN_NEXT(h, hh, hm);
  }
}

void
deps_sort_post_add_dep_of_pkg(Deps_Sort *ds, int edgestart)
{
  int j;
  Queue *edata = &ds->edata;

  for (j = edgestart; j < edata->count; j++)
  {
#ifdef MAPCLR_AT
    MAPCLR_AT(&ds->edgeunifymap, edata->elements[j]);
#else
    MAPCLR(&ds->edgeunifymap, edata->elements[j]);
#endif
  }

  queue_push(edata, 0); /* terminate edge array */
}

struct scc_data {
  Id *edata;
  Id *vedge;
  Queue *sccs;
  int *stack;
  int nstack;
  int *low;
  int idx;
};

static void
scc_collect(struct scc_data *scc, int node)
{
  int *low = scc->low;
  Id *e;
  queue_push(scc->sccs, node);
  low[node] = -1;
  for (e = scc->edata + scc->vedge[node]; *e; e++)
    if (*e != -1 && low[*e] > 0)
      scc_collect(scc, *e);
}

/* Tarjan's SCC algorithm */
static int
scc_visit(struct scc_data *scc, int node)
{
  int l, myidx, *low = scc->low, nontrivial = 0;
  Id *e;
  low[node] = myidx = scc->idx++;
  for (e = scc->edata + scc->vedge[node]; *e; e++)
    {
      if (*e == -1 || *e == node)
        continue;
      if (!(l = low[*e]))
        l = scc_visit(scc, *e);
      if (l > 0)
        nontrivial = 1;
      if (l > 0 && l < low[node])
        low[node] = l;
    }
  if (low[node] != myidx)
    return low[node];
  low[node] = -1;
  if (nontrivial)
    {
      scc_collect(scc, node);
      queue_push(scc->sccs, 0);
    }
  return -1;
}

static void
find_sccs(Queue *edata, Queue *vedge, Queue *sccs)
{
  struct scc_data scc;
  int i;
  scc.edata = edata->elements;
  scc.vedge = vedge->elements;
  scc.sccs = sccs;
  scc.low = solv_calloc(vedge->count, sizeof(int));
  scc.idx = 1;
  for (i = 1; i < vedge->count; i++)
    if (!scc.edata[vedge->elements[i]])
      scc.low[i] = -1;
  for (i = 1; i < vedge->count; i++)
    if (!scc.low[i])
      scc_visit(&scc, i);
  solv_free(scc.low);
}

int
deps_sort_start(Deps_Sort *ds, int depsortsccs)
{
  Queue todo;
  Queue cycles;

  queue_init(&todo);
  queue_init(&cycles);

  Queue *vedge = &ds->vedge;

  int i, j, k, cy, didsccs, cycstart, failed = 0;
  Id *e;
  Id *mark = solv_calloc(vedge->count, sizeof(Id));
  for (i = vedge->count - 1; i; i--)
    queue_push(&todo, i);

  Queue *out = &ds->out;
  while (todo.count)
  {
    i = queue_pop(&todo);
    if (i < 0)
    {
      i = -i;
      mark[i] = 2;
      queue_push(out, i);
      continue;
    }
    if (mark[i] == 2)
      continue;
    if (mark[i] == 0)
    {
      int edgestovisit = 0;
      e = ds->edata.elements + vedge->elements[i];
      for (; *e; e++)
      {
        if (*e == -1)
          continue;        /* broken */
        if (mark[*e] == 2)
          continue;
        if (!edgestovisit++)
          queue_push(&todo, -i);
        queue_push(&todo, *e);
      }
      if (!edgestovisit)
      {
        mark[i] = 2;
        queue_push(out, i);
      }
      else
        mark[i] = 1;
      continue;
    }
    /* oh no, we found a cycle, record and break it */
    if (depsortsccs && !didsccs)
    {
      /* use Tarjan's SCC algorithm */
      find_sccs(&ds->edata, vedge, &cycles);
      queue_push(&cycles, 0);
      didsccs = cycles.count;
    }
    cy = cycles.count;
    for (j = todo.count - 1; j >= 0; j--)
      if (todo.elements[j] == -i)
        break;
    cycstart = j;
    // printf("cycle:\n");
    for (j = cycstart; j < todo.count; j++)
      if (todo.elements[j] < 0)
      {
        k = -todo.elements[j];
        mark[k] = 0;
        queue_push(&cycles, k);
        // printf("  %d\n", k);
      }
    queue_push(&cycles, 0);
    todo.elements[cycstart] = i;
    /* break it */
    for (k = cy; cycles.elements[k]; k++)
      ;
    if (!cycles.elements[k])
      k = cy;
    j = cycles.elements[k + 1] ? cycles.elements[k + 1] : cycles.elements[cy];
    k = cycles.elements[k];
    /* breaking edge from k -> j */
    // printf("break %d -> %d\n", k, j);
    e = ds->edata.elements + vedge->elements[k];
    for (; *e; e++)
      if (*e == j)
        break;
    if (!*e)
    {
      failed = 1;
      break;
    }
    *e = -1;
    todo.count = cycstart + 1;
  }

  if (failed)
  {
    queue_free(&cycles);
    queue_free(&todo);
    solv_free(mark);

    return 0;
  }

  if (didsccs && depsortsccs != 2)
    queue_truncate(&cycles, didsccs - 1);

  /* record cycles */
  int n, gn = 0;
  for (i = 0; i < cycles.count; i++)
  {
    for (n = 0; cycles.elements[i]; i++, n++)
      queue_push(&ds->cycles, cycles.elements[i]);

    if (n > 0)
    {
      queue_push(&ds->cycles, n);
      gn++;
    }
  }
  if (gn > 0)
    queue_push(&ds->cycles, gn);

  queue_free(&cycles);
  queue_free(&todo);
  solv_free(mark);

  return 1;
}

int
deps_sort_get_pkg_num(Deps_Sort *ds)
{
  return ds->out.count;
}

const char *
deps_sort_get_pkg(Deps_Sort *ds)
{
  Id id = queue_shift(&ds->out);
  if (id == 0)
    return 0;

  return ds->names[id];
}

int
deps_sort_get_cycles_total_group_num(Deps_Sort *ds)
{
  return queue_pop(&ds->cycles);
}

int
deps_sort_get_cycles_next_group_num(Deps_Sort *ds)
{
  return queue_pop(&ds->cycles);
}

const char *
deps_sort_get_cycles_next_group_member(Deps_Sort *ds)
{
  Id id = queue_pop(&ds->cycles);
  if (id == 0)
    return 0;

  return ds->names[id];
}
