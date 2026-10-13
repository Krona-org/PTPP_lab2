#pragma once

#include <cstddef>
#include <filesystem>
#include <regex>
#include <string>
#include <vector>

namespace scanfile {

/// @brief Шаблоны регулярных выражений
struct MarkdownRegex {
  static inline const std::regex header{R"(^(#{1,6})\s+(.*))"};
  static inline const std::regex bulletList{R"(^\s*[-*+]\s+(.*))"};
  static inline const std::regex numberList{R"(^\s*(\d+)\.\s+(.*))"};
  static inline const std::regex codeBlock{R"(^```(\w*))"};
  static inline const std::regex inlineLink{R"(\[([^\]]+)\]\(([^)]+)\))"};
  static inline const std::regex emptyLine{R"(^\s*$)"};

  // Регулярные выражения для распознавания элементов других языков разметки
  static inline const std::regex htmlTag{R"(<\/?([a-zA-Z][a-zA-Z0-9]*)\b[^>]*>)"};
  static inline const std::regex latexCommand{R"(\\(documentclass|begin|section|subsection|usepackage)\b)"};
};

/// @brief Результаты поиска и подсчета элементов разметки Markdown
struct MarkdownStats {
  size_t headersCount{0};     ///< Количество заголовков
  size_t paragraphsCount{0};  ///< Количество абзацев
  size_t listsCount{0};       ///< Количество списков (блоков)
  size_t listItemsCount{0};   ///< Общее количество пунктов в списках

  // Контейнеры библиотеки STL для хранения элементов согласно п. 2 ТЗ
  std::vector<std::string> headers;                     ///< Найденные заголовки
  std::vector<std::string> paragraphs;                  ///< Найденные абзацы
  std::vector<std::vector<std::string>> lists;          ///< Найденные списки с их пунктами
};

class Scanner {
public:
  Scanner() = default;
  ~Scanner() = default;

  /// @brief Проверка файла на соответствие синтаксису Markdown с выводом сообщений об ошибках в консоль
  static bool checkValidation(const std::filesystem::path &path);

  /// @brief Анализ содержимого файла: поиск и извлечение заголовков, абзацев и списков в STL-контейнеры
  static MarkdownStats analyzeMarkdown(const std::filesystem::path &path);

private:
};

} // namespace scanfile