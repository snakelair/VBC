use std::collections::HashMap;

fn main() {
    let mut map = HashMap::with_capacity(500000);
    for i in 0..500000 {
        map.insert(i, i * 3);
    }
    let mut sum: i64 = 0;
    for i in 0..500000 {
        if let Some(&v) = map.get(&i) {
            sum += v as i64;
        }
    }
    println!("{}", sum);
}
