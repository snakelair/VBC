package main

import (
	"fmt"
	"strconv"
)

func recurse(depth int) int {
	if depth <= 0 {
		return 0
	}
	s := "step_" + strconv.Itoa(depth)
	return len(s) + recurse(depth-1)
}

func main() {
	total := recurse(30000)
	fmt.Println(total)
}
