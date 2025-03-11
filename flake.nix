{
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    devenv.url = "github:cachix/devenv";
  };

  outputs = { self, nixpkgs, devenv, ... } @ inputs:
    let
      pkgs = nixpkgs.legacyPackages."x86_64-linux";
    in
    {
      devShell.x86_64-linux = pkgs.mkShell {
        buildInputs = with pkgs; [
          just
          clang
          clang-tools
          gdb
          qbe
        ];
      };
    };
}

