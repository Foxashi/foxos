fn main() {
    cc::Build::new()
        .file("src/chelp.c")
        .compile("chelp");
}