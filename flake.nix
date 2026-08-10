{
  description = "BierKistn Radio — Qt6/QML touch-screen UI for a Pi 4B-based smart speaker";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
  };

  outputs = { self, nixpkgs }:
    let
      supportedSystems = [ "x86_64-linux" "aarch64-linux" ];

      forAllSystems = f:
        nixpkgs.lib.genAttrs supportedSystems
          (system: f nixpkgs.legacyPackages.${system});

      qtInputs = kdePackages: with kdePackages; [
        qtbase
        qtdeclarative
        qtwayland
        qtvirtualkeyboard
        qtsvg
        qtimageformats
      ];

      mkDerivation = pkgs:
        pkgs.stdenv.mkDerivation {
          pname = "bierkistn-radio";
          version = "0.1.0";
          src = self;

          nativeBuildInputs = with pkgs; [
            cmake
            ninja
            pkg-config
            kdePackages.wrapQtAppsHook
          ];

          buildInputs = qtInputs pkgs.kdePackages;

          cmakeFlags = [
            "-DCMAKE_BUILD_TYPE=Release"
          ];
        };
    in
    {
      devShells = forAllSystems (pkgs:
        let
          isX86 = pkgs.stdenv.hostPlatform.system == "x86_64-linux";
        in
        {
          default = pkgs.mkShell {
            name = "bierkistn-radio";

            packages = with pkgs; [
              cmake
              ninja
              pkg-config
              gcc
            ] ++ qtInputs pkgs.kdePackages
            ++ (with pkgs.kdePackages; [
              qttools
              qtlanguageserver
            ]) ++ [
              wayland
              wayland-protocols
              wireplumber
            ] ++ nixpkgs.lib.optionals isX86 (with pkgs; [
              llvmPackages_19.clang-tools
              llvmPackages_19.lld
            ]);
          };
        });

      packages = forAllSystems (pkgs: {
        bierkistnRadio = mkDerivation pkgs;
        default = mkDerivation pkgs;
      });
    };
}
