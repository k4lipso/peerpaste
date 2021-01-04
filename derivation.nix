{ pkgs, boost, stdenv }:

stdenv.mkDerivation {
  name = "peerpaste";
  src = ./.;

  enableParallelBuilding = true;

  nativeBuildInputs = [ pkgs.pkgconfig pkgs.cmake pkgs.gnumake42 ];
  depsBuildBuild = [ ];
  buildInputs = [ pkgs.sqlite pkgs.protobuf3_7 boost pkgs.cryptopp pkgs.clang-tools pkgs.boost-build pkgs.libsodium pkgs.doxygen pkgs.catch2 ];

  installPhase = ''
    mkdir -p $out/bin
    cp peerpaste $out/bin/
  '';
}
