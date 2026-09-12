#include <dgds/version.h>

#include <iostream>
#include <string_view>

int main(int argc, char *argv[]) {
  if (argc > 1 && std::string_view(argv[1]) == "--version") {
    std::cout << dgds::k_version << '\n';
    return 0;
  }

  std::cout << "Hello, DGDS!" << std::endl;
  return 0;
}
