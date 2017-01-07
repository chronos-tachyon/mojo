// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "path/path.h"

namespace path {

std::string clean(const std::string& path) {
  std::string out;
  out.reserve(path.size());

  const char* p = path.data();
  const char* q = p + path.size();

  if (p == q) {
    out.push_back('.');
    return out;
  }

  bool rooted = (*p == '/');
  if (rooted) {
    out.push_back('/');
    ++p;
  }

  auto backtrack = [&out, &rooted] {
    if (rooted) {
      while (true) {
        char ch = out.back();
        out.pop_back();
        if (ch == '/') break;
      }
      if (out.empty()) out.push_back('/');
    } else if (out.empty()) {
      out.push_back('.');
      out.push_back('.');
    } else {
      while (!out.empty()) {
        char ch = out.back();
        out.pop_back();
        if (ch == '/') break;
      }
    }
  };

  while (p != q) {
    // Handle repeated slashes
    if (*p == '/') {
      ++p;
      continue;
    }

    // Handle '.' and '..'
    if (*p == '.') {
      ++p;
      // partial: '.<unexamined>'
      if (p == q) {
        // '.$'
        break;
      }
      if (*p == '/') {
        // './<rest>'
        ++p;
        continue;
      }
      if (*p == '.') {
        ++p;
        // partial: '..<unexamined>'
        if (p == q) {
          // '..$'
          backtrack();
          break;
        }
        if (*p == '/') {
          // '../<rest>'
          ++p;
          backtrack();
          continue;
        }
        // '..foo'
        --p;
      }
      // '.foo'
      --p;
    }

    // Add missing slash:
    //    'foo'  -> 'foo/'
    //    '/foo' -> '/foo/'
    //    '/'    -> '/'
    if (!out.empty() && out.back() != '/') out.push_back('/');

    // Append component characters:
    //    'foo/'  -> 'foo/bar'
    //    '/foo/' -> '/foo/bar'
    //    '/'     -> '/bar'
    while (p != q && *p != '/') {
      out.push_back(*p);
      ++p;
    }

    // Skip past the component's final slash, if any
    if (p != q) ++p;
  }

  // Turn '' -> '.'
  if (out.empty()) out.push_back('.');
  return out;
}

}  // namespace path
