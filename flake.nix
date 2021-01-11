{
  description = "A very basic flake";

  inputs.utils.url = "github:numtide/flake-utils";
  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

  outputs = { self, nixpkgs, utils }:

    utils.lib.eachSystem [ "aarch64-linux" "i686-linux" "x86_64-linux" ]
      (system:
      let 
        pkgs = nixpkgs.legacyPackages.${system}; 
      in
        {
          devShell = import ./default.nix { inherit pkgs; stdenv = pkgs.overrideCC pkgs.stdenv pkgs.gcc9; };

          packages.peerpaste = import ./derivation.nix { pkgs = pkgs; boost = pkgs.boost173; stdenv = pkgs.overrideCC pkgs.stdenv pkgs.gcc9; };
          packages.peerpaste_future = import ./derivation.nix { pkgs = pkgs; boost = pkgs.boost174; stdenv = pkgs.overrideCC pkgs.stdenv pkgs.gcc9; };

          defaultPackage = self.packages.${system}.peerpaste;

          hydraJobs.peerpaste = self.packages.${system}.peerpaste;
          hydraJobs.peerpaste_future = self.packages.${system}.peerpaste_future;
        }
      );
}
