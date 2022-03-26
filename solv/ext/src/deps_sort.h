/*
 * deps_sort.h
 *
 */

#ifndef OMNI_SCHEDULER_DEPS_SORT_H
#define OMNI_SCHEDULER_DEPS_SORT_H

#include "queue.h"
#include "bitmap.h"
#include "hash.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct s_Deps_Sort {
  Queue out;
  Queue cycles;

  Queue edata;
  Queue vedge;
  Map edgeunifymap;

  Hashval hashmask;
  Hashtable ht;
  char **names;
  char **depnames;
  int packs_num;
  int nnames;
} Deps_Sort;


extern Deps_Sort * deps_sort_create();
extern void deps_sort_free(Deps_Sort *ds);

extern void deps_sort_add_pkg_init(Deps_Sort *ds, int packs_num);
extern int deps_sort_add_pkg_src(Deps_Sort *ds, const char *pkg, const char *src);

extern void deps_sort_add_dep_init(Deps_Sort *ds);
extern int deps_sort_pre_add_dep_of_pkg(Deps_Sort *ds);
extern void deps_sort_add_dep_of_pkg(Deps_Sort *ds, int pkgid, const char *depsrc);
extern void deps_sort_post_add_dep_of_pkg(Deps_Sort *ds, int edgestart);
extern int deps_sort_start(Deps_Sort *ds, int depsortsccs);

extern int deps_sort_get_pkg_num(Deps_Sort *ds);
extern const char * deps_sort_get_pkg(Deps_Sort *ds);

extern int deps_sort_get_cycles_total_group_num(Deps_Sort *ds);
extern int deps_sort_get_cycles_next_group_num(Deps_Sort *ds);
extern const char * deps_sort_get_cycles_next_group_member(Deps_Sort *ds);

#ifdef __cplusplus
}
#endif

#endif /* OMNI_SCHEDULER_DEPS_SORT_H */
