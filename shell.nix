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
    xorgserver # provides Xephyr
  ];

  buildInputs = with pkgs; [
    fontconfig
    freetype
    imlib2
    libX11
    libXcursor
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

  shellHook = ''
    echo "dwm dev shell"
    echo "  make            build dwm (run inside src/)"
    echo "  make xephyr     start Xephyr on :1 (1280x720)"
    echo "  make run        run ./dwm inside Xephyr"
    echo "  make dev        build, start Xephyr if needed, run dwm inside it"
    echo "  make keys       regenerate dwm-keys(1) from config.h"
  '';
}
