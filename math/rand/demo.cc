// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include <cstdlib>
#include <iomanip>
#include <iostream>

#include "math/rand/rand.h"

int main(int argc, char** argv) {
  std::cout << std::hex << std::setfill('0');

  auto src = math::rand::new_lcg_source();
  std::cout << "[Linear Congruential Generator]" << "\n";
  for (std::size_t i = 0; i < 100; i++) {
    std::cout << std::setw(16) << src->next() << "\n";
  }
  std::cout << std::endl;

  src = math::rand::new_lfsr_source();
  std::cout << "[Linear Feedback Shift Register]" << "\n";
  for (std::size_t i = 0; i < 100; i++) {
    std::cout << std::setw(16) << src->next() << "\n";
  }
  std::cout << std::endl;

  src = math::rand::new_mt_source();
  std::cout << "[Mersenne Twister]" << "\n";
  for (std::size_t i = 0; i < 100; i++) {
    std::cout << std::setw(16) << src->next() << "\n";
  }
  std::cout << std::endl;

  src = math::rand::new_xorshift_source();
  std::cout << "[xorshift*]" << "\n";
  for (std::size_t i = 0; i < 100; i++) {
    std::cout << std::setw(16) << src->next() << "\n";
  }
  std::cout << std::endl;

  return 0;
}
