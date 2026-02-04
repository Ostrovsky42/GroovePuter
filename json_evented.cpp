#include "json_evented.h"

#include <cctype>
#include <cstdlib>
#include <utility>

namespace {
class CharStream {
public:
  explicit CharStream(JsonVisitor::NextChar nextChar) : nextChar_(std::move(nextChar)) {}

  bool get(char& c) {
    if (hasBuffered_) {
      hasBuffered_ = false;
      c = buffered_;
      // printf("CharStream::get: returning buffered '%c' (%d)\n", c, (int)c);
      return true;
    }
    int value = nextChar_();
    if (value < 0) {
      printf("CharStream::get: nextChar returned %d (EOF)\n", value);
      return false;
    }
    c = static_cast<char>(value);
    printf("CharStream::get: read '%c' (%d)\n", c, (int)c);
    return true;
  }

  bool peek(char& c) {
    if (hasBuffered_) {
      c = buffered_;
      return true;
    }
    int value = nextChar_();
    if (value < 0) {
      printf("CharStream::peek: nextChar returned %d (EOF)\n", value);
      return false;
    }
    buffered_ = static_cast<char>(value);
    hasBuffered_ = true;
    c = buffered_;
    printf("CharStream::peek: read '%c' (%d)\n", c, (int)c);
    return true;
  }

