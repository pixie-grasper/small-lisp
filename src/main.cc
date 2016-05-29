// Copyright 2016 pixie.grasper

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

char* gets(char* s);
#include <list>
#include <map>
#include <memory>
#include <vector>

using TokenID = uint64_t;

enum class Type {
  cell, token,
};

enum class SpecialTokenID {
  nil = 0,
  t, f,
  lparent, rparent,
  quote, quasiquote, comma, comma_at,
  dot, dots,
  cons, car, cdr, atom, eq, cond, lambda, define, quote2,
  add, sub, mul, div, mod, le, lt, ge, gt,
  Max
};

class Object {
 public:
  virtual ~Object() {
    return;
  }

  virtual Type type() const = 0;
  virtual void print() const = 0;
};

class Cell : public Object {
 private:
  std::shared_ptr<Object> a, d;

 public:
  Cell() : Object(), a(nullptr), d(nullptr) {
    return;
  }

  template <typename T, typename U>
  Cell(T&& a_, U&& d_)
      : Object(),
        a(std::forward<T>(a_)),
        d(std::forward<U>(d_)) {
    return;
  }

  ~Cell() override {
    return;
  }

  Type type() const override {
    return Type::cell;
  }

  void print(const std::shared_ptr<Object>& object) const {
    if (object == nullptr) {
      printf("()");
    } else {
      object->print();
    }
    return;
  }

  void print() const override {
    printf("(");
    print(a);
    auto object_d = d;
    for (; object_d != nullptr;) {
      printf(" ");
      if (object_d->type() == Type::cell) {
        auto cell = std::dynamic_pointer_cast<Cell>(object_d);
        print(cell->car());
        object_d = cell->cdr();
      } else {
        printf(". ");
        object_d->print();
        break;
      }
    }
    printf(")");
    return;
  }

  const std::shared_ptr<Object>& car() const {
    return a;
  }

  const std::shared_ptr<Object>& cdr() const {
    return d;
  }

  template <typename T>
  void set_car(T&& a_) {
    a = std::forward<T>(a_);
    return;
  }

  template <typename T>
  void set_cdr(T&& d_) {
    d = std::forward<T>(d_);
    return;
  }
};

class Token : public Object {
 private:
  const TokenID id;

  explicit Token(TokenID id_) : id(id_) {
    return;
  }

 public:
  Token() = delete;

  static std::shared_ptr<Token> make(TokenID id) {
    struct impl : public Token {
      explicit impl(TokenID id_) : Token(id_) {}
    };
    return std::make_shared<impl>(id);
  }

  ~Token() override {
    return;
  }

  Type type() const override {
    return Type::token;
  }

  void print() const override {
    printf("%zd", id);
    return;
  }

  TokenID get_id() const {
    return id;
  }
};

class File {
 private:
  std::vector<uint8_t> source;
  std::size_t index;

  enum class TokenType {
    unknown = 0,
    parent,
    boolean, number, character, string, id, prefix, dot,
  };

  using Unicode = uint32_t;
  std::map<std::vector<Unicode>, TokenID> forward_map;
  std::map<TokenID, std::vector<Unicode>> backword_map;
  std::map<TokenID, TokenType> type_from_id;

 public:
  explicit File(std::vector<uint8_t>&& source_)
      : source(std::move(source_)),
        index(0),
        forward_map{},
        backword_map{} {
    init_maps();
    return;
  }

