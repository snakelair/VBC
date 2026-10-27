fn recurse(depth: i32) -> usize {
    if depth <= 0 {
        return 0;
    }
    let s = format!("step_{}", depth);
    s.len() + recurse(depth - 1)
}

fn main() {
    let total = recurse(30000);
    println!("{}", total);
}
