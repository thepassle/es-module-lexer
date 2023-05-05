fn main() {
  println!("cargo:rerun-if-changed=src/lexer.h");
  println!("cargo:rerun-if-changed=src/lexer.c");
  cc::Build::new()
    .warnings(false)
    .flag_if_supported("-std=c99")
    .file("src/lexer.c")
    .compile("lexer.a");
}
