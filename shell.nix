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
}
