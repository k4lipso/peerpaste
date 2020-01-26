with import <nixpkgs> {};

stdenv.mkDerivation {
  name = "env";
  nativeBuildInputs = [ pkgconfig python3 python37Packages.klein gcc9 ccls cmake lldb gdb ];
  buildInputs = [ protobuf cryptopp boost171 boost-build libsodium doxygen ];
}
