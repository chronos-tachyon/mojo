#include <cstdio>

#include "base/logging.h"
#include "base/result.h"
#include "event/manager.h"
#include "net/net.h"

int main(int argc, char** argv) {
  if (argc != 3) {
    fprintf(stderr, "Usage: %s <protocol> <name>\n", argv[0]);
    return 1;
  }

  event::ManagerOptions mo;
  mo.set_async_mode();
  event::Manager m;
  CHECK_OK(event::new_manager(&m, mo));
  event::set_system_manager(std::move(m));

  std::vector<net::Addr> out;
  CHECK_OK(resolve(&out, argv[1], argv[2]));

  fprintf(stdout, "%zd result(s)\n", out.size());
  for (const auto& addr : out) {
    fprintf(stdout, "\t%s\t%s\n", addr.protocol().c_str(),
            addr.address().c_str());
  }
  return 0;
}
