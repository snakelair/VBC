fn main() {
    let n = 400;
    let size = n * n;
    let mut a = vec![1.0f64; size];
    let mut b = vec![2.0f64; size];
    let mut c = vec![0.0f64; size];

    for i in 0..n {
        for k in 0..n {
            let aik = a[i * n + k];
            for j in 0..n {
                c[i * n + j] += aik * b[k * n + j];
            }
        }
    }

    let total: f64 = c.iter().sum();
    println!("{}", total);
}
