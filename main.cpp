#include "includes/OpenDialogFile.hpp"
#include "scanfile/scanfile.h"

#include <iostream>
#include <windows.h>

int main() {
  SetConsoleCP(65001);
  SetConsoleOutputCP(65001);

  std::cout << std::boolalpha
            << scanfile::Scanner::checkValidation(fileDialog::OpenFileDialog())
            << std::endl;

  return 0;
}