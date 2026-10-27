#[derive(Clone, Copy)]
struct Point3D {
    x: f64,
    y: f64,
    z: f64,
}

fn main() {
    let n = 1000000;
    let mut pts = vec![Point3D { x: 0.0, y: 0.0, z: 0.0 }; n];
    for i in 0..n {
        pts[i].x = i as f64 * 1.0;
        pts[i].y = i as f64 * 2.0;
        pts[i].z = i as f64 * 3.0;
    }
    let mut sum = 0.0;
    for i in 0..n {
        sum += pts[i].x + pts[i].y + pts[i].z;
    }
    println!("{}", sum);
}
