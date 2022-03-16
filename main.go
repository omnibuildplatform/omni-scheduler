package main

import (
	"fmt"
	"os"

	"github.com/opensourceways/scheduler/models"
	"github.com/opensourceways/scheduler/scheduler"
)

func main() {
	scheduler.Test()

	testRepoScan()
}

func testRepoScan() {
	args := os.Args
	fmt.Println(args)

	if len(args) != 5 {
		fmt.Println("not enough parameters")
		return
	}

	gctx := scheduler.NewGCtx(args[1], args[2])

	e := models.EventScanRepo{
		Project:    args[3],
		Repository: args[4],
	}

	err := scheduler.HandleEvent(gctx, e)
	fmt.Printf("handle event, err:%v\n", err)
}
