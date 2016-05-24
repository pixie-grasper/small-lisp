// Copyright 2016 pixie.grasper

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

char* gets(char* s);
#include <vector>
#include <map>

class File {
 private:
  std::vector<uint8_t> source;
  std::size_t index;

  using Unicode = uint32_t;
  using TokenID = uint64_t;
  std::map<std::vector<Unicode>, TokenID> forward_map;
  std::map<TokenID, std::vector<Unicode>> backword_map;
  enum class SpecialTokenID {
    nil = 0,
    t, f,
    lparents, rparents,
    quote, quasiquote, comma, comma_at,
    double_quote,
    Max
  };

 public:
  explicit File(std::vector<uint8_t>&& source_)
      : source(std::move(source_)),
        index(0),
        forward_map{},
        backword_map{} {
    return;
  }

 private:
  // it decodes from utf-8 stream
  Unicode get_next_unicode() {
    if (index >= source.size()) {
      return 0;
    }
    auto c0 = static_cast<Unicode>(source[index]); index++;
    if (c0 < 0x80) {
      return c0;
    } else if (c0 < 0xc2) {
      return 0;
    } else if (c0 < 0xe0) {
      if (index >= source.size()) {
        return 0;
      }
      auto c1 = static_cast<Unicode>(source[index]); index++;
      if ((c1 & 0xc0) != 0x80) {
        return 0;
      }
      return ((c0 & 0x1f) << 6) |
              (c1 & 0x3f);
    } else if (c0 < 0xf0) {
      if (index + 1 >= source.size()) {
        return 0;
      }
      auto c1 = static_cast<Unicode>(source[index]); index++;
      auto c2 = static_cast<Unicode>(source[index]); index++;
      if (((c1 & 0xc0) != 0x80) ||
          ((c2 & 0xc0) != 0x80)) {
        return 0;
      }
      return ((c0 & 0x0f) << 12) |
             ((c1 & 0x3f) <<  6) |
              (c2 & 0x3f);
    } else if (c0 < 0xf8) {
      if (index + 2 >= source.size()) {
        return 0;
      }
      auto c1 = static_cast<Unicode>(source[index]); index++;
      auto c2 = static_cast<Unicode>(source[index]); index++;
      auto c3 = static_cast<Unicode>(source[index]); index++;
      if (((c1 & 0xc0) != 0x80) ||
          ((c2 & 0xc0) != 0x80) ||
          ((c3 & 0xc0) != 0x80)) {
        return 0;
      }
      return ((c0 & 0x07) << 18) |
             ((c1 & 0x3f) << 12) |
             ((c2 & 0x3f) <<  6) |
              (c3 & 0x3f);
    } else if (c0 < 0xfc) {
      if (index + 3 >= source.size()) {
        return 0;
      }
      auto c1 = static_cast<Unicode>(source[index]); index++;
      auto c2 = static_cast<Unicode>(source[index]); index++;
      auto c3 = static_cast<Unicode>(source[index]); index++;
      auto c4 = static_cast<Unicode>(source[index]); index++;
      if (((c1 & 0xc0) != 0x80) ||
          ((c2 & 0xc0) != 0x80) ||
          ((c3 & 0xc0) != 0x80) ||
          ((c4 & 0xc0) != 0x80)) {
        return 0;
      }
      return ((c0 & 0x03) << 24) |
             ((c1 & 0x3f) << 18) |
             ((c2 & 0x3f) << 12) |
             ((c3 & 0x3f) <<  6) |
              (c4 & 0x3f);
    } else if (c0 < 0xfe) {
      if (index + 4 >= source.size()) {
        return 0;
      }
      auto c1 = static_cast<Unicode>(source[index]); index++;
      auto c2 = static_cast<Unicode>(source[index]); index++;
      auto c3 = static_cast<Unicode>(source[index]); index++;
      auto c4 = static_cast<Unicode>(source[index]); index++;
      auto c5 = static_cast<Unicode>(source[index]); index++;
      if (((c1 & 0xc0) != 0x80) ||
          ((c2 & 0xc0) != 0x80) ||
          ((c3 & 0xc0) != 0x80) ||
          ((c4 & 0xc0) != 0x80) ||
          ((c5 & 0xc0) != 0x80)) {
        return 0;
      }
      return ((c0 & 0x01) << 30) |
             ((c1 & 0x3f) << 24) |
             ((c2 & 0x3f) << 18) |
             ((c3 & 0x3f) << 12) |
             ((c4 & 0x3f) <<  6) |
              (c5 & 0x3f);
    } else {
      return 0;
    }
  }
};

void eval(std::vector<uint8_t>&& stream) {
  File file(std::move(stream));
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
  eval(std::move(file));

  return 0;
}
