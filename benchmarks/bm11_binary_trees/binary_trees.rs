#[derive(Clone, Copy)]
struct TreeNode {
    left: i32,
    right: i32,
    item: i32,
}

fn build_tree(nodes: &mut [TreeNode], depth: i32, node_count: &mut usize) -> i32 {
    if depth <= 0 {
        return -1;
    }
    let id = *node_count;
    *node_count += 1;
    nodes[id].item = depth;
    let left = build_tree(nodes, depth - 1, node_count);
    let right = build_tree(nodes, depth - 1, node_count);
    nodes[id].left = left;
    nodes[id].right = right;
    id as i32
}

fn check_tree(nodes: &[TreeNode], id: i32) -> i32 {
    if id == -1 {
        return 1;
    }
    nodes[id as usize].item
        + check_tree(nodes, nodes[id as usize].left)
        + check_tree(nodes, nodes[id as usize].right)
}

fn main() {
    let max_nodes = 1048576;
    let mut nodes = vec![TreeNode { left: -1, right: -1, item: 0 }; max_nodes];
    let mut node_count = 0;
    let root = build_tree(&mut nodes, 18, &mut node_count);
    let check = check_tree(&nodes, root);
    println!("{}", check);
}
