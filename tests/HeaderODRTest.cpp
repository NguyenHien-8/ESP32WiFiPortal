#include <cassert>
#include <iostream>

bool headerODRPartA();
bool headerODRPartB();

int main() {
  assert(headerODRPartA());
  assert(headerODRPartB());
  std::cout << "Multiple-translation-unit header test passed\n";
  return 0;
}
