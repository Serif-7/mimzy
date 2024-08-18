{
  description = "Mimzy - Zathura rewrite in Zig";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = nixpkgs.legacyPackages.${system};
      in
      {
        packages.default = pkgs.stdenv.mkDerivation {
          name = "mimzy";
          src = ./.;
          nativeBuildInputs = with pkgs; [meson ninja pkg-config];
          buildInputs = with pkgs; [ 
            gtk3
            girara
            glib
            json-glib
            file
            sqlite
            meson
            gettext
            pkgconf
            pkg-config
            cairo
            

            # optional
            libseccomp
            ];
          configurePhase = "meson setup builddir --prefix=$out";
          buildPhase = "meson build";
          installPhase = ''
            ninja -C builddir install
            mkdir -p $out/bin
            cp builddir/zathura $out/bin/
          '';

        };

        devShells.default = pkgs.mkShell {
          buildInputs = with pkgs; [
            zig
            gdb
            gtk3
            girara
            glib
            json-glib
            file
            sqlite
            meson
            gettext
            pkgconf
            pkg-config
            cairo
            ];
        };
      });
}
