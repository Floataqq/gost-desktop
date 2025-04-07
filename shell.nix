{ pkgs ? import <nixpkgs> {} }:
  pkgs.mkShell {
  packages = with pkgs; [
    poetry
    fontconfig.lib
    freetype
    xorg.libX11
    glib.out
    util-linuxMinimal
    wayland
    gtk3
    xkeyboard_config
  ];
  NIX_LD_LIBRARY_PATH = "";
  LD_LIBRARY_PATH = ''${pkgs.glib.out}/lib:${pkgs.fontconfig.lib}/lib:${pkgs.freetype}/lib:${pkgs.xorg.libX11}/lib:${pkgs.util-linuxMinimal}/lib:${pkgs.wayland}/lib:${pkgs.gtk3}/lib'';
  QT_XKB_CONFIG_ROOT = ''${pkgs.xkeyboard_config}/share/X11/xkb'';
  shellHook = ''
  '';
}
