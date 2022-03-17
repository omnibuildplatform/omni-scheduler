/*
 * ext_repo.c
 *
 * extend libsolv repo
 *
 */

#include "ext_repo.h"

#define MY_ERROR_CODE 100

int
ext_repo_add_solv(Repo *repo, char *filename) {
  FILE *fp = fopen(filename, "r");
  if (!fp)
    return MY_ERROR_CODE;

  int v = repo_add_solv(repo, fp, 0);

  fclose(fp);

  return v;
}
