package main

import "fmt"

type Point3D struct {
	x, y, z float64
}

func main() {
	n := 1000000
	pts := make([]Point3D, n)
	for i := 0; i < n; i++ {
		pts[i].x = float64(i) * 1.0
		pts[i].y = float64(i) * 2.0
		pts[i].z = float64(i) * 3.0
	}
	var sum float64 = 0.0
	for i := 0; i < n; i++ {
		sum += pts[i].x + pts[i].y + pts[i].z
	}
	fmt.Println(sum)
}
