#include <cstdio>
#include <print>

import dashboard.greeting;

int main() {
  std::println("{}", dashboard::greeting());
  return std::fflush(stdout) == 0 ? 0 : 1;
}