  std::shared_ptr<Object> read() {
    auto first_token = get_next_token_id();
    switch (type_from_id[first_token]) {
      case TokenType::boolean:
      case TokenType::number:
      case TokenType::character:
      case TokenType::string:
      case TokenType::id:
        return Token::make(first_token);
      case TokenType::prefix:
        // prefix item -> '(prefix item)
        return std::make_shared<Cell>(Token::make(first_token),
               std::make_shared<Cell>(read(), nullptr));
      case TokenType::dot:
      case TokenType::unknown:
        return nullptr;
      case TokenType::parent:
        break;
    }
    if (first_token == static_cast<TokenID>(SpecialTokenID::rparent)) {
      return nullptr;
    }
    auto ret = std::make_shared<Cell>();
    auto current = ret;
    std::shared_ptr<Cell> current_prev = nullptr;
    for (;;) {
      auto old_index = index;
      auto second_token = get_next_token_id();
      if (second_token == static_cast<TokenID>(SpecialTokenID::rparent)) {
        if (current_prev == nullptr) {
          // (a b) == (a . (b . nil))
          // (a) == (a . nil)
          // () == nil
          return nullptr;
        } else {
          return ret;
        }
      } else if (second_token == static_cast<TokenID>(SpecialTokenID::dot)) {
        if (current_prev == nullptr) {
          // invalid `("(" "." ,@any)
          return nullptr;
        } else {
          current_prev->set_cdr(read());
          auto last_token = get_next_token_id();
          if (last_token != static_cast<TokenID>(SpecialTokenID::rparent)) {
            return nullptr;
          }
          return ret;
        }
      }
      index = old_index;
      current->set_car(read());
      if (current_prev != nullptr) {
        current_prev->set_cdr(current);
      }
      current_prev = std::move(current);
      current = std::make_shared<Cell>();
    }
  }

  bool eof() {
    return index == source.size();
  }

  TokenType token_type_from_id(TokenID id) {
    auto it = type_from_id.find(id);
    if (it == type_from_id.end()) {
      return TokenType::unknown;
    } else {
      return it->second;
    }
  }

  const std::vector<Unicode>& token_from_id(TokenID id) {
    auto it = backword_map.find(id);
    if (it == backword_map.end()) {
      return backword_map[static_cast<TokenID>(SpecialTokenID::nil)];
    } else {
      return it->second;
    }
  }

 private:
  void init_maps() {
    regist_as({},              SpecialTokenID::nil,         TokenType::unknown);
    regist_as({'#', 't'},      SpecialTokenID::t,           TokenType::boolean);
    regist_as({'#', 'f'},      SpecialTokenID::f,           TokenType::boolean);
    regist_as({'('},           SpecialTokenID::lparent,     TokenType::parent);
    regist_as({')'},           SpecialTokenID::rparent,     TokenType::parent);
    regist_as({'\''},          SpecialTokenID::quote,       TokenType::prefix);
    regist_as({'`'},           SpecialTokenID::quasiquote,  TokenType::prefix);
    regist_as({','},           SpecialTokenID::comma,       TokenType::prefix);
    regist_as({',', '@'},      SpecialTokenID::comma_at,    TokenType::prefix);
    regist_as({'.'},           SpecialTokenID::dot,         TokenType::dot);
    regist_as({'.', '.', '.'}, SpecialTokenID::dots,        TokenType::id);
    regist_as({'c', 'o', 'n', 's'}, SpecialTokenID::cons,   TokenType::id);
    regist_as({'c', 'a', 'r'},      SpecialTokenID::car,    TokenType::id);
    regist_as({'c', 'd', 'r'},      SpecialTokenID::cdr,    TokenType::id);
    regist_as({'a', 't', 'o', 'm'}, SpecialTokenID::atom,   TokenType::id);
    regist_as({'e', 'q'},           SpecialTokenID::eq,     TokenType::id);
    regist_as({'c', 'o', 'n', 'd'},
              SpecialTokenID::cond,   TokenType::id);
    regist_as({'l', 'a', 'm', 'b', 'd', 'a'},
              SpecialTokenID::lambda, TokenType::id);
    regist_as({'d', 'e', 'f', 'i', 'n', 'e'},
              SpecialTokenID::define, TokenType::id);
    regist_as({'q', 'u', 'o', 't', 'e'},
              SpecialTokenID::quote2, TokenType::id);
    regist_as({'+'},      SpecialTokenID::add, TokenType::id);
    regist_as({'-'},      SpecialTokenID::sub, TokenType::id);
    regist_as({'*'},      SpecialTokenID::mul, TokenType::id);
    regist_as({'/'},      SpecialTokenID::div, TokenType::id);
    regist_as({'%'},      SpecialTokenID::mod, TokenType::id);
    regist_as({'<', '='}, SpecialTokenID::le,  TokenType::id);
    regist_as({'<'},      SpecialTokenID::lt,  TokenType::id);
    regist_as({'>', '='}, SpecialTokenID::ge,  TokenType::id);
    regist_as({'>'},      SpecialTokenID::gt,  TokenType::id);
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

  TokenID get_next_token_id() {
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
            return regist(std::move(token), TokenType::string);
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
        return get_next_token_id();
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
          return regist(std::move(token), TokenType::id);
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
          return regist(std::move(token), TokenType::character);
        } else {
          return static_cast<TokenID>(SpecialTokenID::nil);
        }
      }
      default:
        break;
    }
    std::vector<Unicode> token{c0};
    auto type = TokenType::unknown;
    if (c0 == '.' || c0 == '+' || c0 == '-' || ('0' <= c0 && c0 <= '9')) {
      type = TokenType::number;
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
      type = TokenType::id;
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
    return regist(std::move(token), type);
  }

  TokenID regist(std::vector<Unicode>&& token, TokenType type) {
    auto it = forward_map.find(token);
    if (it == forward_map.end()) {
      auto new_id = static_cast<TokenID>(forward_map.size());
      forward_map[token] = new_id;
      backword_map[new_id] = std::move(token);
      type_from_id[new_id] = type;
      return new_id;
    } else {
      return it->second;
    }
  }

  void regist_as(std::vector<Unicode>&& token,
                 SpecialTokenID sid,
                 TokenType type) {
    auto id = static_cast<TokenID>(sid);
    forward_map[token] = id;
    backword_map[id] = std::move(token);
    type_from_id[id] = type;
    return;
  }
};

