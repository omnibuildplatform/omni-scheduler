/*
 * common.h
 *
 */

#ifndef OMNI_SCHEDULER_COMMON_H
#define OMNI_SCHEDULER_COMMON_H

#include "repo.h"
#include "ext_knownid.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ISNOARCH(arch) (arch == ARCH_NOARCH || arch == ARCH_ALL || arch == ARCH_ANY)

const char *ID_DIRECTDEPSEND;
const char *ID_BUILDSERVICE_DODURL;
const char *ID_BUILDSERVICE_MODULES;
const char *ID_BUILDSERVICE_EXTERNAL;
const char *ID_BUILDSERVICE_DODCOOKIE;
const char *ID_BUILDSERVICE_ANNOTATION;
const char *ID_BUILDSERVICE_DODRESOURCES;

void set_disttype(Pool *pool, int disttype);

int has_keyname(Repo *repo, Id keyname);

int match_modules_req(Pool *pool, Id id);

#ifdef __cplusplus
}
#endif

#endif /* OMNI_SCHEDULER_COMMON_H */

