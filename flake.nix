{
  description = "BierKistn Radio — Qt6/QML touch-screen UI for a Pi 4B-based smart speaker";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
  };

  # Parameters: current flake self and nixpkgs input. Returns package/devShell attributes;
  # evaluation declares builds and does not launch the application or configure NixOS.
  outputs = { self, nixpkgs }:
    let
      supportedSystems = [ "x86_64-linux" "aarch64-linux" ];

      # f: native package-set callback. Returns attributes keyed by supported system.
      forAllSystems = f:
        nixpkgs.lib.genAttrs supportedSystems
          # system: platform name. Returns f's value for that native package set.
          (system: f nixpkgs.legacyPackages.${system});

      # kdePackages: Qt/KDE package set. Returns the app's Qt dependency list.
      qtInputs = kdePackages: with kdePackages; [
        qtbase
        qtdeclarative
        qtwayland
        qtvirtualkeyboard
        qtsvg
        qtimageformats
      ];

      # pkgs: native platform packages. Returns wrapped Release app derivation without tests.
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
            # Tests run in the development build. Deployment builds only need
            # the app, especially when building ARM through emulation.
            "-DBUILD_TESTING=OFF"
          ];
        };
    in
    {
      # Callback pkgs: native packages. Returns default dev shell, with x86-only LLVM extras.
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

      # Callback pkgs: native packages. Returns app/default aliases; ARM uses builder/emulation.
      packages = forAllSystems (pkgs: {
        bierkistnRadio = mkDerivation pkgs;
        default = mkDerivation pkgs;
      });
    };
}
