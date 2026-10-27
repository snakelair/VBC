package main

import "fmt"

func main() {
	n := 400
	size := n * n
	a := make([]float64, size)
	b := make([]float64, size)
	c := make([]float64, size)

	for i := 0; i < n; i++ {
		for j := 0; j < n; j++ {
			idx := i*n + j
			a[idx] = 1.0
			b[idx] = 2.0
		}
	}

	for i := 0; i < n; i++ {
		for k := 0; k < n; k++ {
			aik := a[i*n+k]
			for j := 0; j < n; j++ {
				c[i*n+j] += aik * b[k*n+j]
			}
		}
	}

	total := 0.0
	for i := 0; i < size; i++ {
		total += c[i]
	}
	fmt.Println(total)
}
