fn main() {
    let raw = "GET /api/v1/resource?id=123 HTTP/1.1\r\nHost: example.com\r\nAccept: application/json\r\n\r\n";
    let mut total_len = 0;

    for _ in 1..=200000 {
        let sp1 = raw.find(' ').unwrap();
        let method = &raw[..sp1];

        let rest = &raw[sp1 + 1..];
        let sp2 = rest.find(' ').unwrap();
        let path = &rest[..sp2];

        total_len += method.len() + path.len();
    }

    println!("{}", total_len);
}
