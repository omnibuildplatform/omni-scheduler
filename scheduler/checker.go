package scheduler

import (
	"fmt"
	"sort"
	"strings"

	"github.com/opensourceways/omni-scheduler/models"
	"github.com/opensourceways/omni-scheduler/solv"
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

func genprp(project, repo string) string {
	return fmt.Sprintf("%s%s%s", project, prpSeparator, repo)
}

type Checker struct {
	gctx       *GCtx
	prp        string // format: project/repository
	arch       string
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

func NewChecker(gctx *GCtx, prp, arch string) *Checker {
	v := strings.Split(prp, prpSeparator)

	return &Checker{
		gctx:       gctx,
		prp:        prp,
		arch:       arch,
		project:    v[0],
		repository: v[1],
		gdst:       fmt.Sprintf("%s/%s/%s", gctx.repoRoot, prp, arch),
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

func (c *Checker) PreparePool() (pool *solv.Pool, status string, err error) {
	pool = solv.NewPool()

	defer func() {
		if err != nil {
			pool.Free()
			pool = nil
		}
	}()

	for _, prp := range c.prpSearchPath {
		// check prp access

		if err = c.addRepo(pool, prp); err != nil {
			return
		}

	}

	status = schedStatusScheduling

	return
}

func (c *Checker) addRepo(pool *solv.Pool, prp string) error {
	return addRepoScan(c.gctx, prp, c.arch, pool)
}
