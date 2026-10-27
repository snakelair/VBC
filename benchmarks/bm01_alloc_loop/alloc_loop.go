package main

import (
	"fmt"
	"strconv"
)

func formatToken(i int) string {
	return "id_" + strconv.Itoa(i)
}

func main() {
	totalLen := 0
	for i := 1; i <= 5000000; i++ {
		tok := formatToken(i)
		totalLen += len(tok)
	}
	fmt.Println(totalLen)
}
