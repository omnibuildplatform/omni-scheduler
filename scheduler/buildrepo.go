package scheduler

import (
	"fmt"
	"os"
	"path/filepath"
	"strings"

	"github.com/opensourceways/omni-scheduler/solv"
)

type buildRepo struct {
}

func isFileAvailable(f string) bool {
	_, err := os.Stat(f)
	return err == nil
}

func addRepoScan(gctx *GCtx, prp, arch string, pool *solv.Pool) error {
	dir := filepath.Join(gctx.repoRoot, prp, arch, ":full")
	solvFile := dir + ".solv"

	if isFileAvailable(solvFile) {
		return pool.RepoFromSolvFile(prp, solvFile)
	}

	files, err := os.ReadDir(dir)
	if err != nil {
		save, err := pool.RepoFromBins(prp, dir, nil)
		if err != nil {
			return err
		}

		return save(solvFile)
	}

	bins := make([]string, 0, 2*len(files))

	for _, file := range files {
		if name := file.Name(); strings.HasSuffix(name, ".rpm") {
			info, err := file.Info()
			if err != nil {
				continue
			}

			bins = append(
				bins, name,
				fmt.Sprintf("%d/%d/%s", info.ModTime().Unix(), info.Size(), name),
			)
		}
	}

	save, err := pool.RepoFromBins(prp, dir, bins)
	if err != nil {
		return err
	}

	return save(solvFile)
}
