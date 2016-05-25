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
    lparent, rparent,
    quote, quasiquote, comma, comma_at,
    dot, dots,
    Max
  };

 public:
  explicit File(std::vector<uint8_t>&& source_)
      : source(std::move(source_)),
        index(0),
        forward_map{},
        backword_map{} {
    init_maps();
    return;
  }

 private:
  void init_maps() {
    regist_as({}, SpecialTokenID::nil);
    regist_as({'#', 't'},      SpecialTokenID::t);
    regist_as({'#', 'f'},      SpecialTokenID::f);
    regist_as({'('},           SpecialTokenID::lparent);
    regist_as({')'},           SpecialTokenID::rparent);
    regist_as({'\''},          SpecialTokenID::quote);
    regist_as({'`'},           SpecialTokenID::quasiquote);
    regist_as({','},           SpecialTokenID::comma);
    regist_as({',', '@'},      SpecialTokenID::comma_at);
    regist_as({'.'},           SpecialTokenID::dot);
    regist_as({'.', '.', '.'}, SpecialTokenID::dots);
    return;
  }

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

  TokenID get_next_token() {
    auto c0 = get_next_unicode();
    while (c0 == ' ' || c0 == '\t' || c0 == '\r' || c0 == '\n') {
      c0 = get_next_unicode();
    }
    switch (c0) {
      case 0:
        return static_cast<TokenID>(SpecialTokenID::nil);
      case '(':
        return static_cast<TokenID>(SpecialTokenID::lparent);
      case ')':
        return static_cast<TokenID>(SpecialTokenID::rparent);
      case '\'':
        return static_cast<TokenID>(SpecialTokenID::quote);
      case '`':
        return static_cast<TokenID>(SpecialTokenID::quasiquote);
      case ',': {
        auto old_index = index;
        auto c1 = get_next_unicode();
        if (c1 == '@') {
          return static_cast<TokenID>(SpecialTokenID::comma_at);
        } else {
          index = old_index;
          return static_cast<TokenID>(SpecialTokenID::comma);
        }
      }
      case '"': {
        std::vector<Unicode> token{};
        for (;;) {
          auto ck = get_next_unicode();
          if (ck == '"') {
            return regist(std::move(token));
          } else if (ck == 0) {
            return static_cast<TokenID>(SpecialTokenID::nil);
          } else if (ck == '\\') {
            ck = get_next_unicode();
            switch (ck) {
              case 't':
                ck = '\t';
                break;
              case 'n':
                ck = '\n';
                break;
              default:
                break;
            }
          }
          token.push_back(ck);
        }
      }
      case ';':
        while (!(c0 == '\r' || c0 == '\n' || c0 == 0)) {
          c0 = get_next_unicode();
        }
        if (c0 == 0) {
          return static_cast<TokenID>(SpecialTokenID::nil);
        }
        return get_next_token();
      case '.':
      case '+':
      case '-': {
        auto old_index = index;
        auto c1 = get_next_unicode();
        if ('0' <= c1 && c1 <= '9') {
          index = old_index;
          break;
        } else if (c0 == '.') {
          if (c1 == '.') {
            if (get_next_unicode() == '.') {
              return static_cast<TokenID>(SpecialTokenID::dots);
            } else {
              return 0;
            }
          } else {
            index = old_index;
            return static_cast<TokenID>(SpecialTokenID::dot);
          }
        } else {
          index = old_index;
          std::vector<Unicode> token{c0};
          return regist(std::move(token));
        }
      }
      case '#': {
        auto c1 = get_next_unicode();
        if (c1 == 't') {
          return static_cast<TokenID>(SpecialTokenID::t);
        } else if (c1 == 'f') {
          return static_cast<TokenID>(SpecialTokenID::f);
        } else if (c1 == '\\') {
          auto c2 = get_next_unicode();
          std::vector<Unicode> token{c0, c1, c2};
          return regist(std::move(token));
        } else {
          return static_cast<TokenID>(SpecialTokenID::nil);
        }
      }
      default:
        break;
    }
    std::vector<Unicode> token{c0};
    if (c0 == '.' || c0 == '+' || c0 == '-' || ('0' <= c0 && c0 <= '9')) {
      bool dotted = c0 == '.';
      for (;;) {
        auto old_index = index;
        auto ck = get_next_unicode();
        if ((dotted && ck == '.') || !('0' <= ck && ck <= '9')) {
          index = old_index;
          break;
        } else if (ck == '.') {
          dotted = true;
        }
        token.push_back(ck);
      }
    } else {
      for (;;) {
        auto old_index = index;
        auto ck = get_next_unicode();
        if (ck == '!' || ck == '$' || ck == '%' || ck == '&' || ck == '*' ||
            ck == '+' || ck == '-' || ck == '.' || ck == '/' || ck == ':' ||
            ck == '<' || ck == '=' || ck == '>' || ck == '?' || ck == '@' ||
            ck == '^' || ck == '_' || ck == '~' ||
            ('a' <= ck && ck <= 'z') ||
            ('A' <= ck && ck <= 'Z') ||
            ('0' <= ck && ck <= '9')) {
          token.push_back(ck);
        } else {
          index = old_index;
          break;
        }
      }
    }
    return regist(std::move(token));
  }

  TokenID regist(std::vector<Unicode>&& token) {
    auto it = forward_map.find(token);
    if (it == forward_map.end()) {
      auto new_id = static_cast<TokenID>(forward_map.size());
      forward_map[token] = new_id;
      backword_map[new_id] = std::move(token);
      return new_id;
    } else {
      return it->second;
    }
  }

  void regist_as(std::vector<Unicode>&& token, SpecialTokenID sid) {
    auto id = static_cast<TokenID>(sid);
    forward_map[token] = id;
    backword_map[id] = std::move(token);
    return;
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
