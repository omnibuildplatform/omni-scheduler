/*
 * expander_ctx.h
 *
 */

#ifndef OMNI_SCHEDULER_EXPANDER_CTX_H
#define OMNI_SCHEDULER_EXPANDER_CTX_H

#include "expander.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _ExpanderCtx {
  Pool *pool;
  Expander *xp;
  Queue *out;			/* the result */
  Map installed;		/* installed packages */
  Map conflicts;		/* conflicts from installed packages */
  Queue conflictsinfo;		/* source info for the above */
  int cidone;			/* conflictsinfo done position */
  Queue todo;			/* requires todo list */
  Queue errors;			/* expansion errors */
  Queue cplxq;			/* complex dep work queue */
  Queue cplxblks;		/* complex dep block data, add only */
  Queue todo_cond;		/* delayed requires/conflicts */
  Queue pruneq;			/* multi purpose queue for pruning packages */
  Map todo_condmap;		/* all neg packages in todo_cond blocks */
  Map recommended;		/* recommended packages */
  int recdone;			/* recommended done position */

  /* options */
  int ignoreconflicts;
  int ignoreignore;
  int userecommendsforchoices;
  int usesupplementsforchoices;
  int dorecommends;
  int dosupplements;

  /* hacks */
  Solvable *ignore_s;		/* small hack: ignore requires of this solvable */
} ExpanderCtx;

void expander_installed_complexdep(ExpanderCtx *xpctx, Id p, Id dep, int deptype);

void expander_installed_multiple(ExpanderCtx *xpctx, Queue *toinstall);

int findconflictsinfo(ExpanderCtx *xpctx, Id p, int recorderrors);

void add_recommended_packages(ExpanderCtx *xpctx, Solvable *s);

int expander_checkconflicts(ExpanderCtx *xpctx, Id p, Id *conflicts, int isobsoletes, int recorderrors);

int expander_dep_fulfilled(ExpanderCtx *xpctx, Id dep);

void add_noproviderinfo(ExpanderCtx *xpctx, Id dep, Id who);

int prune_neg_prefers(ExpanderCtx *xpctx, Id who, Id *e, int n);

int prune_pos_prefers(ExpanderCtx *xpctx, Id who, Id *e, int n, int domulti);

int prune_or_dep(ExpanderCtx *xpctx, Id dep, Id *e, int n);

int prune_supplemented(ExpanderCtx *xpctx, Id *e, int n);

void expander_updaterecommendedmap(ExpanderCtx *xpctx);


#ifdef __cplusplus
}
#endif

#endif /* OMNI_SCHEDULER_EXPANDER_CTX_H */

