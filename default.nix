{ lib, stdenv, cmake, abseil-cpp,
  fetchFromGitHub }:

stdenv.mkDerivation rec {
  pname = "riegeli";
  version = "1.0";
  
  src = ./.;

  nativeBuildInputs = [ cmake abseil-cpp ];

  meta = with lib; {
    homepage = "https://github.com/google/riegeli";
    description = ''
      Riegeli/records is a file format for storing a sequence of
      string records, typically serialized protocol buffers. It
      supports dense compression, fast decoding, seeking, detection
      and optional skipping of data corruption, filtering of proto
      message fields for even faster decoding, and parallel encoding.
    '';
    platforms = with platforms; linux ++ darwin;
    licencse = licenses.asl2;
  };
}
