fn main() {
  println!("cargo:rerun-if-changed=src/lexer.h");
  println!("cargo:rerun-if-changed=src/lexer.c");
  cc::Build::new().warnings(false).file("src/lexer.c").compile("lexer.a");
}
