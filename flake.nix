{
  description = "Task Tracker - A TODO management CLI tool for managing tasks in code";

  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs/nixos-unstable";
    flake-parts.url = "github:hercules-ci/flake-parts";
  };

  outputs = inputs @ {
    self,
    flake-parts,
    ...
  }: flake-parts.lib.mkFlake {inherit inputs;} {
      imports = [
        # To import an internal flake module: ./other.nix
        # To import an external flake module:
        #   1. Add foo to inputs
        #   2. Add foo as a parameter to the outputs function
        #   3. Add here: foo.flakeModule
      ];
      systems = ["x86_64-linux" "aarch64-linux" "aarch64-darwin" "x86_64-darwin"];
      perSystem = {
        pkgs,
        system,
        ...
      }: {
        # Per-system attributes can be defined here. The self' and inputs'
        # module parameters provide easy access to attributes of the same
        # system.
        _module.args.pkgs = import self.inputs.nixpkgs {
          inherit system;
          config.allowUnfree = true;
          config.cudaSupport.enable = true;
        };

        packages.default = pkgs.stdenv.mkDerivation {
          pname = "tatr";
          version = "0.1.0";
          src = ./.;

          nativeBuildInputs = [ pkgs.clang ];

          buildPhase = ''
            make
          '';

          installPhase = ''
            make install PREFIX=$out
          '';

          meta = with pkgs.lib; {
            description = "Task Tracker - A TODO management CLI tool for managing tasks in code";
            license = licenses.mit;
            platforms = platforms.unix;
          };
        };

        devShells.default = pkgs.mkShell {
          packages = [
            pkgs.clang
            pkgs.valgrind
          ];
        };
      };
      flake = {
        # The usual flake attributes can be defined here, including system-
        # agnostic ones like nixosModule and system-enumerating ones, although
        # those are more easily expressed in perSystem.
        
        overlays.default = final: prev: {
          tatr = self.packages.${final.stdenv.hostPlatform.system}.default;
        };
      };
    };
}
