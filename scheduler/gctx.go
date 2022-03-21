package scheduler

import (
	"github.com/opensourceways/omni-scheduler/models"
	"k8s.io/apimachinery/pkg/util/sets"
)

type projectLastCheck map[string]string // key: package name

type GCtx struct {
	repoRoot string
	arch     string // x86_64 etc

	projPacks     map[string]*models.Project  // key: project name
	lastCheck     map[string]projectLastCheck // key: prp
	prpSearchPath map[string][]string         // key: prp
	prpNotReady   map[string]sets.String
}

func (g *GCtx) isNotReady(prp, pkg string) bool {
	if g == nil {
		return false
	}

	if v, ok := g.prpNotReady[prp]; ok {
		return v.Has(pkg)
	}

	return false
}

func NewGCtx(root, arch string) *GCtx {
	return &GCtx{
		repoRoot: root,
		arch:     arch,
	}
}
