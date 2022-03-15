package scheduler

import (
	"fmt"
	"sort"
	"strings"

	"github.com/opensourceways/scheduler/models"
	"github.com/opensourceways/scheduler/solv"
)

func Test() {
	p := solv.NewPool()
	defer p.Free()

	fmt.Println(p.GetSolvablesNum())

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

	pool *solv.Pool
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

func (c *Checker) PreparePool() {
	pool := solv.NewPool()

	c.pool = pool

	/*
		for _, path := range c.prpSearchPath {
			// check prp access

		}
	*/
}

func (c *Checker) addRepo(path string) error {
	return nil
}
