package solv

/*
#cgo CFLAGS: -I../libsolv/src -I../libsolv/ext
#cgo LDFLAGS: -L../libsolv/src -lsolv
#cgo LDFLAGS: -L../libsolv/ext -lsolvext

#include <stdlib.h>
#include "pool.h"
#include "repo.h"
#include "repo_solv.h"
#include "repo_write.h"
#include "repo_rpmdb.h"

int
myrepowritefilter(Repo *repo, Repokey *key, void *kfdata)
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
my_repodata_write_filtered(Repodata *data, FILE *fp)
{
    return repodata_write_filtered(data, fp, myrepowritefilter, 0, 0);
}

*/
import "C"

import (
	"fmt"
	"path/filepath"
	"strings"
	"unsafe"
)

const repoCOOKIE = "buildservice repo 1.1"

type commonStrTag struct {
	tagBuildserviceID           C.int
	tagBuildserviceRepocookie   C.int
	tagBuildserviceExternal     C.int
	tagBuildserviceDodurl       C.int
	tagBuildserviceDodcookie    C.int
	tagBuildserviceDodresources C.int
	tagBuildserviceAnnotation   C.int
	tagBuildserviceModules      C.int
	tagDirectdepsend            C.int
}

func (t *commonStrTag) init(pool *C.struct_s_Pool) {
	f := func(s string) C.int {
		cs := C.CString(s)
		defer C.free(unsafe.Pointer(cs))

		return C.pool_str2id(pool, cs, C.int(1))
	}

	t.tagBuildserviceID = f("buildservice:id")
	t.tagBuildserviceRepocookie = f("buildservice:repocookie")
	t.tagBuildserviceExternal = f("buildservice:external")
	t.tagBuildserviceDodurl = f("buildservice:dodurl")
	t.tagBuildserviceDodcookie = f("buildservice:dodcookie")
	t.tagBuildserviceDodresources = f("buildservice:dodresources")
	t.tagBuildserviceAnnotation = f("buildservice:annotation")
	t.tagBuildserviceModules = f("buildservice:modules")
	t.tagDirectdepsend = f("-directdepsend--")
}

type Pool struct {
	pool *C.struct_s_Pool
	tag  *commonStrTag
}

func NewPool() *Pool {
	pool := C.pool_create()

	C.pool_setdisttype(pool, C.DISTTYPE_RPM)

	tag := new(commonStrTag)
	tag.init(pool)

	C.pool_freeidhashes(pool)

	return &Pool{
		pool: pool,
		tag:  tag,
	}
}

func (p *Pool) Free() {
	C.pool_free(p.pool)
	p.pool = nil
}

func (p *Pool) GetPool() *C.struct_s_Pool {
	return p.pool
}

func (p *Pool) GetSolvablesNum() int {
	return int(p.pool.nsolvables)
}

func (p *Pool) isPoolUnavailable() bool {
	return p.pool == nil
}

func (p *Pool) RepoFromSolvFile(prp, filename string) error {
	repo := p.NewRepo(prp)
	return repo.addSolv(filename)
}

func (p *Pool) RepoFromBins(prp, dir string, bins []string) (func(string) error, error) {
	if p.isPoolUnavailable() {
		return nil, fmt.Errorf("pool is unavailable")
	}

	repo := p.NewRepo(prp)

	rd := repo.addRepoData(0)

	n := len(bins)

	for i := range bins {
		if i+1 >= n {
			continue
		}

		bin := bins[i]

		if !strings.HasSuffix(bin, ".rpm") {
			continue
		}

		rd.addBin(dir, bin, bins[i+1], p)
	}

	repo.setStr(C.SOLVID_META, p.tag.tagBuildserviceRepocookie, repoCOOKIE)
	repo.internalize()

	return rd.save, nil
}

func (p *Pool) NewRepo(name string) *Repo {
	cs := C.CString(name)
	defer C.free(unsafe.Pointer(cs))

	r := C.repo_create(p.pool, cs)
	return &Repo{repo: r}

}

type Repo struct {
	repo *C.struct_s_Repo
}

func (r *Repo) addRepoData(flag int) *RepoData {
	rd := C.repo_add_repodata(r.repo, C.int(flag))

	return &RepoData{
		data: rd,
	}
}

func (r *Repo) addSolv(filename string) error {
	s := C.CString(filename)
	defer C.free(unsafe.Pointer(s))

	w := C.CString("r")
	defer C.free(unsafe.Pointer(w))

	fp := C.fopen(s, w)
	if fp == nil {
		return fmt.Errorf("can't open file:%s", filename)
	}
	defer C.fclose(fp)

	v := C.repo_add_solv(r.repo, fp, C.int(0))

	if v != 0 {
		return fmt.Errorf("repo_add_solv return:%d for solv file:%s", v, filename)
	}

	return nil
}

func (r *Repo) setStr(solvid, key C.int, str string) {
	cs := C.CString(str)
	defer C.free(unsafe.Pointer(cs))

	C.repo_set_str(r.repo, solvid, key, cs)
}

func (r *Repo) internalize() {
	C.repo_internalize(r.repo)
}

type RepoData struct {
	data *C.struct_s_Repodata
}

func (rd *RepoData) addBin(dir, bin, binid string, pool *Pool) C.int {
	cs := C.CString(filepath.Join(dir, bin))
	defer C.free(unsafe.Pointer(cs))

	// rpm
	flag := C.REPO_REUSE_REPODATA |
		C.REPO_NO_INTERNALIZE |
		C.REPO_NO_LOCATION |
		C.RPM_ADD_WITH_PKGID |
		C.RPM_ADD_NO_FILELIST |
		C.RPM_ADD_NO_RPMLIBREQS

	solvid := C.repo_add_rpm(rd.data.repo, cs, C.int(flag))

	if solvid == C.int(0) {
		return 0
	}

	cs = C.CString(bin)
	defer C.free(unsafe.Pointer(cs))

	C.repodata_set_location(rd.data, solvid, C.int(0), nil, cs)

	cs = C.CString(binid)
	defer C.free(unsafe.Pointer(cs))

	C.repodata_set_str(rd.data, solvid, pool.tag.tagBuildserviceID, cs)

	return solvid
}

func (rd *RepoData) save(filename string) error {
	s := C.CString(filename)
	defer C.free(unsafe.Pointer(s))

	w := C.CString("w")
	defer C.free(unsafe.Pointer(w))

	fp := C.fopen(s, w)
	if fp == nil {
		return fmt.Errorf("can't open file:%s", filename)
	}
	defer C.fclose(fp)

	//C.repodata_write_filtered(rd.data, fp, C.myrepowritefilter, nil, nil)
	C.my_repodata_write_filtered(rd.data, fp)

	return nil
}
