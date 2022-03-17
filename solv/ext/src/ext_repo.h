/*
 * ext_repo.h
 *
 */

#ifndef OMNI_SCHEDULER_EXT_REPO_H
#define OMNI_SCHEDULER_EXT_REPO_H

#include "repo_solv.h"

#ifdef __cplusplus
extern "C" {
#endif

extern int ext_repo_add_solv(Repo *repo, char *filename);

#ifdef __cplusplus
}
#endif

#endif /* OMNI_SCHEDULER_EXT_REPO_H */
