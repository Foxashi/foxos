unsafe extern "C" {
    fn c_add(a: i32, b: i32) -> i32;
    fn hello_from_c();
}

fn main() {
    unsafe {
        hello_from_c();
        let s = c_add(2, 3);
        println!("c_add(2,3) = {}", s);
    }
}