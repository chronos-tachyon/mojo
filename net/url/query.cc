#include "net/url/query.h"

#include <algorithm>
#include <tuple>

#include "net/url/_internal.h"
#include "base/backport.h"

namespace net {
namespace url {

using internal::EscapeMode;

std::string query_escape(base::StringPiece raw) {
  return internal::escape(raw, EscapeMode::query_component);
}

std::pair<bool, std::string> query_unescape(base::StringPiece escaped) {
  return internal::unescape(escaped, EscapeMode::query_component);
}

Query::Map Query::copy(const Query::Map& in) {
  Map out;
  for (const auto& pair : in) {
    auto item = base::backport::make_unique<Item>(*pair.second);
    base::StringPiece key = item->key;
    out[key] = std::move(item);
  }
  return out;
}

const Query::Item* Query::find(base::StringPiece key) const noexcept {
  auto it = map_.find(key);
  if (it == map_.end()) return nullptr;
  return it->second.get();
}

Query::Item& Query::find_or_insert(base::StringPiece key) {
  Item* item;
  auto range = map_.equal_range(key);
  if (range.first == range.second) {
    auto ptr = base::backport::make_unique<Item>();
    item = ptr.get();
    item->key = key;
    map_.emplace_hint(range.first, item->key, std::move(ptr));
  } else {
    item = range.first->second.get();
  }
  return *item;
}

base::Result Query::parse(base::StringPiece raw) {
  clear();

  auto is_key = [](char ch) -> bool {
    return ch != '?' && ch != '&' && ch != ';' && ch != '=';
  };
  auto is_val = [](char ch) -> bool {
    return ch != '?' && ch != '&' && ch != ';';
  };

  auto p = raw.begin(), q = raw.end();
  std::string key, value;
  while (true) {
    // Skip leading [?&;]*
    while (p != q && !is_val(*p)) ++p;
    if (p == q) break;

    // Capture [^?&;=]* -> key
    while (p != q && is_key(*p)) {
      key.push_back(*p);
      ++p;
    }

    // Determine EOF (0), '?', '&', ';', or '='
    char ch = ((p != q) ? *p : 0);

    // For EOF, '?', '&', ';' while reading key:
    // treat captured "key" as value of empty key ""
    if (ch != '=') {
      key.swap(value);
      auto result = query_unescape(value);
      if (!result.first)
        return base::Result::invalid_argument(
            "malformed '%'-escape in query string component");
      add("", result.second);
      value.clear();
      continue;
    }
    // For '=': skip to next char
    ++p;

    // Capture [^?&;]* -> value
    while (p != q && is_val(*p)) {
      value.push_back(*p);
      ++p;
    }
    auto key_result = query_unescape(key);
    auto val_result = query_unescape(value);
    if (!key_result.first)
      return base::Result::invalid_argument(
          "malformed '%'-escape in query string key");
    if (!val_result.first)
      return base::Result::invalid_argument(
          "malformed '%'-escape in query string component");
    add(key_result.second, val_result.second);
    key.clear();
    value.clear();
  }
  return base::Result();
}

std::vector<base::StringPiece> Query::keys() const {
  std::vector<base::StringPiece> out;
  out.reserve(map_.size());
  for (const auto& pair : map_) {
    out.push_back(pair.first);
  }
  return out;
}

std::map<base::StringPiece, std::vector<base::StringPiece>> Query::items() const {
  std::map<base::StringPiece, std::vector<base::StringPiece>> out;
  for (const auto& pair : map_) {
    const auto& item = *pair.second;
    auto& v = out[item.key];
    v.reserve(item.values.size());
    for (const auto& value : item.values) {
      v.push_back(value);
    } 
  }
  return out;
}

std::pair<bool, base::StringPiece> Query::get(base::StringPiece key) const {
  auto item = find(key);
  if (item && !item->values.empty())
    return std::make_pair(true, item->values.front());
  else
    return std::make_pair(false, nullptr);
}

std::pair<bool, base::StringPiece> Query::get_last(base::StringPiece key) const {
  auto item = find(key);
  if (item && !item->values.empty())
    return std::make_pair(true, item->values.back());
  else
    return std::make_pair(false, nullptr);
}

std::vector<base::StringPiece> Query::get_all(base::StringPiece key) const {
  std::vector<base::StringPiece> out;
  auto item = find(key);
  if (item) {
    out.reserve(item->values.size());
    for (const auto& str : item->values) {
      out.push_back(str);
    }
  }
  return out;
}

void Query::set(base::StringPiece key, std::vector<base::StringPiece> values) {
  if (values.empty()) {
    map_.erase(key);
    return;
  }

  Item& item = find_or_insert(key);
  item.values.clear();
  item.values.reserve(values.size());
  for (auto value : values) {
    item.values.push_back(value);
  }
}

void Query::set(base::StringPiece key, base::StringPiece values) {
  set(key, std::vector<base::StringPiece>{values});
}

void Query::add(base::StringPiece key, std::vector<base::StringPiece> values) {
  if (values.empty()) return;

  Item& item = find_or_insert(key);
  item.values.reserve(item.values.size() + values.size());
  for (auto value : values) {
    item.values.push_back(value);
  }
}

void Query::add(base::StringPiece key, base::StringPiece values) {
  add(key, std::vector<base::StringPiece>{values});
}

void Query::remove(base::StringPiece key) {
  map_.erase(key);
}

std::string Query::as_string() const {
  std::string out;
  append_to(&out);
  return out;
}

void Query::append_to(std::string* out) const {
  bool b = false;
  for (const auto& pair : map_) {
    const auto& item = pair.second;
    for (const auto& value : item->values) {
      if (!item->key.empty()) {
        out->append(query_escape(item->key));
        out->push_back('=');
      }
      out->append(query_escape(value));
      out->push_back('&');
      b = true;
    }
  }
  if (b) out->pop_back();
}

bool operator==(const Query& a, const Query& b) {
  auto am_iter = a.map_.begin(), am_end = a.map_.end();
  auto bm_iter = b.map_.begin(), bm_end = b.map_.end();
  while (am_iter != am_end && bm_iter != bm_end) {
    if (am_iter->first != bm_iter->first) return false;
    const auto& av = am_iter->second->values;
    const auto& bv = bm_iter->second->values;
    auto av_iter = av.begin(), av_end = av.end();
    auto bv_iter = bv.begin(), bv_end = bv.end();
    while (av_iter != av_end && bv_iter != bv_end) {
      if (*av_iter != *bv_iter) return false;
      ++av_iter;
      ++bv_iter;
    }
    if (av_iter != av_end || bv_iter != bv_end) return false;
    ++am_iter;
    ++bm_iter;
  }
  if (am_iter != am_end || bm_iter != bm_end) return false;
  return true;
}

}  // namespace url
}  // namespace net
