/*
 * project_info.h
 *
 */

#ifndef OMNI_SCHEDULER_PROJECT_INFO_H
#define OMNI_SCHEDULER_PROJECT_INFO_H

#include "pool.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct s_Project_Info {
  Pool *pool;

  Queue dep2src;
  Queue dep2pkg;
  Queue subpacks;
  Queue localdeps;
  Queue nonlocal_depsrcs;

} Project_Info;


extern Project_Info * new_Project_Info(Pool *pool);
extern void project_info_free(Project_Info *pi);

extern void project_info_parse(Project_Info *pi, char *prp);

extern int project_info_get_dep2src_num(Project_Info *pi);
extern const char * project_info_get_dep2src(Project_Info *pi);

extern const char * project_info_get_dep2pkg_dep(Project_Info *pi);
extern int project_info_get_dep2pkg_pkg(Project_Info *pi);
extern int project_info_get_dep2pkg_num(Project_Info *pi);

extern int project_info_get_localdeps_num(Project_Info *pi);
extern const char * project_info_get_localdeps_dep(Project_Info *pi);

extern int project_info_get_subpacks_next_group_num(Project_Info *pi);
extern const char * project_info_get_subpacks_next_group_member(Project_Info *pi);

extern int project_info_get_nonlocal_depsrcs_next_group_num(Project_Info *pi);
extern const char * project_info_get_nonlocal_depsrcs_repo_name(Project_Info *pi);
extern const char * project_info_get_nonlocal_depsrcs_src(Project_Info *pi);

#ifdef __cplusplus
}
#endif

#endif /* OMNI_SCHEDULER_PROJECT_INFO_H */
