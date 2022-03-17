/*
 * ext_repodata.h
 *
 */

#ifndef OMNI_SCHEDULER_EXT_REPODATA_H
#define OMNI_SCHEDULER_EXT_REPODATA_H

#include "repodata.h"

#ifdef __cplusplus
extern "C" {
#endif

extern int ext_repodata_write_filtered(Repodata *data, char *filename);

#ifdef __cplusplus
}
#endif

#endif /* OMNI_SCHEDULER_EXT_REPODATA_H */
