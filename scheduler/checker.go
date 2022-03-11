package scheduler

/*
#cgo CFLAGS: -I../libsolv
#cgo LDFLAGS: -L../libsolv -lsolv

#include "pool.h"
*/
import "C"

import (
	"fmt"
	"sort"
	"strings"

	"github.com/opensourceways/scheduler/models"
)

func Test() {
	p := C.pool_create()
	defer C.pool_free(p)

	fmt.Println(p.nsolvables)

	fmt.Println("done")
}

const (
	prpSeparator = "/"

	schedStatusBroken     = "broken"
	schedStatusDisabled   = "disabled"
	schedStatusScheduling = "scheduling"
)

type Checker struct {
	gctx       *GCtx
	prp        string // format: project/repository
	project    string
	repository string
	gdst       string

	prpSearchPath []string
	repo          *models.Repository
	bconf         *models.ProjectConfig
	prptype       string
	packs         []string // pakcage's name list
	genMetaAlgo   int
}

func NewChecker(reporoot, prp, arch string) *Checker {
	v := strings.Split(prp, prpSeparator)

	return &Checker{
		prp:        prp,
		project:    v[0],
		repository: v[1],
		gdst:       fmt.Sprintf("%s/%s/%s", reporoot, prp, arch),
	}
}

func (c *Checker) Setup() (string, string) {
	if p, ok := c.gctx.prpSearchPath[c.prp]; ok {
		c.prpSearchPath = p
	} else {
		return "", "no prpsearchpath"
	}

	proj := c.gctx.projPacks[c.project]

	repo := proj.GetRepo(c.repository)
	if repo == nil {
		return "", "repo does not exist"
	}

	if repo.Status == models.RepoStatusDisabled {
		return schedStatusDisabled, ""
	}

	c.repo = repo

	// getconfig

	var bconf *models.ProjectConfig

	if bconf == nil {
		return schedStatusBroken, "no config"
	}

	c.bconf = bconf

	c.prptype = bconf.Type

	if v := proj.GetPackageNameList(); len(v) > 0 {
		sort.Strings(v)
		c.packs = v
	}

	c.genMetaAlgo = 0

	return schedStatusScheduling, ""
}
