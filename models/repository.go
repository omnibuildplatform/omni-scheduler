package models

const (
	RepoStatusDisabled = "disabled"
)

type Repository struct {
	Name   string
	Status string
}

type Package struct{}

type Project struct {
	Repository []Repository
	Package    map[string]Package // key: package name
}

func (p *Project) GetRepo(name string) *Repository {
	if p == nil {
		return nil
	}

	for i := range p.Repository {
		if p.Repository[i].Name == name {
			return &p.Repository[i]
		}
	}

	return nil
}

func (p *Project) GetPackageNameList() []string {
	if p == nil {
		return nil
	}

	v := make([]string, 0, len(p.Package))

	for k := range p.Package {
		v = append(v, k)
	}

	return v
}

type ProjectConfig struct {
	Type string
}
