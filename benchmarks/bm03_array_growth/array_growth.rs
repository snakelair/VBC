fn main() {
    let mut arr = Vec::new();
    for i in 0..2000000 {
        arr.push(i);
    }
    let mut sum: i64 = 0;
    for v in arr {
        sum += v as i64;
    }
    println!("{}", sum);
}
