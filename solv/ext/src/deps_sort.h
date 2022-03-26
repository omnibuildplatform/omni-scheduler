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

#ifdef __cplusplus
}
#endif

#endif /* OMNI_SCHEDULER_DEPS_SORT_H */
