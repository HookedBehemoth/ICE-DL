{ pkgs ? import <nixpkgs> {} }:

pkgs.mkShell {
  name = "rust-ffmpeg-python-env";

  buildInputs = [
    pkgs.rustc
    pkgs.cargo
    pkgs.ffmpeg
    pkgs.python3
    pkgs.python3Packages.requests
  ];

  shellHook = ''
    echo "Welcome to your Rust + FFmpeg + Python environment!"
    echo "Rust version: $(rustc --version)"
    echo "Python version: $(python3 --version)"
    echo "FFmpeg version: $(ffmpeg -version | head -n1)"
  '';
}