  void skipWhitespace() {
    char c;
    while (peek(c)) {
      if (!std::isspace(static_cast<unsigned char>(c))) break;
      hasBuffered_ = false;
    }
  }

private:
  JsonVisitor::NextChar nextChar_;
  char buffered_ = 0;
  bool hasBuffered_ = false;
};

bool parseValue(CharStream& stream, JsonObserver& observer);

bool parseLiteral(CharStream& stream, const char* literal) {
  for (const char* ptr = literal; *ptr; ++ptr) {
    char c;
    if (!stream.get(c)) {
      printf("JsonVisitor::parseLiteral: Unexpected EOF for literal '%s'\n", literal);
      return false;
    }
    if (c != *ptr) {
      printf("JsonVisitor::parseLiteral: Mismatch for '%s', expected '%c', got '%c' (%d)\n", literal, *ptr, c, (int)c);
      return false;
    }
  }
  return true;
}

bool parseString(CharStream& stream, std::string& out) {
  out.clear();
  char c;
  while (stream.get(c)) {
    if (c == '"') return true;
    if (c == '\\') {
      char esc;
      if (!stream.get(esc)) return false;
      switch (esc) {
      case '"': out.push_back('"'); break;
      case '\\': out.push_back('\\'); break;
      case '/': out.push_back('/'); break;
      case 'b': out.push_back('\b'); break;
      case 'f': out.push_back('\f'); break;
      case 'n': out.push_back('\n'); break;
      case 'r': out.push_back('\r'); break;
      case 't': out.push_back('\t'); break;
      case 'u': {
        // Minimal \uXXXX handling: consume four hex digits and skip unicode conversion.
        for (int i = 0; i < 4; ++i) {
          if (!stream.get(esc) || !std::isxdigit(static_cast<unsigned char>(esc))) return false;
        }
        out.push_back('?');
        break;
      }
      default:
        return false;
      }
    } else {
      out.push_back(c);
    }
  }
  return false;
}

bool parseNumber(CharStream& stream, JsonObserver& observer, char firstChar) {
  std::string numStr;
  numStr.push_back(firstChar);
  char c;
  while (stream.peek(c)) {
    if (std::isdigit(static_cast<unsigned char>(c)) || c == '.' || c == 'e' || c == 'E' || c == '+' || c == '-') {
      stream.get(c);
      numStr.push_back(c);
    } else {
      break;
    }
  }

  bool isFloat = (numStr.find('.') != std::string::npos) || (numStr.find('e') != std::string::npos) ||
                 (numStr.find('E') != std::string::npos);
  const char* start = numStr.c_str();
  char* endPtr = nullptr;
  if (isFloat) {
    double value = std::strtod(start, &endPtr);
    if (endPtr != start + static_cast<int>(numStr.size())) return false;
    observer.onNumber(value);
  } else {
    long long value = std::strtoll(start, &endPtr, 10);
    if (endPtr != start + static_cast<int>(numStr.size())) return false;
    observer.onNumber(static_cast<int>(value));
  }
  return true;
}

bool parseArray(CharStream& stream, JsonObserver& observer) {
  observer.onArrayStart();
  stream.skipWhitespace();
  char c;
  if (stream.peek(c) && c == ']') {
    stream.get(c);
    observer.onArrayEnd();
    return true;
  }

  while (true) {
    observer.onObjectValueStart();
    if (!parseValue(stream, observer)) return false;
    observer.onObjectValueEnd();
    stream.skipWhitespace();
    if (!stream.get(c)) {
      printf("JsonVisitor::parseArray: Unexpected EOF, expected ',' or ']'\n");
      return false;
    }
    if (c == ']') {
      observer.onArrayEnd();
      return true;
    }
    if (c != ',') {
      printf("JsonVisitor::parseArray: Expected ',' or ']', got '%c' (%d)\n", c, (int)c);
      return false;
    }
    stream.skipWhitespace();
  }
}

bool parseObject(CharStream& stream, JsonObserver& observer) {
  observer.onObjectStart();
  stream.skipWhitespace();
  char c;
  if (stream.peek(c) && c == '}') {
    stream.get(c);
    observer.onObjectEnd();
    return true;
  }

  while (true) {
    if (!stream.get(c)) {
      printf("JsonVisitor::parseObject: Unexpected EOF, expected key\n");
      return false;
    }
    if (c != '"') {
      printf("JsonVisitor::parseObject: Expected '\"' (key start), got '%c' (%d)\n", c, (int)c);
      return false;
    }
    std::string key;
    if (!parseString(stream, key)) return false;
    observer.onObjectKey(key);
    stream.skipWhitespace();
    if (!stream.get(c) || c != ':') {
      printf("JsonVisitor::parseObject: Expected ':', got '%c' (%d)\n", c, (int)c);
      return false;
    }
    stream.skipWhitespace();
    observer.onObjectValueStart();
    if (!parseValue(stream, observer)) return false;
    observer.onObjectValueEnd();
    stream.skipWhitespace();
    if (!stream.get(c)) {
      printf("JsonVisitor::parseObject: Unexpected EOF, expected ',' or '}'\n");
      return false;
    }
    if (c == '}') {
      observer.onObjectEnd();
      return true;
    }
    if (c != ',') {
      printf("JsonVisitor::parseObject: Expected ',' or '}', got '%c' (%d)\n", c, (int)c);
      return false;
    }
    stream.skipWhitespace();
  }
}

bool parseValue(CharStream& stream, JsonObserver& observer) {
  stream.skipWhitespace();
  char c;
  if (!stream.get(c)) {
    // This is often just EOF at the very end, but if it's during parseValue, it's an error.
    return false;
  }
  switch (c) {
  case '{':
    return parseObject(stream, observer);
  case '[':
    return parseArray(stream, observer);
  case '"': {
    std::string value;
    if (!parseString(stream, value)) return false;
    observer.onString(value);
    return true;
  }
  case 't':
    if (!parseLiteral(stream, "rue")) return false;
    observer.onBool(true);
    return true;
  case 'f':
    if (!parseLiteral(stream, "alse")) return false;
    observer.onBool(false);
    return true;
  case 'n':
    if (!parseLiteral(stream, "ull")) return false;
    observer.onNull();
    return true;
  default:
    if (c == '-' || std::isdigit(static_cast<unsigned char>(c))) {
      return parseNumber(stream, observer, c);
    }
    printf("JsonVisitor::parseValue: Unexpected character '%c' (%d)\n", c, (int)c);
    return false;
  }
}
} // namespace

bool JsonVisitor::parse(const std::string& input, JsonObserver& observer) {
  size_t idx = 0;
  auto nextChar = [&input, &idx]() -> int {
    if (idx >= input.size()) return -1;
    return static_cast<unsigned char>(input[idx++]);
  };
  return parseImpl(nextChar, observer);
}

bool JsonVisitor::parseImpl(NextChar nextChar, JsonObserver& observer) {
  CharStream stream(std::move(nextChar));
  if (!parseValue(stream, observer)) return false;
  stream.skipWhitespace();
  char extra;
  return !stream.peek(extra);
}
