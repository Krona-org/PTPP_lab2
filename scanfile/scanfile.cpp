#include "scanfile.h"

#include <fstream>

namespace scanfile {

/// @brief Метод проверки на соответствие синтаксису Markdown
/// @param path Путь к проверяемому файлу
/// @return true, если файл соответствует синтаксису Markdown, иначе false
bool Scanner::checkValidation(const std::filesystem::path &path) {
  if (path.empty() ||
      (path.extension() != ".md" && path.extension() != ".markdown"))
    return false;

  std::ifstream file(path);
  if (!file.is_open())
    return false;

  static const std::regex rawCodeRegex(R"(\w+\(.*\)|;\s*$)");

  bool inCodeBlock = false;
  bool hasAnyMarkdown = false;
  std::string line;

  while (std::getline(file, line)) {
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }

    if (std::regex_match(line, MarkdownRegex::codeBlock)) {
      inCodeBlock = !inCodeBlock;
      hasAnyMarkdown = true;
      continue;
    }

    if (inCodeBlock)
      continue;

    if (std::regex_match(line, MarkdownRegex::emptyLine))
      continue;

    if (std::regex_match(line, MarkdownRegex::header) ||
        std::regex_match(line, MarkdownRegex::bulletList) ||
        std::regex_match(line, MarkdownRegex::numberList)) {
      hasAnyMarkdown = true;
      continue;
    }

    if (std::regex_search(line, rawCodeRegex))
      return false;

    if (inCodeBlock)
      return false;
  }
  return hasAnyMarkdown;
}

/// @brief Анализ содержимого файла: подсчет заголовков, абзацев и списков
/// @param path Путь к Markdown-файлу
/// @return Структура со статистикой MarkdownStats
MarkdownStats Scanner::analyzeMarkdown(const std::filesystem::path &path) {
  MarkdownStats stats;

  if (path.empty()) {
    return stats;
  }

  std::ifstream file(path);
  if (!file.is_open()) {
    return stats;
  }

  bool inCodeBlock = false;
  bool inParagraph = false;
  bool inList = false;
  std::string line;

  while (std::getline(file, line)) {
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }

    // 1. Блок кода ```
    if (std::regex_match(line, MarkdownRegex::codeBlock)) {
      inCodeBlock = !inCodeBlock;
      inParagraph = false;
      inList = false;
      continue;
    }

    if (inCodeBlock) {
      continue;
    }

    // 2. Пустая строка завершает текущий абзац и список
    if (std::regex_match(line, MarkdownRegex::emptyLine)) {
      inParagraph = false;
      inList = false;
      continue;
    }

    // 3. Заголовок (#{1,6})
    if (std::regex_match(line, MarkdownRegex::header)) {
      stats.headersCount++;
      inParagraph = false;
      inList = false;
      continue;
    }

    // 4. Элемент списка (маркированный или нумерованный)
    if (std::regex_match(line, MarkdownRegex::bulletList) ||
        std::regex_match(line, MarkdownRegex::numberList)) {
      if (!inList) {
        stats.listsCount++; // Начался новый список
        inList = true;
      }
      stats.listItemsCount++; // Пункт списка
      inParagraph = false;
      continue;
    }

    // 5. Обычный текст (абзац)
    // Несколько строк подряд без пустых строк считаются одним абзацем
    if (!inParagraph) {
      stats.paragraphsCount++;
      inParagraph = true;
    }
    inList = false;
  }

  return stats;
}

} // namespace scanfile
