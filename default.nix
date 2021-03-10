{ pkgs ? import <nixpkgs> {}, stdenv, sqlite_modern_cpp }:
with pkgs; 

stdenv.mkDerivation {
  name = "peerpaste";
  src = ./.;
  nativeBuildInputs = [ sqlite_modern_cpp pkgconfig python3 python37Packages.klein cmake gnumake lldb gdb ];
  depsBuildBuild = [ ccls ];
  buildInputs = [ spdlog sqlite protobuf3_7 boost174 cryptopp clang-tools boost-build libsodium doxygen catch2 ];
}
