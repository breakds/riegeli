#include "riegeli/chunk_encoding/hash.h"

#include <cstdio>

int main(int argc, char **argv) {
  printf("hashed = %lu\n", riegeli::internal::Hash("This is some sample text."));
  return 0;
}
