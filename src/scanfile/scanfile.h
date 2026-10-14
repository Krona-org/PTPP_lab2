#pragma once

#include <cstddef>
#include <filesystem>
#include <regex>
#include <string>
#include <vector>

namespace scanfile {

using WinHandle = void *;

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
  Scanner();
  explicit Scanner(const std::filesystem::path &path);
  ~Scanner();

  Scanner(const Scanner &) = delete;
  Scanner &operator=(const Scanner &) = delete;

  Scanner(Scanner &&other) noexcept;
  Scanner &operator=(Scanner &&other) noexcept;

  /// @brief Загрузка файла (с поддержкой Win32 Memory-Mapped Files для файлов >= 4 ГБ)
  bool loadFile(const std::filesystem::path &path);

  /// @brief Очистка ресурсов отображения памяти
  void cleanupMapping();

  /// @brief Возвращает строковое представление содержимого (MMF или RAM буфер)
  std::string_view getBufferView() const { return buffer_view_; }

  /// @brief Размер файла в байтах
  uintmax_t getFileSize() const { return fileSize_; }

  /// @brief Признак использования Memory-Mapped File
  bool isMemoryMapped() const { return isMapped_; }

  /// @brief Проверка файла на соответствие синтаксису Markdown с выводом сообщений об ошибках в консоль
  static bool checkValidation(const std::filesystem::path &path);

  /// @brief Анализ содержимого файла: поиск и извлечение заголовков, абзацев и списков в STL-контейнеры
  static MarkdownStats analyzeMarkdown(const std::filesystem::path &path);

private:
  // Порог 4 ГБ для переключения на Win32 Memory-Mapped File (лучшая практика из py_war)
  static constexpr size_t MMF_THRESHOLD = 4ULL * 1024 * 1024 * 1024;

  WinHandle fileHandle_{nullptr};
  WinHandle mappingHandle_{nullptr};
  const char *mappedData_{nullptr};
  bool isMapped_{false};

  // Буфер для файлов < 4 ГБ
  std::string buffer_;
  // View на данные (виртуальная память MMF или буфер RAM)
  std::string_view buffer_view_;
  uintmax_t fileSize_{0};
};

} // namespace scanfile