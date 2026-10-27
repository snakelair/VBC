fn format_token(i: i32) -> String {
    format!("id_{}", i)
}

fn main() {
    let mut total_len = 0;
    for i in 1..=5000000 {
        let tok = format_token(i);
        total_len += tok.len();
    }
    println!("{}", total_len);
}
