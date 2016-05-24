// Copyright 2016 pixie.grasper

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <vector>

void eval(const std::vector<uint8_t>&) {
  return;
}

int main(int argc, char** argv) {
  // check the counts
  if (argc != 2) {
    printf("usage: %s source.lisp\n", argv[0]);
    return 0;
  }

  // open the file
  auto file_name = argv[1];
  auto fd = open(file_name, O_RDONLY);
  if (fd == -1) {
    auto err = errno;
    fprintf(stderr, "error: cannot open '%s'.\n", file_name);
    fprintf(stderr, "info: %s\n", strerror(err));
    return 1;
  }

  // get size of the file
  struct stat s;
  if (fstat(fd, &s) == -1) {
    auto err = errno;
    fprintf(stderr, "error: fstat failed.\n");
    fprintf(stderr, "info: %s\n", strerror(err));
    close(fd);
    return 1;
  }

  // isn't file?
  if ((s.st_mode & S_IFREG) == 0) {
    fprintf(stderr, "error: '%s' isn't a file.\n", file_name);
    close(fd);
    return 1;
  }

  // read the file
  std::vector<uint8_t> file(static_cast<std::size_t>(s.st_size));
  read(fd, &file[0], file.size());
  close(fd);

  // do something
  eval(file);

  return 0;
}
