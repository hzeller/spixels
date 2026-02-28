# This is a nix-shell for use with the nix package manager.
# If you have nix installed, you may simply run `nix-shell`
# in this repo, and have all dependencies ready in the new shell.

{ pkgs ? import (fetchTarball "https://github.com/NixOS/nixpkgs/archive/nixos-25.11.tar.gz") {} }:
pkgs.mkShell {
  buildInputs = with pkgs;
    [
      git
      python3
      python3Packages.kicad
      # Kicad packages refer to this, but don't
      # bring into dependencies in 25.11
      python3Packages.wxpython
      kicad
      zip
      gerbv
    ];
}
