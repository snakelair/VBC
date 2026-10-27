fn main() {
    let mut total_len = 0;
    for _ in 1..=50000 {
        let mut s = String::from("base");
        for _ in 1..=10 {
            s.push_str("_chunk");
        }
        total_len += s.len();
    }
    println!("{}", total_len);
}
