/*
 * transitive_deps.h
 *
 */

#ifndef OMNI_SCHEDULER_TRANSITIVE_DEPS_H
#define OMNI_SCHEDULER_TRANSITIVE_DEPS_H

#include "expander.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct s_Transitive_Deps {
  Expander *xp;

  Queue in;
  Queue out;
  Queue indep;
  Queue ignoreq;

  int options;
  int directdepsend;
} Transitive_Deps;

extern Transitive_Deps * new_Transitive_Deps(Expander *xp);
extern void transitive_deps_free(Transitive_Deps *td);

extern void transitive_deps_pre_expand(Transitive_Deps *td, char *dep);
extern int transitive_deps_expand(Transitive_Deps *td);
extern const char * transitive_deps_get_dep(Transitive_Deps *td, int i);
extern int transitive_deps_get_expand_error(Transitive_Deps *td, int i, char *buf);

#ifdef __cplusplus
}
#endif

#endif /* OMNI_SCHEDULER_TRANSITIVE_DEPS_H */
