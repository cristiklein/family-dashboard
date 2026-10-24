#include <cstdio>

import dashboard.greeting;

int main() {
  return std::puts(dashboard::greeting()) < 0 ? 1 : 0;
}