enum class ISA {
  load, cons, car, cdr, atom, eq
};

struct Instruction {
  ISA instruction;
  uint64_t operand[3];

  explicit Instruction(ISA inst,
                       uint64_t o1 = 0,
                       uint64_t o2 = 0,
                       uint64_t o3 = 0)
      : instruction(inst),
        operand{o1, o2, o3} {
    return;
  }

  void print() {
    switch (instruction) {
      case ISA::load:
        printf("load r%zu, %zu\n", operand[0], operand[1]);
        break;
      case ISA::cons:
        printf("cons r%zu, r%zu, r%zu\n", operand[0], operand[1], operand[2]);
        break;
      case ISA::car:
        printf("car r%zu, r%zu\n", operand[0], operand[1]);
        break;
      case ISA::cdr:
        printf("cdr r%zu, r%zu\n", operand[0], operand[1]);
        break;
      case ISA::atom:
        printf("atom r%zu, r%zu\n", operand[0], operand[1]);
        break;
      case ISA::eq:
        printf("eq r%zu, r%zu, r%zu\n", operand[0], operand[1], operand[2]);
        break;
    }
    return;
  }
};

struct Snippet {
  std::shared_ptr<std::vector<Instruction>> instructions;

  Snippet() : instructions(std::make_shared<std::vector<Instruction>>()) {
    return;
  }

  void push_back(Instruction&& inst) {
    instructions->push_back(std::move(inst));
    return;
  }

  void print() {
    for (auto it = instructions->begin(); it != instructions->end(); ++it) {
      it->print();
    }
  }
};

