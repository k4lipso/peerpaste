with import (builtins.fetchGit {
  name = "nixos-unstable-2020-10-23";
  url = "https://github.com/nixos/nixpkgs-channels/";
  ref = "refs/heads/nixos-unstable";
  rev = "84d74ae9c9cbed73274b8e4e00be14688ffc93fe";
}) {};

stdenv.mkDerivation {
  name = "peerpaste";
  src = ./.;

  nativeBuildInputs = [ valgrind pkgconfig python3 python37Packages.klein cmake gnumake42 lldb gdb ];
  depsBuildBuild = [ gcc9 ccls ];
  buildInputs = [ protobuf3_7 boost172 cryptopp clang-tools boost-build libsodium doxygen catch2 ];

  installPhase = ''
    mkdir -p $out/bin
    cp peerpaste $out/bin/
  '';
}