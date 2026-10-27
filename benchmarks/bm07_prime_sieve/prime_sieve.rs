fn main() {
    let limit = 10000000;
    let mut is_prime = vec![1u8; limit + 1];
    is_prime[0] = 0;
    is_prime[1] = 0;

    let mut p = 2;
    while p * p <= limit {
        if is_prime[p] == 1 {
            let mut k = p * p;
            while k <= limit {
                is_prime[k] = 0;
                k += p;
            }
        }
        p += 1;
    }

    let count = is_prime[2..=limit].iter().filter(|&&x| x == 1).count();
    println!("{}", count);
}