Snippet compile(std::shared_ptr<Object> x,
             uint64_t shift_width,
             struct Snippet&& snippet) {
  if (x->type() == Type::token) {
    auto id = std::dynamic_pointer_cast<Token>(x)->get_id();
    snippet.push_back(Instruction(ISA::load, shift_width, id));
  } else {
    auto x_ = std::dynamic_pointer_cast<Cell>(x);
    auto ax = x_->car();
    auto dx = x_->cdr();
    if (ax->type() == Type::token) {
      auto op = std::dynamic_pointer_cast<Token>(ax)->get_id();
      if (op == static_cast<TokenID>(SpecialTokenID::cons)) {
        if (dx == nullptr || dx->type() != Type::cell) {
          fprintf(stderr, "error.\n");
          return {};
        }
        auto dx_ = std::dynamic_pointer_cast<Cell>(dx);
        auto adx = dx_->car();
        auto ddx = dx_->cdr();
        snippet = compile(adx, shift_width, std::move(snippet));
        if (ddx->type() != Type::cell) {
          fprintf(stderr, "error.\n");
          return {};
        }
        auto ddx_ = std::dynamic_pointer_cast<Cell>(ddx);
        auto addx = ddx_->car();
        auto dddx = ddx_->cdr();
        snippet = compile(addx, shift_width + 1, std::move(snippet));
        if (dddx != nullptr) {
          fprintf(stderr, "error.\n");
          return {};
        }
        snippet.push_back(Instruction(ISA::cons,
                                      shift_width,
                                      shift_width,
                                      shift_width + 1));
      } else if (op == static_cast<TokenID>(SpecialTokenID::car)) {
        if (dx == nullptr || dx->type() != Type::cell) {
          fprintf(stderr, "error.\n");
          return {};
        }
        auto dx_ = std::dynamic_pointer_cast<Cell>(dx);
        auto adx = dx_->car();
        auto ddx = dx_->cdr();
        snippet = compile(adx, shift_width, std::move(snippet));
        if (ddx != nullptr) {
          fprintf(stderr, "error.\n");
        }
        snippet.push_back(Instruction(ISA::car, shift_width, shift_width));
      } else if (op == static_cast<TokenID>(SpecialTokenID::cdr)) {
        if (dx == nullptr || dx->type() != Type::cell) {
          fprintf(stderr, "error.\n");
          return {};
        }
        auto dx_ = std::dynamic_pointer_cast<Cell>(dx);
        auto adx = dx_->car();
        auto ddx = dx_->cdr();
        snippet = compile(adx, shift_width, std::move(snippet));
        if (ddx != nullptr) {
          fprintf(stderr, "error.\n");
        }
        snippet.push_back(Instruction(ISA::cdr, shift_width, shift_width));
      } else if (op == static_cast<TokenID>(SpecialTokenID::atom)) {
        if (dx == nullptr || dx->type() != Type::cell) {
          fprintf(stderr, "error.\n");
          return {};
        }
        auto dx_ = std::dynamic_pointer_cast<Cell>(dx);
        auto adx = dx_->car();
        auto ddx = dx_->cdr();
        snippet = compile(adx, shift_width, std::move(snippet));
        if (ddx != nullptr) {
          fprintf(stderr, "error.\n");
          return {};
        }
        snippet.push_back(Instruction(ISA::atom, shift_width, shift_width));
      } else if (op == static_cast<TokenID>(SpecialTokenID::eq)) {
        if (dx == nullptr || dx->type() != Type::cell) {
          fprintf(stderr, "error.\n");
          return {};
        }
        auto dx_ = std::dynamic_pointer_cast<Cell>(dx);
        auto adx = dx_->car();
        auto ddx = dx_->cdr();
        snippet = compile(adx, shift_width, std::move(snippet));
        if (ddx->type() != Type::cell) {
          fprintf(stderr, "error.\n");
          return {};
        }
        auto ddx_ = std::dynamic_pointer_cast<Cell>(ddx);
        auto addx = ddx_->car();
        auto dddx = ddx_->cdr();
        snippet = compile(addx, shift_width + 1, std::move(snippet));
        if (dddx != nullptr) {
          fprintf(stderr, "error.\n");
          return {};
        }
        snippet.push_back(Instruction(ISA::eq,
                                      shift_width,
                                      shift_width,
                                      shift_width + 1));
      }
    }
  }
  return std::move(snippet);
}

void eval(std::vector<uint8_t>&& stream) {
  File file(std::move(stream));
  for (;;) {
    // parse
    auto list = file.read();
    if (list == nullptr) {
      break;
    }

    // print
    list->print();
    puts("");

    // compile
    compile(list, 0, {}).print();
    puts("");
  }
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
