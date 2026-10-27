package main

import "fmt"

func main() {
	totalLen := 0
	for i := 1; i <= 50000; i++ {
		s := "base"
		for j := 1; j <= 10; j++ {
			s = s + "_chunk"
		}
		totalLen += len(s)
	}
	fmt.Println(totalLen)
}
