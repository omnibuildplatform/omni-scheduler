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

#define DEPTYPE_REQUIRES		0
#define DEPTYPE_CONFLICTS		1
#define DEPTYPE_OBSOLETES		2
#define DEPTYPE_RECOMMENDS		3

#define ERROR_NOPROVIDER		1
#define ERROR_CHOICE			2
#define ERROR_CONFLICTINGPROVIDERS	3
#define ERROR_PROVIDERINFO		4
#define ERROR_PROVIDERINFO2		5
#define ERROR_BADDEPENDENCY		6
#define ERROR_CONFLICT			7
#define ERROR_CONFLICT2			8
#define ERROR_ALLCONFLICT		9
#define ERROR_NOPROVIDERINFO		10

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

void expander_dbg(Expander *xp, const char *format, ...);

const char * expander_solvid2name(Expander *xp, Id p);
const char * expander_solvid2str(Expander *xp, Id p);

#ifdef __cplusplus
}
#endif

#endif /* OMNI_SCHEDULER_EXPANDER_H */

