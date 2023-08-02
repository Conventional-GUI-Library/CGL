{
  description = "A Conventional GUI Library";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs?ref=nixos-23.05";
    flake-utils.url = "github:numtide/flake-utils";

    mini-compile-commands = {
      url = "github:danielbarter/mini_compile_commands";
      flake = false;
    };
  };

  outputs = {
    nixpkgs,
    flake-utils,
    mini-compile-commands,
    ...
  }: let
    supportedSystems = let
      inherit (flake-utils.lib) system;
    in [
      system.aarch64-linux
      system.x86_64-linux
      # macOS possible but untested
    ];
  in
    flake-utils.lib.eachSystem supportedSystems (system: let
      pkgs = import nixpkgs {inherit system;};
      stdenv = (pkgs.callPackage mini-compile-commands {}).wrap pkgs.clangStdenv;
    in {
      devShell =
        pkgs.mkShell.override
        {
          inherit stdenv;
        }
        {
          packages = with pkgs; [
            # build tools
            pkg-config
            gnumake
            autoconf
            libtool
            automake
            gnome2.gtk-doc

            # dependencies
            glib
            gdk-pixbuf
            pango
            gobject-introspection
            gettext
            xorg.libX11
            xorg.libXi
            xorg.libXfixes
            xorg.libXrandr
            xorg.libXdamage
            xorg.libXext
            xorg.libXcomposite
            cairo
            atk
            wayland
            libxkbcommon
          ];
        };
    });
}
