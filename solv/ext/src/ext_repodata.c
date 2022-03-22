/*
 * ext_repodata.c
 *
 * extend libsolv repodata
 *
 */

#include "ext_repodata.h"
#include "repo_write.h"

#define MY_ERROR_CODE 100

static int
my_repo_write_filter(Repo *repo, Repokey *key, void *kfdata)
{
  int i = 0;

  switch (key->name)
  {
    case SOLVABLE_URL:
    case SOLVABLE_HEADEREND:
    case SOLVABLE_PACKAGER:
    case SOLVABLE_GROUP:
    case SOLVABLE_LICENSE:
      i = KEY_STORAGE_DROPPED;
      break;

    case SOLVABLE_PKGID:
    case SOLVABLE_CHECKSUM:
      i = KEY_STORAGE_INCORE;
      break;

    default:
      i = repo_write_stdkeyfilter(repo, key, kfdata);
      if (i == KEY_STORAGE_VERTICAL_OFFSET)
        i = KEY_STORAGE_DROPPED;
  }

  return i;
}

int
ext_repodata_write_filtered(Repodata *data, char *filename)
{
  FILE *fp = fopen(filename, "w");
  if (!fp)
    return MY_ERROR_CODE;

  int v = repodata_write_filtered(data, fp, my_repo_write_filter, 0, 0);

  fclose(fp);

  return v;
}
