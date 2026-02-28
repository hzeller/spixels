# This is a nix-shell for use with the nix package manager.
# If you have nix installed, you may simply run `nix-shell`
# in this repo, and have all dependencies ready in the new shell.

# nixpkgs with current kicad 9.0.7 and a compile fix to gerbv
{ pkgs ? import (fetchTarball "https://github.com/NixOS/nixpkgs/archive/b46e7e3879793c3dd810f9c315d9dc211f550af3.tar.gz") {} }:
pkgs.mkShell {
  buildInputs = with pkgs;
    [
      kicad
      git
      python3
      python3Packages.kicad
      # Kicad packages refer to this, but don't
      # bring into dependencies in 25.11
      python3Packages.wxpython
      zip
      gerbv
    ];
}
