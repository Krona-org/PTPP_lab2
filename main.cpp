#include "includes/OpenDialogFile.hpp"
#include "includes/menu.hpp"
#include "scanfile/scanfile.h"

#include <filesystem>
#include <iostream>
#include <windows.h>

int main() {
  SetConsoleCP(65001);
  SetConsoleOutputCP(65001);

  std::filesystem::path currentPath;

  menu::ConsoleMenu myMenu;
  myMenu.setTitle("--- Анализатор файлов ---");

  myMenu.setOptions(
      {"1. Выбрать файл и проверить синтаксис", "2. Выполнить анализ файла",
       "3. Выбрать файл для проверки пользовательских данных", "0. Выйти"});

  myMenu.bindAction(1, [&currentPath]() {
    currentPath = fileDialog::OpenFileDialog();
    if (!currentPath.empty()) {
      std::cout << "Выбран файл: " << currentPath.string() << "\n";
    } else {
      std::cout << "Файл не был выбран.\n";
    }
  });

  myMenu.bindAction(2, [&currentPath]() {
    if (currentPath.empty()) {
      std::cout << "Ошибка: сначала выберите файл через пункт 1!\n";
      return;
    }
    bool isValid = scanfile::Scanner::checkValidation(currentPath);
    if (isValid) {
      std::cout << "Результат: файл соответствует синтаксису Markdown!\n";
    } else {
      std::cout << "Результат: файл НЕ соответствует синтаксису Markdown.\n";
    }
  });

  myMenu.showMenu();
  return 0;
}
