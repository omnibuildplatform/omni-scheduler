package scheduler

import "github.com/opensourceways/scheduler/models"

type projectLastCheck map[string]string // key: package name

type GCtx struct {
	arch          string                      // x86_64 etc
	projPacks     map[string]*models.Project  // key: project name
	lastCheck     map[string]projectLastCheck // key: prp
	prpSearchPath map[string][]string         // key: prp
}
