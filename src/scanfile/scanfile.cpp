#include "scanfile.h"

#include <windows.h>

#include <iostream>
#include <regex>
#include <string_view>

namespace scanfile {
namespace {

/// @brief Шаблоны регулярных выражений
namespace re {
const std::regex header{R"(^(#{1,6})\s+(.*))"};
const std::regex bulletList{R"(^\s*[-*+]\s+(.*))"};
const std::regex numberList{R"(^\s*(\d+)\.\s+(.*))"};
const std::regex codeBlock{R"(^```(\w*))"};
const std::regex inlineLink{R"(\[([^\]]+)\]\(([^)]+)\))"};
// Элементы других языков разметки и программный код вне блока ```
const std::regex html{R"(<\/?(html|head|body|div|p|script|style|table|tr|td|span|h[1-6])\b[^>]*>)",
                      std::regex::icase};
const std::regex latex{R"(\\(documentclass|begin|section|subsection|usepackage)\b)"};
const std::regex rawCode{R"(\w+\(.*\)|;\s*$)"};
} // namespace re

/// @brief Read-only отображение файла в память (Win32 Memory-Mapped File).
/// В отличие от std::ifstream/std::getline работает с файлами любого размера (в т.ч. > 4 ГБ):
/// данные подгружаются ОС постранично, ОЗУ под весь файл не требуется.
class MappedFile {
public:
  explicit MappedFile(const std::filesystem::path &path) {
    file_ = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    LARGE_INTEGER size{};
    if (file_ == INVALID_HANDLE_VALUE || !GetFileSizeEx(file_, &size)) {
      return;
    }
    if (size.QuadPart == 0) { // пустой файл отобразить нельзя, но это не ошибка
      ok_ = true;
      return;
    }
    mapping_ = CreateFileMappingW(file_, nullptr, PAGE_READONLY, 0, 0, nullptr);
    if (!mapping_) {
      return;
    }
    if (auto *data = static_cast<const char *>(MapViewOfFile(mapping_, FILE_MAP_READ, 0, 0, 0))) {
      view_ = {data, static_cast<size_t>(size.QuadPart)};
      ok_ = true;
    }
  }

  ~MappedFile() {
    if (!view_.empty()) UnmapViewOfFile(view_.data());
    if (mapping_) CloseHandle(mapping_);
    if (file_ != INVALID_HANDLE_VALUE) CloseHandle(file_);
  }

  MappedFile(const MappedFile &) = delete;
  MappedFile &operator=(const MappedFile &) = delete;

