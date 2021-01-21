{ pkgs, boost, stdenv, sqlite_modern_cpp }:

stdenv.mkDerivation {
  name = "peerpaste";
  src = ./.;

  enableParallelBuilding = true;

  nativeBuildInputs = [ sqlite_modern_cpp pkgs.pkgconfig pkgs.cmake pkgs.gnumake42 ];
  depsBuildBuild = [ ];
  buildInputs = [ pkgs.sqlite pkgs.protobuf3_7 boost pkgs.cryptopp pkgs.clang-tools pkgs.boost-build pkgs.libsodium pkgs.doxygen pkgs.catch2 ];

  installPhase = ''
    mkdir -p $out/bin
    cp peerpaste $out/bin/
  '';
}
