{ ... }:

let
  pkgs = (import (builtins.fetchGit {
    name = "nixos-unstable-2020-10-23";
    url = "https://github.com/nixos/nixpkgs/";
    ref = "refs/heads/nixos-unstable";
    rev = "24eb3f87fc610f18de7076aee7c5a84ac5591e3e";
  }) {});

  compilers = with pkgs; {
    gcc9 = stdenv;
    gcc10 = overrideCC stdenv gcc10;
  };

  boostLibs = {
    inherit (pkgs) boost173 boost174;
  };

  originalDerivation = [ (pkgs.callPackage (import ./derivation.nix) {}) ];

  f = libname: libs: derivs: with pkgs.lib;
    concatMap (deriv:
      mapAttrsToList (libVers: lib:
        (deriv.override { "${libname}" = lib; }).overrideAttrs
          (old: { name = "${old.name}-${libVers}"; })
      ) libs
    ) derivs;

  overrides = [
    (f "stdenv" compilers)
    #(f "poco"   pocoLibs)
    (f "boost"  boostLibs)
  ];
in
  pkgs.lib.foldl (a: b: a // { "${b.name}" = b; }) {} (
    pkgs.lib.foldl (a: f: f a) originalDerivation overrides
  )
