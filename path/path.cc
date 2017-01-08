// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "path/path.h"

#include <unistd.h>

#include <algorithm>
#include <cstdlib>
#include <deque>
#include <vector>

#include "base/cleanup.h"
#include "base/logging.h"

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

namespace path {

static std::size_t count(char ch, const char* begin, const char* end) noexcept {
  std::size_t n = 0;
  while (begin != end) {
    if (*begin == ch) ++n;
    ++begin;
  }
  return n;
}

static std::size_t common_prefix(const std::string& a,
                                 const std::string& b) noexcept {
  std::size_t i = 0;
  std::size_t n = std::min(a.size(), b.size());
  while (i < n && a[i] == b[i]) ++i;
  return i;
}

static std::size_t common_prefix(const std::vector<std::string>& a,
                                 const std::vector<std::string>& b) noexcept {
  std::size_t i = 0;
  std::size_t n = std::min(a.size(), b.size());
  while (i < n && a[i] == b[i]) ++i;
  return i;
}

static base::Result readlink(std::string* out, const std::string& path) {
  CHECK_NOTNULL(out);
  out->clear();

  long pathmax = ::pathconf(path.c_str(), _PC_PATH_MAX);
  if (pathmax < PATH_MAX) pathmax = PATH_MAX;

  std::vector<char> buf;
  buf.resize(pathmax);

  ssize_t n = ::readlink(path.c_str(), buf.data(), buf.size());
  if (n < 0) {
    int err_no = errno;
    return base::Result::from_errno(err_no, "readlink(2) path=", path);
  }
  out->assign(buf.data(), n);
  return base::Result();
}

std::string partial_clean(const std::string& path) {
  std::string out;
  out.reserve(path.size());

  const char* p = path.data();
  const char* q = p + path.size();

  if (p == q) {
    out.push_back('.');
    goto end;
  }

  if (*p == '/') {
    out.push_back('/');
    ++p;
  }

  while (p != q) {
    // Handle repeated slashes
    if (*p == '/') {
      ++p;
      continue;
    }

    // Handle '.'
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

end:
  return out;
}

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

std::vector<std::string> explode(const std::string& path) {
  std::vector<std::string> out;
  const char* begin = path.data();
  const char* end = begin + path.size();
  const char* ptr;

  // "" -> {"."}
  if (begin == end) {
    out.emplace_back(".", 1);
    goto done;
  }

  // Absolute paths -> {"/", ...}
  if (*begin == '/') {
    out.emplace_back("/", 1);
    ++begin;
    while (begin != end && *begin == '/') ++begin;
  }

  // Trim trailing slashes
  while (end != begin) {
    --end;
    if (*end != '/') {
      ++end;
      break;
    }
  }

  // Find components and append them to |out|
  ptr = begin;
  while (ptr != end) {
    while (ptr != end && *ptr != '/') ++ptr;
    out.emplace_back(begin, ptr);
    while (ptr != end && *ptr == '/') ++ptr;
    begin = ptr;
  }

done:
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

std::string join(const std::vector<std::string>& vec) {
  std::string out;
  if (vec.empty()) {
    out.push_back('.');
  } else {
    out.assign(vec.front());
    auto it = vec.begin() + 1, end = vec.end();
    while (it != end) {
      join(&out, *it);
      ++it;
    }
  }
  return out;
}

std::string abspath(const std::string& path, const std::string& root) {
  if (path.empty()) return clean(root);
  if (path.front() == '/') return clean(path);
  return clean(join(root, path));
}

std::string relpath(const std::string& path, const std::string& root) {
  std::string cpath = clean(path);
  std::string croot = clean(root);

  if (!is_abs(cpath)) return cpath;

  // root         path            common      result
  //
  // [len == croot.size() && len == cpath.size()]
  // "/"          "/"             "/"         "."
  // "/foo"       "/foo"          "/foo"      "."
  // "/foo/bar"   "/foo/bar"      "/foo/bar"  "."
  //
  // [len == croot.size() && len == 1]
  // "/"          "/foo"          "/"         "foo"
  //
  // [len == croot.size() && cpath[len] == '/']
  // "/foo"       "/foo/bar"      "/foo"      "bar"
  // "/foo/bar"   "/foo/bar/baz"  "/foo/bar"  "baz"
  //
  // [len == croot.size()]
  // "/foo"       "/foobar"       "/foo"      "../foobar"
  //
  // [len == cpath.size()]
  // "/foo"       "/"             "/"         ".."
  // "/foo/bar"   "/"             "/"         "../.."
  // "/foo/bar"   "/foo"          "/foo"      ".."
  //
  // [other]
  // "/foo"       "/foobar"       "/"         "../foobar"
  // "/foo"       "/bar"          "/"         "../bar"
  // "/foo/bar"   "/foo/baz"      "/foo/ba"   "../baz"
  // "/foo/bar"   "/baz"          "/"         "../../baz"
  // "/foobar"    "/foobaz"       "/fooba"    "../foobaz"

  std::size_t len = common_prefix(cpath, croot);
  if (len == croot.size()) {
    if (len == cpath.size()) {
      return ".";
    }
    if (len == 1) {
      return cpath.substr(1);
    }
    if (cpath[len] == '/') {
      return cpath.substr(len + 1);
    }
  }

  if (len == cpath.size()) {
    const char* p = croot.data();
    const char* q = p + croot.size();
    p += len;
    if (*p == '/') ++p;
    std::size_t dotdots = 1 + count('/', p, q);

    std::string out;
    while (dotdots--) join(&out, "..");
    return out;
  }

  while (true) {
    --len;
    if (croot[len] == '/') break;
  }
  ++len;

  const char* p = croot.data() + len;
  const char* q = p + croot.size() - len;
  std::size_t dotdots = 1 + count('/', p, q);

  std::string out;
  while (dotdots--) join(&out, "..");
  join(&out, cpath.substr(len));
  return out;
}

base::Result cwd(std::string* out) {
  CHECK_NOTNULL(out);

  char* ptr = ::get_current_dir_name();
  if (ptr == nullptr) {
    int err_no = errno;
    return base::Result::from_errno(err_no, "get_current_dir_name(3)");
  }
  auto cleanup = base::cleanup([ptr] { ::free(ptr); });
  CHECK(*ptr == '/');
  out->assign(ptr);
  return base::Result();
}

static base::Result canonicalize_exploded(std::vector<std::string>* vec) {
  CHECK_NOTNULL(vec);
  CHECK(vec->size() > 0 && vec->front() == "/");

  std::vector<std::string> out;
  out.reserve(vec->size());

  std::deque<std::string> q;
  q.insert(q.end(), vec->begin(), vec->end());

  while (!q.empty()) {
    const auto component = q.front();
    q.pop_front();

    if (component != "..") {
      if (component != ".") out.push_back(component);
      continue;
    }

    // Found "..", so need to backtrack by one component.
    // But... is that component a symlink?  And if so, where does it point?

    // Root directory is never a link, and should never be backtracked past.
    if (out.size() == 1 && out.front() == "/") continue;

    // Determine if the path so far ends in a symlink.
    bool islink;
    std::string linktext;
    auto r = readlink(&linktext, join(out));
    if (r) {
      islink = true;
    } else {
      // EINVAL -> not a symlink
      // ENOENT -> does not exist, therefore not a symlink
      int err_no = r.errno_value();
      if (err_no != EINVAL && err_no != ENOENT) return r;
      islink = false;
    }

    // Not a link?  Just backtrack.
    if (!islink) {
      out.pop_back();
      continue;
    }

    // Stuff the link's components into the front of the queue,
    // followed by the ".." that we thought we'd consumed.
    if (is_abs(linktext)) out.clear();
    auto tmp = explode(linktext);
    q.push_front(component);
    q.insert(q.begin(), tmp.begin(), tmp.end());
  }

  *vec = std::move(out);
  return base::Result();
}

base::Result canonicalize(std::string* path) {
  CHECK_NOTNULL(path);

  std::string in = *path;
  if (!is_abs(in)) {
    std::string tmp;
    auto r = cwd(&tmp);
    if (!r) return r;
    join(&tmp, in);
    in = std::move(tmp);
  }

  std::vector<std::string> vec;
  vec = explode(in);
  auto r = canonicalize_exploded(&vec);
  if (!r) return r;

  *path = join(vec);
  return base::Result();
}

base::Result make_abs(std::string* path, const std::string& root) {
  CHECK_NOTNULL(path);
  CHECK(root.empty() || root.front() == '/')
      << ": root must be an absolute path";

  if (!is_abs(*path) && !root.empty()) {
    *path = join(root, *path);
  }
  return canonicalize(path);
}

base::Result make_rel(std::string* path, const std::string& root) {
  CHECK_NOTNULL(path);
  CHECK(root.empty() || root.front() == '/')
      << ": root must be an absolute path";

  if (!is_abs(*path)) {
    *path = partial_clean(*path);
    return base::Result();
  }

  std::string croot = root;
  auto r = canonicalize(&croot);
  if (!r) return r;

  std::vector<std::string> xroot, xpath, tmp, out;
  xroot = explode(croot);
  xpath = explode(*path);

  // Our task?  Find the relative path whose canonicalization
  // is equal to the canonicalization of root.

  // The |len == xroot.size()| case is easy (pure syntax):
  //
  // root = "/home/chronos"
  // path = "/home/chronos"
  // correct result = "."
  //
  // root = "/home/chronos"
  // path = "/home/chronos/src/mojo"
  // correct result = "src/mojo"

  std::size_t len = common_prefix(xroot, xpath);
  if (len == xroot.size()) {
    if (len == xpath.size()) {
      out.push_back(".");
    } else {
      out.insert(out.begin(), xpath.begin() + len, xpath.end());
    }
    goto done;
  }

  // The remaining cases get very complicated, very fast:
  //
  // root = "/home/chronos/src/mojo/bazel-bin/path"
  // path = "/home/chronos/src/mojo"
  //
  // symlink:
  //   "/home/chronos/src/mojo/bazel-bin" ->
  //     ("/home/chronos/.cache/bazel/_bazel_chronos/"
  //      "af45689a65e49d32fd4a80b96a5abdde/execroot/"
  //      "mojo/bazel-out/local-fastbuild/bin")
  //
  // correct result = "../../../../../../../../../../src/mojo"

  // Use out to hold the relative path built so far.
  // USe tmp to hold the absolute path that out represents.

  // Start with the root, then peel it off piece by piece:
  //
  // xroot = ["/" "home" "chronos" "src" "mojo" "bazel-bin" "path"]
  // xpath = ["/" "home" "chronos" "src" "mojo"]
  //
  // tmp = ["/" "home" "chronos" "src" "mojo" "bazel-bin" "path"]
  // out = []
  //
  // tmp = ["/" "home" "chronos" "src" "mojo" "bazel-bin"]
  // out = [".."]
  //
  // tmp = ["/" "home" "chronos" ".cache" "bazel" "_bazel_chronos"
  //        "af45..." "execroot" "mojo" "bazel-out" "local-fastbuild"]
  // out = [".." ".."]
  //
  // ...
  //
  // tmp = ["/" "home" "chronos" ".cache"]
  // out = [".." ".." ".." ".." ".." ".." ".." ".." ".."]
  //
  // tmp = ["/" "home" "chronos"]
  // out = [".." ".." ".." ".." ".." ".." ".." ".." ".." ".."]
  //
  // Terminate the loop because |common_prefix(xpath, tmp) == tmp.size()|.
  //
  // xpath = ["/" "home" "chronos" "src" "mojo"]
  //   tmp = ["/" "home" "chronos"]
  //   out = [".." ".." ".." ".." ".." ".." ".." ".." ".." ".."]
  //   len = 3
  //
  // Then add the remaining pieces of xpath:
  //
  //   tmp = ["/" "home" "chronos" "src" "mojo"]
  //   out = [".." ".." ".." ".." ".." ".." ".." ".." ".." ".." "src" "mojo"]
  //
  // Join that, assign to *path, and we are done.

  tmp = xroot;
  while (len != tmp.size()) {
    out.push_back("..");
    tmp.push_back("..");
    auto r = canonicalize_exploded(&tmp);
    if (!r) return r;
    len = common_prefix(xpath, tmp);
  }
  out.insert(out.end(), xpath.begin() + len, xpath.end());

done:
  *path = join(out);
  return base::Result();
}

}  // namespace path
