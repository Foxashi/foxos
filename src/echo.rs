#![no_std]
#![no_main]

use core::slice;

unsafe extern "C" {
    fn terminal_write(data: *const u8, size: usize);
}

#[no_mangle]
pub unsafe extern "C" fn echo(ptr: *const u8, len: usize) {
    unsafe {
        terminal_write(ptr, len);
    }
}

use core::panic::PanicInfo;

#[panic_handler]
fn panic(_info: &PanicInfo) -> ! {
    loop {}
}