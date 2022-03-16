package scheduler

import "github.com/opensourceways/scheduler/models"

type projectLastCheck map[string]string // key: package name

type GCtx struct {
	repoRoot string
	arch     string // x86_64 etc

	projPacks     map[string]*models.Project  // key: project name
	lastCheck     map[string]projectLastCheck // key: prp
	prpSearchPath map[string][]string         // key: prp
}

func NewGCtx(root, arch string) *GCtx {
	return &GCtx{
		repoRoot: root,
		arch:     arch,
	}
}
