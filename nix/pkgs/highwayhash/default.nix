{ lib, stdenv, fetchFromGitHub }:

stdenv.mkDerivation rec {
  pname = "highwayhash";
  version = "1.0";

  src = fetchFromGitHub {
    owner = "google";
    repo = "highwayhash";
    rev = "ea06651bd11387f5aac694c819314536fe7df060";
    sha256 = "sha256-SD1k+/Ze4ocTD2SH+4DZpWTVBFB08XjB3SCFaVtDWgc=";
  };

  nativeBuildInputs = [];
  installFlags = [ "DESTDIR=$(out)" "PREFIX=''" ];

  meta = with lib; {
    homepage = "https://github.com/google/highwayhash";
    description = ''
      Strong (well-distributed and unpredictable) hashes:
      Portable implementation of SipHash
      HighwayHash, a 5x faster SIMD hash with security claims
    '';
    platforms = with platforms; linux ++ darwin;
    licencse = licenses.asl2;
  };
}
