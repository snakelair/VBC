package main

import "fmt"

func main() {
	var arr []int
	for i := 0; i < 2000000; i++ {
		arr = append(arr, i)
	}
	var sum int64 = 0
	for _, v := range arr {
		sum += int64(v)
	}
	fmt.Println(sum)
}
