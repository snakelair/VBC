package main

import (
	"fmt"
	"strings"
)

func main() {
	raw := "GET /api/v1/resource?id=123 HTTP/1.1\r\nHost: example.com\r\nAccept: application/json\r\n\r\n"
	totalLen := 0

	for i := 1; i <= 200000; i++ {
		sp1 := strings.Index(raw, " ")
		method := raw[:sp1]

		rest := raw[sp1+1:]
		sp2 := strings.Index(rest, " ")
		path := rest[:sp2]

		totalLen += len(method) + len(path)
	}

	fmt.Println(totalLen)
}
