#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace scanfile {

/// @brief Результаты поиска и подсчета элементов разметки Markdown
struct MarkdownStats {
  size_t headersCount{0};     ///< Количество заголовков
  size_t paragraphsCount{0};  ///< Количество абзацев
  size_t listsCount{0};       ///< Количество списков (блоков)
  size_t listItemsCount{0};   ///< Общее количество пунктов в списках

  // Контейнеры библиотеки STL для хранения элементов согласно п. 2 ТЗ.
  // Для очень больших файлов хранится только начало (см. kMaxStored в scanfile.cpp),
  // точное количество — в счётчиках выше.
  std::vector<std::string> headers;             ///< Найденные заголовки
  std::vector<std::string> paragraphs;          ///< Найденные абзацы
  std::vector<std::vector<std::string>> lists;  ///< Найденные списки с их пунктами
};

/// @brief Валидация и анализ Markdown-файлов любого размера (чтение через Win32 Memory-Mapped File)
class Scanner {
public:
  /// @brief Проверка файла на соответствие синтаксису Markdown с выводом сообщений об ошибках в консоль
  static bool checkValidation(const std::filesystem::path &path);

  /// @brief Анализ содержимого файла: поиск и извлечение заголовков, абзацев и списков в STL-контейнеры
  static MarkdownStats analyzeMarkdown(const std::filesystem::path &path);
};

} // namespace scanfile