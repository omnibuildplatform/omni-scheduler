package models

type EventScanRepo struct {
	Project    string `json:"project" required:"true"`
	Repository string `json:"repository" required:"true"`
}
