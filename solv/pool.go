package solv

/*
#cgo CFLAGS: -I../libsolv/src -I../libsolv/ext -I./ext/src
#cgo LDFLAGS: -L../libsolv/src -lsolv
#cgo LDFLAGS: -L../libsolv/ext -lsolvext
#cgo LDFLAGS: -L./ext/lib -lsched

#include <stdlib.h>
#include "repo_rpmdb.h"
#include "ext_pool.h"
#include "ext_repo.h"
#include "ext_repodata.h"
*/
import "C"

import (
	"fmt"
	"path/filepath"
	"strings"
	"unsafe"
)

const repoCOOKIE = "buildservice repo 1.1"

func freeCString(s *C.char) {
	C.free(unsafe.Pointer(s))
}

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
		defer freeCString(cs)

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
	defer freeCString(cs)

	r := C.repo_create(p.pool, cs)
	return &Repo{repo: r}

}

func (p *Pool) CreateWhatProviders() error {
	C.ext_pool_createwhatprovides(p.pool, C.int(0))
	return nil
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
	defer freeCString(s)

	if v := C.ext_repo_add_solv(r.repo, s); v != 0 {
		return fmt.Errorf(
			"ext_repo_add_solv return:%d for solv file:%s", v, filename,
		)
	}

	return nil
}

func (r *Repo) setStr(solvid, key C.int, str string) {
	cs := C.CString(str)
	defer freeCString(cs)

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
	defer freeCString(cs)

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
	defer freeCString(cs)

	C.repodata_set_location(rd.data, solvid, C.int(0), nil, cs)

	cs = C.CString(binid)
	defer freeCString(cs)

	C.repodata_set_str(rd.data, solvid, pool.tag.tagBuildserviceID, cs)

	return solvid
}

func (rd *RepoData) save(filename string) error {
	s := C.CString(filename)
	defer freeCString(s)

	if v := C.ext_repodata_write_filtered(rd.data, s); v != 0 {
		return fmt.Errorf(
			"ext_repodata_write_filtered return:%d for file:%s",
			v, filename,
		)
	}

	return nil
}
