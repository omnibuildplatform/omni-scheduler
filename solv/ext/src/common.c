/*
 * common.c
 *
 * common functions
 *
 */

#include "common.h"

const char *ID_DIRECTDEPSEND             = "-directdepsend--";
const char *ID_BUILDSERVICE_ID           = "buildservice:id";
const char *ID_BUILDSERVICE_DODURL       = "buildservice:dodurl";
const char *ID_BUILDSERVICE_MODULES      = "buildservice:modules";
const char *ID_BUILDSERVICE_EXTERNAL     = "buildservice:external";
const char *ID_BUILDSERVICE_DODCOOKIE    = "buildservice:dodcookie";
const char *ID_BUILDSERVICE_REPOCOOKIE   = "buildservice:repocookie";
const char *ID_BUILDSERVICE_ANNOTATION   = "buildservice:annotation";
const char *ID_BUILDSERVICE_DODRESOURCES = "buildservice:dodresources";


void
set_disttype(Pool *pool, int disttype)
{
  pool_setdisttype(pool, disttype);
#ifdef POOL_FLAG_HAVEDISTEPOCH
  /* make newer mandriva work, hopefully there are no ill effects... */
  pool_set_flag(pool, POOL_FLAG_HAVEDISTEPOCH, disttype == DISTTYPE_RPM ? 1 : 0);
#endif
}

int
has_keyname(Repo *repo, Id keyname)
{
  Repodata *data;
  int rdid;
  FOR_REPODATAS(repo, rdid, data)
    if (repodata_has_keyname(data, keyname))
      return 1;
  return 0;
}

int
match_modules_req(Pool *pool, Id id)
{
  const char *dep = pool_id2str(pool, id);
  Id *modules;
  if (strncmp(dep, "platform", 8) == 0 && (dep[8] == 0 || dep[8] == '-'))
    return 1;
  for (modules = pool->appdata; *modules; modules++)
    {
      const char *name, *rname;
      if (*modules == id)
	return 1;
      name = pool_id2str(pool, *modules);
      if ((rname = strrchr(name, '-')) == 0 || rname == name)
	continue;
      if (!strncmp(dep, name, rname - name) && dep[rname - name] == 0)
	return 1;
    }
  return 0;
}

Id
str2id_dup(Pool *pool, const char *str)
{
  char buf[256];
  size_t l = strlen(str);
  if (l < 256) {
    memcpy(buf, str, l + 1);
    return pool_str2id(pool, buf, 1);
  } else {
    return pool_str2id(pool, pool_tmpjoin(pool, str, 0, 0), 1);
  }
}
