#pragma once

#include <cstddef>
#include <filesystem>
#include <regex>

namespace scanfile {

/// @brief Шаблоны регулярных выражений
struct MarkdownRegex {
  static inline const std::regex header{R"(^(#{1,6})\s+(.*))"};
  static inline const std::regex bulletList{R"(^\s*[-*+]\s+(.*))"};
  static inline const std::regex numberList{R"(^\s*(\d+)\.\s+(.*))"};
  static inline const std::regex codeBlock{R"(^```(\w*))"};
  static inline const std::regex inlineLink{R"(\[([^\]]+)\]\(([^)]+)\))"};
  static inline const std::regex emptyLine{R"(^\s*$)"};
};

/// @brief Результаты подсчета элементов разметки Markdown
struct MarkdownStats {
  size_t headersCount{0};     ///< Количество заголовков
  size_t paragraphsCount{0};  ///< Количество абзацев
  size_t listsCount{0};       ///< Количество списков (блоков)
  size_t listItemsCount{0};   ///< Общее количество пунктов в списках
};

class Scanner {
public:
  Scanner() = default;
  ~Scanner() = default;

  /// @brief Проверка файла на соответствие синтаксису Markdown
  static bool checkValidation(const std::filesystem::path &path);

  /// @brief Анализ содержимого файла: подсчет заголовков, абзацев и списков
  static MarkdownStats analyzeMarkdown(const std::filesystem::path &path);

private:
};

} // namespace scanfile