// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include <cstdio>
#include <iomanip>
#include <iostream>
#include <limits>

#include "base/logging.h"
#include "base/result.h"
#include "base/time/breakdown.h"
#include "base/time/time.h"
#include "base/time/zone.h"

using namespace base::time;
using Mode = zone::Recurrence::Mode;

static std::string format_time(Time time) {
  if (time == Time::min()) {
    return "infinite past";
  } else if (time == Time::max()) {
    return "infinite future";
  } else {
    Breakdown u;
    u.set(time);
    return u.iso8601();
  }
}

static void print_type(const zone::Type* type) {
  const char* dst_or_st = (type->is_dst() ? " (daylight)" : "");
  std::cout << "\t" << type->abbreviation() << dst_or_st << "\n";
  if (type->is_specified()) {
    std::cout << "\tUTC" << zone::format_offset(type->utc_offset(), false)
              << "\n";
  } else {
    std::cout << "\tOffset not specified\n";
  }
  std::cout << std::endl;
}

static void print_recurrence(zone::Recurrence r) {
  switch (r.mode()) {
    case Mode::never:
      std::cout << "\t\tMode: never\n";
      break;

    case Mode::always:
      std::cout << "\t\tMode: always\n";
      break;

    case Mode::julian0:
      std::cout << "\t\tMode: julian0 \"" << r.day() << "\"\n";
      break;

    case Mode::julian1:
      std::cout << "\t\tMode: julian1 \"J" << r.day() << "\"\n";
      break;

    case Mode::month_week_wday:
      std::cout << "\t\tMode: month_week_wday \"W" << r.month() << "."
                << r.week() << "." << r.day() << "\"\n";
      break;

    default:
      std::cout << "\t\tMode: UNKNOWN!!!\n";
  }
  std::cout << "\t\tPlus: " << r.seconds_past_midnight() << " seconds\n";
}

static void print_regime(const zone::Regime* regime) {
  auto t0 = regime->regime_begin();
  auto t1 = regime->regime_end();

  std::cout << "\tStarts: " << t0 << " (" << format_time(t0) << ")\n"
            << "\tEnds  : " << t1 << " (" << format_time(t1) << ")\n"
            << "\t[Recurrence: 0 \"" << regime->daylight_time()->abbreviation()
            << "\"]\n";
  print_recurrence(regime->dst_begin());
  std::cout << "\t[Recurrence: 1 \"" << regime->standard_time()->abbreviation()
            << "\"]\n";
  print_recurrence(regime->dst_end());
  std::cout << std::endl;
}

static void print_leap(const zone::LeapSecond* leap) {
  auto at = leap->time();
  std::cout << "\tAt   : " << at << " (" << format_time(at) << ")\n"
            << "\tDelta: " << leap->delta() << "\n"
            << std::endl;
}

int main(int argc, char** argv) {
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " <timezone-name>" << std::endl;
    return 1;
  }

  zone::Pointer zone;
  CHECK_OK(zone::system_database()->get(&zone, argv[1]));
  std::cout << zone->name() << "\n"
            << "\n";
  const auto& types = zone->types();
  for (std::size_t i = 0; i < types.size(); ++i) {
    std::cout << "[Type: " << i << "]\n";
    print_type(&types[i]);
  }
  const auto& regimes = zone->regimes();
  for (std::size_t i = 0; i < regimes.size(); ++i) {
    std::cout << "[Regime: " << i << "]\n";
    print_regime(&regimes[i]);
  }
  const auto& leaps = zone->leap_seconds();
  for (std::size_t i = 0; i < leaps.size(); ++i) {
    std::cout << "[Leap: " << i << "]\n";
    print_leap(&leaps[i]);
  }

  return 0;
}
