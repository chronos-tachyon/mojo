#include <cstdio>

#include "path/path.h"

int main(int argc, char** argv) {
  if (argc != 2 || argv[1][0] == '-') {
    fprintf(stderr,
            "Makes a path absolute.\n"
            "Usage: %s <path>\n",
            argv[0]);
    return 2;
  }

  std::string path(argv[1]);
  auto r = path::make_abs(&path);
  if (!r) {
    fprintf(stderr, "%s\n", r.as_string().c_str());
    return 1;
  }

  printf("%s\n", path.c_str());
  return 0;
}
