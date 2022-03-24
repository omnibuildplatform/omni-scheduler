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
#include "ext_knownid.h"
#include "transitive_deps.h"
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

type Pool struct {
	pool *C.struct_s_Pool
}

func NewPool() *Pool {
	pool := C.ext_pool_create()

	return &Pool{
		pool: pool,
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

	key := C.pool_str2id(p.pool, C.ID_BUILDSERVICE_REPOCOOKIE, 0)
	repo.setStr(C.SOLVID_META, key, repoCOOKIE)
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

func (p *Pool) PrepareHash(prp string, set func(int, string, string, string)) error {
	n := int(p.pool.nsolvables)
	pool := p.pool

	for i := 2; i < n; i++ {
		j := C.int(i)

		if 0 == C.ext_pool_is_considered_packages(pool, j) {
			continue
		}

		set(
			i,
			C.GoString(C.ext_pool_pkg2reponame(pool, j)),
			C.GoString(C.ext_pool_pkg2name(pool, j)),
			C.GoString(C.ext_pool_pkg2srcname(pool, j)),
		)
	}

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

	key := C.pool_str2id(pool.pool, C.ID_BUILDSERVICE_ID, 0)
	C.repodata_set_str(rd.data, solvid, key, cs)

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

type Expander struct {
	xp *C.struct_s_Expander
}

func (e *Expander) expand(deps []string) ([]string, error) {
	td := C.new_Transitive_Deps(e.xp)
	defer C.transitive_deps_free(td)

	pre := func(dep string) {
		s := C.CString(dep)
		defer freeCString(s)

		C.transitive_deps_pre_expand(td, s)
	}

	for _, dep := range deps {
		pre(dep)
	}

	nerrors := C.transitive_deps_expand(td)

	if nerrors == 0 {
		n := int(td.out.count)
		r := make([]string, 0, n)

		for i := 0; i < n; i++ {
			s := C.transitive_deps_get_dep(td, C.int(i))
			r = append(r, C.GoString(s))
		}

		return r, nil
	}

	return nil, e.parseExpandError(td, int(nerrors))
}

func (e *Expander) parseExpandError(td *C.struct_s_Transitive_Deps, nerrors int) error {
	p := C.CBytes(make([]byte, 1024))
	defer C.free(p)

	buf := (*C.char)(p)
	strs := make([]string, 0, nerrors)

	i := C.int(0)
	for {
		if i = C.transitive_deps_get_expand_error(td, i, buf); i == 0 {
			break
		}

		strs = append(strs, string(C.GoBytes(p, C.int(C.strlen(buf)))))
	}

	return fmt.Errorf(strings.Join(strs, ", "))
}
