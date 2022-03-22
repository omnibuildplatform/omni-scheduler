/*
 * expander.h
 *
 */

#ifndef OMNI_SCHEDULER_EXPANDER_H
#define OMNI_SCHEDULER_EXPANDER_H

#include "pool.h"

#ifdef __cplusplus
extern "C" {
#endif

#define EXPANDER_DEBUG_ALL		(1 << 0)
#define EXPANDER_DEBUG_STDOUT		(1 << 1)
#define EXPANDER_DEBUG_STR		(1 << 2)

#define EXPANDER_OPTION_IGNOREIGNORE			(1 << 0)
#define EXPANDER_OPTION_IGNORECONFLICTS			(1 << 1)
#define EXPANDER_OPTION_DORECOMMENDS			(1 << 2)
#define EXPANDER_OPTION_DOSUPPLEMENTS			(1 << 3)
#define EXPANDER_OPTION_USERECOMMENDSFORCHOICES		(1 << 4)
#define EXPANDER_OPTION_USESUPPLEMENTSFORCHOICES	(1 << 5)

#define MAPEXP(m, n) ((m)->size < (((n) + 8) >> 3) ? map_grow(m, n + 256) : 0)

typedef struct _Expander {
  Pool *pool;

  Map ignored;
  Map ignoredx;

  Queue preferposq;
  Map preferpos;
  Map preferposx;

  Map preferneg;
  Map prefernegx;

  Queue conflictsq;
  Map conflicts;

  int havefileprovides;

  /* debug support */
  int debug;
  char *debugstr;
  int debugstrl;
  int debugstrf;

  /* options */
  int ignoreconflicts;
  int ignoreignore;
  int userecommendsforchoices;
  int usesupplementsforchoices;
  int dorecommends;
  int dosupplements;
} Expander;

extern Expander * new_expander(Pool *pool, int debug, int options);

extern void expander_init_prefer(Expander *xp, char *pkg);
extern void expander_init_ignore(Expander *xp, char *pkg);
extern void expander_init_conflict(Expander *xp, char *pkg);
extern void expander_init_file_provides(Expander *xp, char *file, char *provides);

#ifdef __cplusplus
}
#endif

#endif /* OMNI_SCHEDULER_EXPANDER_H */

