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

  // Пункт 1: Выбор файла и проверка синтаксиса
  myMenu.bindAction(1, [&currentPath]() {
    currentPath = fileDialog::OpenFileDialog();
    if (currentPath.empty()) {
      std::cout << "Файл не был выбран.\n";
      return;
    }
    std::cout << "Выбран файл: " << currentPath.filename().string() << "\n";
    if (scanfile::Scanner::checkValidation(currentPath)) {
      std::cout << "Файл соответствует разметке Markdown.\n";
    } else {
      std::cout << "Файл НЕ соответствует разметке Markdown.\n";
    }
  });

  // Пункт 2: Подсчет заголовков, абзацев и списков
  myMenu.bindAction(2, [&currentPath]() {
    if (currentPath.empty()) {
      std::cout << "Файл не выбран! Сначала выберите файл через пункт 1.\n";
      return;
    }
    auto stats = scanfile::Scanner::analyzeMarkdown(currentPath);
    std::cout << "Результаты анализа файла: " << currentPath.filename().string() << "\n";
    std::cout << "  - Заголовков: " << stats.headersCount << "\n";
    std::cout << "  - Абзацев:    " << stats.paragraphsCount << "\n";
    std::cout << "  - Списков:    " << stats.listsCount
              << " (пунктов в списках: " << stats.listItemsCount << ")\n";
  });

  myMenu.showMenu();
  return 0;
}
