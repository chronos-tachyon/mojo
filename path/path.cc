// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "path/path.h"

#include "base/logging.h"

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

std::pair<std::string, std::string> split(const std::string& path) {
  std::pair<std::string, std::string> out;
  const char* begin = path.data();
  const char* end = begin + path.size();
  const char* ptr;

  // '' -> ('.', '')
  if (end == begin) {
    out.first.push_back('.');
    goto end;
  }

  // Trim trailing slashes
  //   'foo'   -> 'foo'
  //   'foo/'  -> 'foo'
  //   '/'     -> ''
  //   '/foo'  -> '/foo'
  //   '/foo/' -> '/foo'
  while (end != begin) {
    --end;
    if (*end != '/') {
      ++end;
      break;
    }
  }

  // '' (was '/') -> ('/', '/')
  if (end == begin) {
    out.first.push_back('/');
    out.second.push_back('/');
    goto end;
  }

  // Find start of final component
  //   'foo'      -> ('',      'foo')
  //   'foo/bar'  -> ('foo/',  'bar')
  //   '/foo'     -> ('/',     'foo')
  //   '/foo/bar' -> ('/foo/', 'bar')
  ptr = end;
  while (ptr != begin) {
    --ptr;
    if (*ptr == '/') {
      ++ptr;
      break;
    }
  }
  out.second.assign(ptr, end - ptr);

  // 'foo' -> ('.', 'foo')
  if (ptr == begin) {
    out.first.push_back('.');
    goto end;
  }

  // Trim trailing slashes
  //   'foo'   -> 'foo'
  //   'foo/'  -> 'foo'
  //   '/'     -> ''
  //   '/foo'  -> '/foo'
  //   '/foo/' -> '/foo'
  while (ptr != begin) {
    --ptr;
    if (*ptr != '/') {
      ++ptr;
      break;
    }
  }

  // '' (was '/') -> ('/', '')
  if (ptr == begin) {
    out.first.push_back('/');
  } else {
    out.first.assign(begin, ptr);
  }

end:
  return out;
}

void join(std::string* head, const std::string& tail) {
  CHECK_NOTNULL(head);
  if (tail.empty()) return;
  if (head->empty()) {
    *head = tail;
    return;
  }
  if (head->back() != '/' && tail.front() != '/') {
    head->push_back('/');
  }
  head->append(tail);
}

}  // namespace path
