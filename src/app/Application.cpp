#include "Application.h"
#include "OpenDialogFile.hpp"
#include "checkuserdata/checkuserdata.h"
#include "scanfile/scanfile.h"

#include <iostream>
#include <limits>

namespace app {

Application::Application() {
  setupMenu();
}

void Application::run() {
  menu_.showMenu();
}

void Application::setupMenu() {
  menu_.setTitle("=== Лабораторная работа: Регулярные выражения в C++ ===");

  menu_.addItem(1, "Выбрать Markdown-файл и проверить синтаксис (п. 1)", [this]() {
    onSelectAndValidateMarkdown();
  });

  menu_.addItem(2, "Выполнить поиск и анализ элементов Markdown (п. 2: заголовки, абзацы, списки)", [this]() {
    onAnalyzeMarkdown();
  });

  menu_.addItem(3, "Выбрать файл для проверки пользовательских данных (п. 3)", [this]() {
    onSelectUserDataFile();
  });

  menu_.addItem(4, "Ввести данные анкеты вручную для проверки и нормализации (п. 3.a, 3.b)", [this]() {
    onManualUserDataInput();
  });

  menu_.addItem(5, "Сгенерировать случайные данные анкет по словарям и нормализовать (п. 3.b, 3.c)", [this]() {
    onGenerateRandomUserData();
  });

  menu_.addItem(0, "Выйти", nullptr);
}

void Application::onSelectAndValidateMarkdown() {
  std::cout << "\nОткрытие диалогового окна выбора Markdown-файла...\n";
  currentMarkdownPath_ = fileDialog::OpenFileDialog();
  if (currentMarkdownPath_.empty()) {
    std::cout << "Файл не был выбран.\n";
    return;
  }

  std::cout << "Выбран файл: " << currentMarkdownPath_.filename().u8string() << "\n";
  std::cout << "Проверка синтаксиса Markdown...\n";

  if (scanfile::Scanner::checkValidation(currentMarkdownPath_)) {
    std::cout << "[УСПЕХ]: Файл полностью соответствует синтаксису Markdown.\n";
  } else {
    std::cout << "[ОШИБКА]: Файл НЕ соответствует синтаксису Markdown.\n";
  }
}

void Application::onAnalyzeMarkdown() {
  if (currentMarkdownPath_.empty()) {
    std::cout << "Файл не выбран! Сначала выберите файл через пункт 1.\n";
    return;
  }

  auto stats = scanfile::Scanner::analyzeMarkdown(currentMarkdownPath_);
  printMarkdownStats(currentMarkdownPath_, stats);
}

void Application::onSelectUserDataFile() {
  std::cout << "\nОткрытие диалогового окна выбора файла анкет...\n";
  currentUserDataPath_ = fileDialog::OpenFileDialog();
  if (currentUserDataPath_.empty()) {
    std::cout << "Файл не был выбран.\n";
    return;
  }
  userdata::UserDataProcessor::processFile(currentUserDataPath_);
}

void Application::onManualUserDataInput() {
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
}

void Application::onGenerateRandomUserData() {
  size_t count = promptCount("\nСколько случайных анкет сгенерировать? (по умолчанию 3): ", 3);

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
}

void Application::printMarkdownStats(const std::filesystem::path &filePath,
                                     const scanfile::MarkdownStats &stats) {
  std::cout << "\n=== Результаты анализа файла: "
            << filePath.filename().u8string() << " ===\n";
  std::cout << "Статистика:\n";
  std::cout << "  - Заголовков: " << stats.headersCount << "\n";
  std::cout << "  - Абзацев:    " << stats.paragraphsCount << "\n";
  std::cout << "  - Списков:    " << stats.listsCount
            << " (пунктов в списках: " << stats.listItemsCount << ")\n\n";

  // Вывод найденных элементов из STL-контейнеров (с ограничением вывода в консоль до 20 элементов для больших файлов)
  static constexpr size_t MAX_CONSOLE_PRINT = 20;

  std::cout << "--- Найденные заголовки (STL std::vector) ---\n";
  if (stats.headers.empty()) {
    std::cout << "  (заголовки не найдены)\n";
  } else {
    size_t count = std::min(stats.headers.size(), MAX_CONSOLE_PRINT);
    for (size_t i = 0; i < count; ++i) {
      std::cout << "  [" << (i + 1) << "] " << stats.headers[i] << "\n";
    }
    if (stats.headersCount > count) {
      std::cout << "  ... [показано " << count << " из " << stats.headersCount << " заголовков]\n";
    }
  }

  std::cout << "\n--- Найденные списки (STL std::vector) ---\n";
  if (stats.lists.empty()) {
    std::cout << "  (списки не найдены)\n";
  } else {
    size_t count = std::min(stats.lists.size(), MAX_CONSOLE_PRINT);
    for (size_t i = 0; i < count; ++i) {
      std::cout << "  Список #" << (i + 1) << " (" << stats.lists[i].size() << " пунктов):\n";
      size_t itemCount = std::min(stats.lists[i].size(), size_t(5));
      for (size_t j = 0; j < itemCount; ++j) {
        std::cout << "    " << stats.lists[i][j] << "\n";
      }
      if (stats.lists[i].size() > itemCount) {
        std::cout << "    ... [ещё " << (stats.lists[i].size() - itemCount) << " пунктов]\n";
      }
    }
    if (stats.listsCount > count) {
      std::cout << "  ... [показано " << count << " из " << stats.listsCount << " списков]\n";
    }
  }

  std::cout << "\n--- Найденные абзацы (STL std::vector) ---\n";
  if (stats.paragraphs.empty()) {
    std::cout << "  (абзацы не найдены)\n";
  } else {
    size_t count = std::min(stats.paragraphs.size(), MAX_CONSOLE_PRINT);
    for (size_t i = 0; i < count; ++i) {
      std::cout << "  [" << (i + 1) << "] " << stats.paragraphs[i] << "\n";
    }
    if (stats.paragraphsCount > count) {
      std::cout << "  ... [показано " << count << " из " << stats.paragraphsCount << " абзацев]\n";
    }
  }
}

size_t Application::promptCount(std::string_view prompt, size_t defaultValue) {
  std::cout << prompt;
  size_t count = defaultValue;
  if (!(std::cin >> count) || count == 0) {
    std::cin.clear();
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    return defaultValue;
  }
  std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
  return count;
}

} // namespace app
