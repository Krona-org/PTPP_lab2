#include "Menu.hpp"
#include "OpenDialogFile.hpp"
#include "checkuserdata/checkuserdata.h"
#include "scanfile/scanfile.h"

#include <filesystem>
#include <iostream>
#include <windows.h>

int main() {
  SetConsoleCP(65001);
  SetConsoleOutputCP(65001);

  std::filesystem::path currentMarkdownPath;
  std::filesystem::path currentUserDataPath;

  menu::ConsoleMenu myMenu;
  myMenu.setTitle("=== Лабораторная работа: Регулярные выражения в C++ ===");

  myMenu.setOptions({
      "1. Выбрать Markdown-файл и проверить синтаксис (п. 1)",
      "2. Выполнить поиск и анализ элементов Markdown (п. 2: заголовки, абзацы, списки)",
      "3. Выбрать файл для проверки пользовательских данных (п. 3)",
      "4. Ввести данные анкеты вручную для проверки и нормализации (п. 3.a, 3.b)",
      "5. Сгенерировать случайные данные анкет по словарям и нормализовать (п. 3.b, 3.c)",
      "0. Выйти"
  });

  // Пункт 1: Выбор файла и проверка синтаксиса
  myMenu.bindAction(1, [&currentMarkdownPath]() {
    std::cout << "\nОткрытие диалогового окна выбора Markdown-файла...\n";
    currentMarkdownPath = fileDialog::OpenFileDialog();
    if (currentMarkdownPath.empty()) {
      std::cout << "Файл не был выбран.\n";
      return;
    }
    std::cout << "Выбран файл: " << currentMarkdownPath.filename().u8string() << "\n";
    std::cout << "Проверка синтаксиса Markdown...\n";

    if (scanfile::Scanner::checkValidation(currentMarkdownPath)) {
      std::cout << "[УСПЕХ]: Файл полностью соответствует синтаксису Markdown.\n";
    } else {
      std::cout << "[ОШИБКА]: Файл НЕ соответствует синтаксису Markdown.\n";
    }
  });

  // Пункт 2: Поиск внутри текста заголовков, абзацев, списков и сохранение в STL
  myMenu.bindAction(2, [&currentMarkdownPath]() {
    if (currentMarkdownPath.empty()) {
      std::cout << "Файл не выбран! Сначала выберите файл через пункт 1.\n";
      return;
    }

    auto stats = scanfile::Scanner::analyzeMarkdown(currentMarkdownPath);
    std::cout << "\n=== Результаты анализа файла: "
              << currentMarkdownPath.filename().u8string() << " ===\n";
    std::cout << "Статистика:\n";
    std::cout << "  - Заголовков: " << stats.headersCount << "\n";
    std::cout << "  - Абзацев:    " << stats.paragraphsCount << "\n";
    std::cout << "  - Списков:    " << stats.listsCount
              << " (пунктов в списках: " << stats.listItemsCount << ")\n\n";

    // Вывод найденных элементов из STL-контейнеров
    std::cout << "--- Найденные заголовки (STL std::vector) ---\n";
    if (stats.headers.empty()) {
      std::cout << "  (заголовки не найдены)\n";
    } else {
      for (size_t i = 0; i < stats.headers.size(); ++i) {
        std::cout << "  [" << (i + 1) << "] " << stats.headers[i] << "\n";
      }
    }

    std::cout << "\n--- Найденные списки (STL std::vector) ---\n";
    if (stats.lists.empty()) {
      std::cout << "  (списки не найдены)\n";
    } else {
      for (size_t i = 0; i < stats.lists.size(); ++i) {
        std::cout << "  Список #" << (i + 1) << " (" << stats.lists[i].size() << " пунктов):\n";
        for (const auto &item : stats.lists[i]) {
          std::cout << "    " << item << "\n";
        }
      }
    }

    std::cout << "\n--- Найденные абзацы (STL std::vector) ---\n";
    if (stats.paragraphs.empty()) {
      std::cout << "  (абзацы не найдены)\n";
    } else {
      for (size_t i = 0; i < stats.paragraphs.size(); ++i) {
        std::cout << "  [" << (i + 1) << "] " << stats.paragraphs[i] << "\n";
      }
    }
  });

  // Пункт 3: Выбрать файл для проверки пользовательских данных
  myMenu.bindAction(3, [&currentUserDataPath]() {
    std::cout << "\nОткрытие диалогового окна выбора файла анкет...\n";
    currentUserDataPath = fileDialog::OpenFileDialog();
    if (currentUserDataPath.empty()) {
      std::cout << "Файл не был выбран.\n";
      return;
    }
    userdata::UserDataProcessor::processFile(currentUserDataPath);
  });

  // Пункт 4: Ввести данные анкеты вручную для проверки и нормализации
  myMenu.bindAction(4, []() {
    userdata::UserData input = userdata::UserDataProcessor::inputUserData();
    userdata::UserData normalized;
    std::vector<std::string> errors;

    std::cout << "\nВходные данные:  " << input.toString() << "\n";

    if (userdata::UserDataProcessor::normalizeAndValidate(input, normalized, errors)) {
      std::cout << "Выходные данные: " << normalized.toString() << "\n";
      std::cout << "[УСПЕХ]: Данные успешно нормализованы в соответствии с шаблоном.\n";
    } else {
      std::cout << "[ОШИБКА]: Данные не прошли проверку валидации:\n";
      for (const auto &err : errors) {
        std::cout << "  * " << err << "\n";
      }
    }
  });

  // Пункт 5: Сгенерировать случайные данные анкет
  myMenu.bindAction(5, []() {
    std::cout << "\nСколько случайных анкет сгенерировать? (по умолчанию 3): ";
    size_t count = 3;
    if (!(std::cin >> count) || count == 0) {
      std::cin.clear();
      std::cin.ignore(10000, '\n');
      count = 3;
    } else {
      std::cin.ignore(10000, '\n');
    }

    std::cout << "\n=== Генерация и нормализация случайных анкет (п. 3.b, 3.c) ===\n";
    auto dataset = userdata::UserDataProcessor::generateRandomDataset(count, true);

    for (size_t i = 0; i < dataset.size(); ++i) {
      std::cout << "\n--- Анкета #" << (i + 1) << " ---\n";
      std::cout << "Сгенерированные данные (с опечатками/вариациями): "
                << dataset[i].toString() << "\n";

      userdata::UserData normalized;
      std::vector<std::string> errors;
      if (userdata::UserDataProcessor::normalizeAndValidate(dataset[i], normalized, errors)) {
        std::cout << "Нормализованные данные (согласно шаблону):       "
                  << normalized.toString() << "\n";
        std::cout << "Статус: [УСПЕШНО]\n";
      } else {
        std::cout << "Статус: [ОШИБКИ ВАЛИДАЦИИ]\n";
        for (const auto &err : errors) {
          std::cout << "  * " << err << "\n";
        }
      }
    }
  });

  myMenu.showMenu();
  return 0;
}
