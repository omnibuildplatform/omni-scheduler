package main

/*
#cgo CFLAGS: -I./libsolv
#cgo LDFLAGS: -L./libsolv -lsolv

#include "pool.h"
*/
import "C"

import "fmt"

func main() {
	p := C.pool_create()
	defer C.pool_free(p)

	fmt.Println(p.nsolvables)

	fmt.Println("done")
}
