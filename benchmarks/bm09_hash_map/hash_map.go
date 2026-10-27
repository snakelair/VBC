package main

import "fmt"

func main() {
	m := make(map[int]int, 500000)
	for i := 0; i < 500000; i++ {
		m[i] = i * 3
	}
	var sum int64 = 0
	for i := 0; i < 500000; i++ {
		sum += int64(m[i])
	}
	fmt.Println(sum)
}
