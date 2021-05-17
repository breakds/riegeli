{
  description = "A file format for storing a sequence of string records, typically serialized protocol buffers";

  inputs = {
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils, ... }: flake-utils.lib.eachSystem [
    "x86_64-linux" "i686-linux" "aarch64-linux"
  ] (system:
    let pkgs = import nixpkgs {
          inherit system;
        };
    in {
      devShell = let
        highwayhash = pkgs.callPackage ./nix/pkgs/highwayhash {};
      in pkgs.mkShell rec {
        name = "riegeli";
        buildInputs = with pkgs; [
          # C/C++ Build Tools
          llvmPackages_11.clang
          cmake
          cmakeCurses
          pkgconfig

          abseil-cpp
          protobuf
          highwayhash
        ];

        shellHook = ''
          export CC=clang
          export CXX=clang++
          export PS1="$(echo -e '\uf1c0') {\[$(tput sgr0)\]\[\033[38;5;228m\]\w\[$(tput sgr0)\]\[\033[38;5;15m\]} (${name}) \\$ \[$(tput sgr0)\]"
          export HIGHWAYHASH_DIR = "${highwayhash}";
         '';
      };

      defaultPackage = pkgs.callPackage ./default.nix {};
    });
}
