{ pkgs, boost, stdenv }:

stdenv.mkDerivation {
  name = "peerpaste";
  src = ./.;

  enableParallelBuilding = false;

  nativeBuildInputs = [ pkgs.valgrind pkgs.pkgconfig pkgs.python3 pkgs.python37Packages.klein pkgs.cmake pkgs.gnumake42 pkgs.lldb pkgs.gdb ];
  depsBuildBuild = [ pkgs.ccls ];
  buildInputs = [ pkgs.protobuf3_7 boost pkgs.cryptopp pkgs.clang-tools pkgs.boost-build pkgs.libsodium pkgs.doxygen pkgs.catch2 ];

  installPhase = ''
    mkdir -p $out/bin
    cp peerpaste $out/bin/
  '';
}
