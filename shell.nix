with import <nixpkgs> {};

stdenv.mkDerivation {
  name = "env";
  nativeBuildInputs = [ python3 python37Packages.klein gcc9 ccls cmake lldb gdb ];
  buildInputs = [ protobuf cryptopp boost boost-build libsodium ];
}
