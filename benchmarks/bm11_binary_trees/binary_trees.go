package main

import "fmt"

type TreeNode struct {
	left, right, item int
}

var nodeCount = 0

func buildTree(nodes []TreeNode, depth int) int {
	if depth <= 0 {
		return -1
	}
	id := nodeCount
	nodeCount++
	nodes[id].item = depth
	nodes[id].left = buildTree(nodes, depth-1)
	nodes[id].right = buildTree(nodes, depth-1)
	return id
}

func checkTree(nodes []TreeNode, id int) int {
	if id == -1 {
		return 1
	}
	return nodes[id].item + checkTree(nodes, nodes[id].left) + checkTree(nodes, nodes[id].right)
}

func main() {
	maxNodes := 1048576
	nodes := make([]TreeNode, maxNodes)
	nodeCount = 0
	root := buildTree(nodes, 18)
	check := checkTree(nodes, root)
	fmt.Println(check)
}
