package main

import "fmt"

func swap(arr []int, i, j int) {
	arr[i], arr[j] = arr[j], arr[i]
}

func partition(arr []int, low, high int) int {
	pivot := arr[high]
	i := low - 1
	for j := low; j < high; j++ {
		if arr[j] <= pivot {
			i++
			swap(arr, i, j)
		}
	}
	swap(arr, i+1, high)
	return i + 1
}

func quicksort(arr []int, low, high int) {
	if low < high {
		pi := partition(arr, low, high)
		quicksort(arr, low, pi-1)
		quicksort(arr, pi+1, high)
	}
}

func main() {
	n := 1000000
	arr := make([]int, n)
	seed := 42
	for i := 0; i < n; i++ {
		seed = int((int64(seed)*1103515245 + 12345) % 2147483647)
		arr[i] = seed
	}
	quicksort(arr, 0, n-1)
	fmt.Println(arr[0])
	fmt.Println(arr[n-1])
}
