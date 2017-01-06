#include <cstdio>
#include <cstring>
#include <cerrno>
#include <cstdlib>
#include <cstdint>

#include "base/logging.h"
#include "base/user.h"

static base::Result parse_id(int32_t* out, const char* str) {
  CHECK_NOTNULL(out);
  CHECK_NOTNULL(str);
  *out = -1;
  char* endptr = const_cast<char*>(str);
  errno = 0;
  long val = strtol(str, &endptr, 10);
  int err_no = errno;
  if (err_no == 0 && *endptr != '\0') err_no = EINVAL;
  if (err_no == 0 && (val < INT32_MIN || val > INT32_MAX)) err_no = ERANGE;
  if (err_no != 0) return base::Result::from_errno(err_no, "strtol(3)");
  *out = val;
  return base::Result();
}

static void print_usage(const char* argv0) {
  fprintf(stderr,
          "Usage:\n"
          "  %1$s user <name>\n"
          "  %1$s uid <id>\n"
          "  %1$s group <name>\n"
          "  %1$s gid <id>\n",
          argv0);
}

static void print_user(const base::User& user) {
  printf(
      "uid = %d\n"
      "gid = %d\n"
      "name = \"%s\"\n"
      "gecos = \"%s\"\n"
      "homedir = \"%s\"\n"
      "shell = \"%s\"\n",
      user.uid, user.gid, user.name.c_str(), user.gecos.c_str(),
      user.homedir.c_str(), user.shell.c_str());
}

static void print_group(const base::Group& group) {
  printf(
      "gid = %d\n"
      "name = \"%s\"\n"
      "members = [\n",
      group.gid, group.name.c_str());
  for (const auto& member : group.members) {
    printf("  \"%s\",\n", member.c_str());
  }
  printf("]\n");
}

static void print_result(base::Result result) {
  fprintf(stderr, "%s\n", result.as_string().c_str());
}

int main(int argc, char** argv) {
  if (argc != 3) {
    print_usage(argv[0]);
    return 2;
  }
  if (strcmp(argv[1], "user") == 0) {
    base::User user;
    base::Result r = base::user_by_name(&user, argv[2]);
    if (!r) {
      print_result(r);
      return 1;
    }
    print_user(user);
    return 0;
  }
  if (strcmp(argv[1], "uid") == 0) {
    int32_t id;
    base::Result r = parse_id(&id, argv[2]);
    if (!r) {
      print_result(r);
      return 1;
    }

    base::User user;
    r = base::user_by_id(&user, id);
    if (!r) {
      print_result(r);
      return 1;
    }
    print_user(user);
    return 0;
  }
  if (strcmp(argv[1], "group") == 0) {
    base::Group group;
    base::Result r = base::group_by_name(&group, argv[2]);
    if (!r) {
      print_result(r);
      return 1;
    }
    print_group(group);
    return 0;
  }
  if (strcmp(argv[1], "gid") == 0) {
    int32_t id;
    base::Result r = parse_id(&id, argv[2]);
    if (!r) {
      print_result(r);
      return 1;
    }

    base::Group group;
    r = base::group_by_id(&group, id);
    if (!r) {
      print_result(r);
      return 1;
    }
    print_group(group);
    return 0;
  }
  print_usage(argv[0]);
  return 2;
}
