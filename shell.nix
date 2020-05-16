with import <nixpkgs> {};

stdenv.mkDerivation {
  name = "env";
  nativeBuildInputs = [ pkgconfig python3 python37Packages.klein cmake lldb gdb ];
  depsBuildBuild = [ gcc9 ccls ];
  buildInputs = [ protobuf3_7 boost171 cryptopp boost-build libsodium doxygen ];
}
