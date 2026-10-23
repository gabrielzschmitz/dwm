{
  stdenv,
  lib,
  makeWrapper,
  pkg-config,
  fontconfig,
  freetype,
  imlib2,
  libX11,
  libXinerama,
  libXrender,
  libXft,
  coreutils,
  gawk,
  gnugrep,
  gnused,
  procps,
  xsetroot,
}:

stdenv.mkDerivation {
  pname = "dwm";
  version = "6.5";

  src = ./src;

  nativeBuildInputs = [
    pkg-config
    makeWrapper
  ];

  buildInputs = [
    fontconfig
    freetype
    imlib2
    libX11
    libXinerama
    libXrender
    libXft
  ];

  installPhase = ''
    runHook preInstall

    mkdir -p $out/bin
    install -Dm755 dwm $out/bin/dwm
    install -Dm755 ${./bar.sh} $out/bin/dwm-bar

    wrapProgram $out/bin/dwm-bar \
      --prefix PATH : ${
        lib.makeBinPath [
          coreutils
          gawk
          gnugrep
          gnused
          procps
          xsetroot
        ]
      }

    mkdir -p $out/share/man/man1
    sed "s/VERSION/$version/g" < dwm.1 > $out/share/man/man1/dwm.1
    chmod 644 $out/share/man/man1/dwm.1

    mkdir -p $out/share/applications
    install -Dm644 dwm.desktop $out/share/applications/dwm.desktop

    mkdir -p $out/share/icons/hicolor/scalable/apps
    install -Dm644 ${./dwm.svg} $out/share/icons/hicolor/scalable/apps/dwm.svg

    mkdir -p $out/share/icons/hicolor/256x256/apps
    install -Dm644 dwm.png $out/share/icons/hicolor/256x256/apps/dwm.png

    runHook postInstall
  '';

  meta = {
    mainProgram = "dwm";
    description = "Custom build of the suckless dynamic window manager by gabrielzschmitz";
    longDescription = ''
      dwm (dynamic window manager) is a tiling window manager for the X Window
      System developed by the suckless community. This package provides a
      custom build maintained by gabrielzschmitz, with personal patches and
      configuration changes.
    '';
    homepage = "https://github.com/gabrielzschmitz/dwm";
    license = lib.licenses.mit;
    maintainers = with lib.maintainers; [ gabrielzschmitz ];
    platforms = lib.platforms.linux;
  };
}
