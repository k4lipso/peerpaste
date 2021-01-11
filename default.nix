{ pkgs ? import <nixpkgs> {}, stdenv }:
with pkgs; 

stdenv.mkDerivation {
  name = "peerpaste";
  src = ./.;
  nativeBuildInputs = [ pkgconfig python3 python37Packages.klein cmake gnumake lldb gdb ];
  depsBuildBuild = [ ccls ];
  buildInputs = [ sqlite protobuf3_7 boost174 cryptopp clang-tools boost-build libsodium doxygen catch2 ];
}
