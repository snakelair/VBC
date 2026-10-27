fn partition(arr: &mut [i32], low: usize, high: usize) -> usize {
    let pivot = arr[high];
    let mut i = low;
    for j in low..high {
        if arr[j] <= pivot {
            arr.swap(i, j);
            i += 1;
        }
    }
    arr.swap(i, high);
    i
}

fn quicksort(arr: &mut [i32], low: usize, high: usize) {
    if low < high {
        let pi = partition(arr, low, high);
        if pi > 0 {
            quicksort(arr, low, pi - 1);
        }
        quicksort(arr, pi + 1, high);
    }
}

fn main() {
    let n = 1000000;
    let mut arr = vec![0i32; n];
    let mut seed: i64 = 42;
    for i in 0..n {
        seed = (seed * 1103515245 + 12345) % 2147483647;
        arr[i] = seed as i32;
    }
    quicksort(&mut arr, 0, n - 1);
    println!("{}", arr[0]);
    println!("{}", arr[n - 1]);
}
