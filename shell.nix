{ pkgs ? import <nixpkgs> {} }:
  pkgs.llvmPackages_13.stdenv.mkDerivation {
    name = "shell";
    nativeBuildInputs = with pkgs; [
      cmake
      ninja
      gdb
    ];
    buildInputs = with pkgs; [
      boost.dev
    ];
    shellHook = ''
      export LD_LIBRARY_PATH=$(nix eval --raw nixpkgs.stdenv.cc.cc.lib)/lib:$LD_LIBRARY_PATH
    '';
}