  bool ok() const { return ok_; }
  std::string_view data() const { return view_; }

private:
  HANDLE file_{INVALID_HANDLE_VALUE};
  HANDLE mapping_{nullptr};
  std::string_view view_;
  bool ok_{false};
};

/// @brief Извлекает очередную строку из text без копирования (поддерживает \n и \r\n)
bool nextLine(std::string_view &text, std::string_view &line) {
  if (text.empty()) {
    return false;
  }
  size_t eol = text.find('\n');
  if (eol == std::string_view::npos) {
    eol = text.size();
  }
  line = text.substr(0, eol);
  text.remove_prefix(eol < text.size() ? eol + 1 : eol);
  if (!line.empty() && line.back() == '\r') {
    line.remove_suffix(1);
  }
  return true;
}

bool matches(std::string_view s, const std::regex &r) {
  return std::regex_match(s.data(), s.data() + s.size(), r);
}

bool contains(std::string_view s, const std::regex &r) {
  return std::regex_search(s.data(), s.data() + s.size(), r);
}

// Классификаторы строк. Перед регуляркой — дешёвая проверка символа,
// чтобы не запускать std::regex на каждой из сотен миллионов строк.

bool isBlank(std::string_view s) { return s.find_first_not_of(" \t\r\v\f") == std::string_view::npos; }

bool isCodeFence(std::string_view s) { return s.substr(0, 3) == "```" && matches(s, re::codeBlock); }

bool isHeader(std::string_view s) { return !s.empty() && s[0] == '#' && matches(s, re::header); }

bool isListItem(std::string_view s) {
  size_t i = s.find_first_not_of(" \t");
  if (i == std::string_view::npos) {
    return false;
  }
  char c = s[i];
  if (c == '-' || c == '*' || c == '+') return matches(s, re::bulletList);
  if (c >= '0' && c <= '9') return matches(s, re::numberList);
  return false;
}

bool hasLink(std::string_view s) { return s.find('[') != std::string_view::npos && contains(s, re::inlineLink); }

bool hasHtml(std::string_view s) { return s.find('<') != std::string_view::npos && contains(s, re::html); }

bool hasLatex(std::string_view s) { return s.find('\\') != std::string_view::npos && contains(s, re::latex); }

bool hasRawCode(std::string_view s) {
  return s.find_first_of("(;") != std::string_view::npos && contains(s, re::rawCode);
}

} // namespace

/// @brief Метод проверки на соответствие синтаксису Markdown
/// @param path Путь к проверяемому файлу
/// @return true, если файл соответствует синтаксису Markdown, иначе false
bool Scanner::checkValidation(const std::filesystem::path &path) {
  if (path.extension() != ".md" && path.extension() != ".markdown") {
    std::cout << "[Ошибка валидации]: Неверное расширение файла (" << path.extension().string()
              << "). Ожидается .md или .markdown!\n";
    return false;
  }

  MappedFile file(path);
  if (!file.ok()) {
    std::cout << "[Ошибка валидации]: Не удалось открыть файл для чтения: " << path.string() << "\n";
    return false;
  }

  std::string_view text = file.data();
  if (text.empty()) {
    std::cout << "[Ошибка валидации]: Файл пуст!\n";
    return false;
  }

  bool inCodeBlock = false;
  bool hasAnyMarkdown = false;
  size_t lineNumber = 0;
  std::string_view line;

  auto fail = [&](const char *reason) {
    std::cout << "[Ошибка валидации] Строка " << lineNumber << ": " << reason << ": \"" << line << "\"\n";
    return false;
  };

  while (nextLine(text, line)) {
    ++lineNumber;

    if (isCodeFence(line)) {
      inCodeBlock = !inCodeBlock;
      hasAnyMarkdown = true;
      continue;
    }
    if (inCodeBlock || isBlank(line)) {
      continue;
    }

    if (hasHtml(line)) return fail("обнаружен тег чужого языка разметки HTML");
    if (hasLatex(line)) return fail("обнаружены команды чужого языка разметки LaTeX");

    if (isHeader(line) || isListItem(line) || hasLink(line)) {
      hasAnyMarkdown = true;
      continue;
    }

    if (hasRawCode(line)) return fail("обнаружен программный код вне блока ```");
  }

  if (inCodeBlock) {
    std::cout << "Блок кода (```) не был закрыт до конца файла!\n";
    return false;
  }

  if (!hasAnyMarkdown) {
    std::cout << "Файл не содержит ни одного элемента разметки Markdown "
                 "(заголовки, списки, ссылки, блоки кода)!\n";
    return false;
  }

  return true;
}

/// @brief Анализ содержимого файла: поиск и извлечение заголовков, абзацев и списков
/// @param path Путь к Markdown-файлу
/// @return Структура со статистикой и найденными элементами MarkdownStats
MarkdownStats Scanner::analyzeMarkdown(const std::filesystem::path &path) {
  // Сколько элементов каждого типа сохранять в векторах. Без лимита файл в десятки ГБ
  // исчерпает память (std::bad_alloc); счётчики при этом считают все элементы.
  constexpr size_t kMaxStored = 50000;

  MarkdownStats stats;
  MappedFile file(path);
  std::string_view text = file.data(); // пусто, если файл не открылся

  bool inCodeBlock = false;
  bool inList = false;
  bool storingList = false; // сохраняется ли текущий список в stats.lists
  std::string paragraph;

  auto flushParagraph = [&]() {
    if (paragraph.empty()) {
      return;
    }
    if (stats.paragraphsCount++ < kMaxStored) {
      stats.paragraphs.push_back(std::move(paragraph));
    }
    paragraph.clear();
  };

  std::string_view line;
  while (nextLine(text, line)) {
    if (isCodeFence(line)) {
      flushParagraph();
      inCodeBlock = !inCodeBlock;
      inList = false;
      continue;
    }
    if (inCodeBlock) {
      continue;
    }

    // Пустая строка завершает текущий абзац и список
    if (isBlank(line)) {
      flushParagraph();
      inList = false;
      continue;
    }

    if (isHeader(line)) {
      flushParagraph();
      inList = false;
      if (stats.headersCount++ < kMaxStored) {
        stats.headers.emplace_back(line);
      }
      continue;
    }

    if (isListItem(line)) {
      flushParagraph();
      if (!inList) { // начался новый список
        inList = true;
        storingList = stats.listsCount++ < kMaxStored;
        if (storingList) {
          stats.lists.emplace_back();
        }
      }
      ++stats.listItemsCount;
      if (storingList && stats.lists.back().size() < kMaxStored) {
        stats.lists.back().emplace_back(line);
      }
      continue;
    }

    // Обычный текст накапливается как абзац
    inList = false;
    if (!paragraph.empty()) {
      paragraph += ' ';
    }
    paragraph += line;
  }

  flushParagraph();
  return stats;
}

} // namespace scanfile
