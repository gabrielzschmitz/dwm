{
  pkgs ? import <nixpkgs> { },
}:

pkgs.mkShell {
  name = "dwm-dev";

  nativeBuildInputs = with pkgs; [
    pkg-config
    makeWrapper
    gnumake
    gcc
    bear
    clang-tools
  ];

  buildInputs = with pkgs; [
    fontconfig
    freetype
    imlib2
    libX11
    libXinerama
    libXrender
    libXft
    coreutils
    gawk
    gnugrep
    gnused
    procps
    xsetroot
  ];
}
