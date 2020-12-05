{ pkgs ? import <nixpkgs> {}, stdenv }:
with pkgs;

mkShell {
  nativeBuildInputs = [ pkgconfig python3 python37Packages.klein cmake gnumake42 lldb gdb ];
  depsBuildBuild = [ ccls ];
  buildInputs = [ protobuf3_7 boost172 cryptopp clang-tools boost-build libsodium doxygen catch2 ];
}
