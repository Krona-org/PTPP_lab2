#include "scanfile.h"

#include <fstream>

namespace scanfile {

/// @brief Метод проверки на сооответствие синтаксису Markdown
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

} // namespace scanfile
