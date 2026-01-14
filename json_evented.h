#ifndef JSON_EVENTED_H
#define JSON_EVENTED_H


#include <functional>
#include <string>

class JsonObserver {
public:
  virtual ~JsonObserver() = default;
  virtual void onObjectStart() = 0;
  virtual void onObjectEnd() = 0;
  virtual void onArrayStart() = 0;
  virtual void onArrayEnd() = 0;
  virtual void onNumber(int value) = 0;
  virtual void onNumber(double value) = 0;
  virtual void onBool(bool value) = 0;
  virtual void onNull() = 0;
  virtual void onString(const std::string& value) = 0;
  virtual void onObjectKey(const std::string& key) = 0;
  virtual void onObjectValueStart() = 0;
  virtual void onObjectValueEnd() = 0;
};

class JsonVisitor {
public:
  using NextChar = std::function<int()>;

  bool parse(const std::string& input, JsonObserver& observer);

  template <typename Stream>
  bool parse(Stream& stream, JsonObserver& observer) {
    auto nextChar = [&stream]() -> int {
      return stream.read();
    };
    return parseImpl(nextChar, observer);
  }

private:
  bool parseImpl(NextChar nextChar, JsonObserver& observer);
};
#endif // JSON_EVENTED_H
