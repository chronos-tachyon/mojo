#ifndef NET_URL_QUERY_H
#define NET_URL_QUERY_H

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/result.h"
#include "base/strings.h"

namespace net {
namespace url {

std::string query_escape(base::StringPiece raw);
std::pair<bool, std::string> query_unescape(base::StringPiece escaped);

class Query {
 public:
  Query() = default;
  Query(const Query& other) : map_(copy(other.map_)) {}
  Query(Query&& other) = default;

  Query& operator=(const Query& other) {
    map_ = copy(other.map_);
    return *this;
  }
  Query& operator=(Query&& other) = default;

  base::Result parse(base::StringPiece raw);

  explicit operator bool() const noexcept { return !empty(); }
  bool empty() const noexcept { return map_.empty(); }
  std::size_t size() const noexcept { return map_.size(); }

  std::vector<base::StringPiece> keys() const;
  std::map<base::StringPiece, std::vector<base::StringPiece>> items() const;

  std::pair<bool, base::StringPiece> get(base::StringPiece key) const;
  std::pair<bool, base::StringPiece> get_last(base::StringPiece key) const;
  std::vector<base::StringPiece> get_all(base::StringPiece key) const;

  void clear() { map_.clear(); }
  void swap(Query& other) noexcept {
    using std::swap;
    swap(map_, other.map_);
  }

  void set(base::StringPiece key, std::vector<base::StringPiece> values);
  void set(base::StringPiece key, base::StringPiece value);
  void add(base::StringPiece key, std::vector<base::StringPiece> values);
  void add(base::StringPiece key, base::StringPiece value);
  void remove(base::StringPiece key);

  void append_to(std::string* out) const;
  std::string as_string() const;

  friend bool operator==(const Query& a, const Query& b);
  friend bool operator!=(const Query& a, const Query& b) { return !(a == b); }

 private:
  struct Item {
    std::string key;
    std::vector<std::string> values;
  };

  using Map = std::map<base::StringPiece, std::unique_ptr<Item>>;

  static Map copy(const Map& in);

  const Item* find(base::StringPiece key) const noexcept;
  Item& find_or_insert(base::StringPiece key);

  Map map_;
};

inline void swap(Query& a, Query& b) noexcept { a.swap(b); }

}  // namespace url
}  // namespace net

#endif  // NET_URL_QUERY_H
