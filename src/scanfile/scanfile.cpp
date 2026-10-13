#include "scanfile.h"

#include <fstream>
#include <iostream>

namespace scanfile {

/// @brief Метод проверки на соответствие синтаксису Markdown
/// @param path Путь к проверяемому файлу
/// @return true, если файл соответствует синтаксису Markdown, иначе false
bool Scanner::checkValidation(const std::filesystem::path &path) {
  if (path.empty()) {
    std::cout << "[Ошибка валидации]: Путь к файлу не указан!\n";
    return false;
  }

  if (!std::filesystem::exists(path)) {
    std::cout << "[Ошибка валидации]: Файл не существует: " << path.string() << "\n";
    return false;
  }

  if (path.extension() != ".md" && path.extension() != ".markdown") {
    std::cout << "[Ошибка валидации]: Неверное расширение файла (" 
              << path.extension().string() 
              << "). Ожидается .md или .markdown!\n";
    return false;
  }

  std::ifstream file(path);
  if (!file.is_open()) {
    std::cout << "[Ошибка валидации]: Не удалось открыть файл для чтения: " 
              << path.string() << "\n";
    return false;
  }

  static const std::regex rawCodeRegex(R"(\w+\(.*\)|;\s*$)");
  static const std::regex foreignHtmlRegex(R"(<\/?(html|head|body|div|p|script|style|table|tr|td|span|h[1-6])\b[^>]*>)", std::regex::icase);

  bool inCodeBlock = false;
  bool hasAnyMarkdown = false;
  std::string line;
  size_t lineNumber = 0;

  while (std::getline(file, line)) {
    lineNumber++;
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }

    // Проверка на блок кода ```
    if (std::regex_match(line, MarkdownRegex::codeBlock)) {
      inCodeBlock = !inCodeBlock;
      hasAnyMarkdown = true;
      continue;
    }

    if (inCodeBlock) {
      continue;
    }

    if (std::regex_match(line, MarkdownRegex::emptyLine)) {
      continue;
    }

    // Проверка на чужую разметку (HTML-документ или теги)
    if (std::regex_search(line, foreignHtmlRegex)) {
      std::cout << "[Ошибка валидации] Строка " << lineNumber 
                << ": обнаружен тег чужого языка разметки HTML: \"" << line << "\"\n";
      return false;
    }

    // Проверка на чужую разметку (LaTeX)
    if (std::regex_search(line, MarkdownRegex::latexCommand)) {
      std::cout << "[Ошибка валидации] Строка " << lineNumber 
                << ": обнаружены команды чужого языка разметки LaTeX: \"" << line << "\"\n";
      return false;
    }

    // Проверка стандартных элементов Markdown
    if (std::regex_match(line, MarkdownRegex::header) ||
        std::regex_match(line, MarkdownRegex::bulletList) ||
        std::regex_match(line, MarkdownRegex::numberList) ||
        std::regex_search(line, MarkdownRegex::inlineLink)) {
      hasAnyMarkdown = true;
      continue;
    }

    // Сырой исходный код вне блока ```
    if (std::regex_search(line, rawCodeRegex)) {
      std::cout << "[Ошибка валидации] Строка " << lineNumber 
                << ": обнаружен программный код вне блока ```: \"" << line << "\"\n";
      return false;
    }
  }

  if (inCodeBlock) {
    std::cout << "[Ошибка валидации]: Блок кода (```) не был закрыт до конца файла!\n";
    return false;
  }

  if (!hasAnyMarkdown) {
    std::cout << "[Ошибка валидации]: Файл не содержит ни одного элемента разметки Markdown (заголовки, списки, ссылки, блоки кода)!\n";
    return false;
  }

  return true;
}

/// @brief Анализ содержимого файла: поиск и извлечение заголовков, абзацев и списков
/// @param path Путь к Markdown-файлу
/// @return Структура со статистикой и найденными элементами MarkdownStats
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
  bool inList = false;
  std::string currentParagraph;
  std::string line;

  auto flushParagraph = [&]() {
    if (!currentParagraph.empty()) {
      stats.paragraphs.push_back(currentParagraph);
      stats.paragraphsCount++;
      currentParagraph.clear();
    }
  };

  while (std::getline(file, line)) {
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }

    // Блок кода ```
    if (std::regex_match(line, MarkdownRegex::codeBlock)) {
      flushParagraph();
      inCodeBlock = !inCodeBlock;
      inList = false;
      continue;
    }

    if (inCodeBlock) {
      continue;
    }

    // Пустая строка завершает текущий абзац и список
    if (std::regex_match(line, MarkdownRegex::emptyLine)) {
      flushParagraph();
      inList = false;
      continue;
    }

    // Заголовок (#{1,6})
    if (std::regex_match(line, MarkdownRegex::header)) {
      flushParagraph();
      stats.headers.push_back(line);
      stats.headersCount++;
      inList = false;
      continue;
    }

    // Элемент списка (маркированный или нумерованный)
    if (std::regex_match(line, MarkdownRegex::bulletList) ||
        std::regex_match(line, MarkdownRegex::numberList)) {
      flushParagraph();
      if (!inList) {
        stats.lists.push_back({}); // Начался новый список
        stats.listsCount++;
        inList = true;
      }
      stats.lists.back().push_back(line);
      stats.listItemsCount++;
      continue;
    }

    // Обычный текст - накапливается как абзац
    inList = false;
    if (!currentParagraph.empty()) {
      currentParagraph += " ";
    }
    currentParagraph += line;
  }

  flushParagraph();

  return stats;
}

} // namespace scanfile
