package main

import "fmt"

func main() {
	limit := 10000000
	isPrime := make([]byte, limit+1)
	for i := 2; i <= limit; i++ {
		isPrime[i] = 1
	}

	for p := 2; p <= 3162; p++ {
		if isPrime[p] == 1 {
			for k := p * p; k <= limit; k += p {
				isPrime[k] = 0
			}
		}
	}

	count := 0
	for i := 2; i <= limit; i++ {
		if isPrime[i] == 1 {
			count++
		}
	}
	fmt.Println(count)
}
