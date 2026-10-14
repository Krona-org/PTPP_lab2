#include "scanfile.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <system_error>

namespace scanfile {

Scanner::Scanner() = default;

Scanner::Scanner(const std::filesystem::path &path) {
  loadFile(path);
}

Scanner::~Scanner() {
  cleanupMapping();
}

Scanner::Scanner(Scanner &&other) noexcept
    : fileHandle_(other.fileHandle_),
      mappingHandle_(other.mappingHandle_),
      mappedData_(other.mappedData_),
      isMapped_(other.isMapped_),
      buffer_(std::move(other.buffer_)),
      buffer_view_(other.buffer_view_),
      fileSize_(other.fileSize_) {
  other.fileHandle_ = nullptr;
  other.mappingHandle_ = nullptr;
  other.mappedData_ = nullptr;
  other.isMapped_ = false;
  other.buffer_view_ = {};
  other.fileSize_ = 0;
}

Scanner &Scanner::operator=(Scanner &&other) noexcept {
  if (this != &other) {
    cleanupMapping();
    fileHandle_ = other.fileHandle_;
    mappingHandle_ = other.mappingHandle_;
    mappedData_ = other.mappedData_;
    isMapped_ = other.isMapped_;
    buffer_ = std::move(other.buffer_);
    buffer_view_ = other.buffer_view_;
    fileSize_ = other.fileSize_;

    other.fileHandle_ = nullptr;
    other.mappingHandle_ = nullptr;
    other.mappedData_ = nullptr;
    other.isMapped_ = false;
    other.buffer_view_ = {};
    other.fileSize_ = 0;
  }
  return *this;
}

void Scanner::cleanupMapping() {
  if (mappedData_) {
    UnmapViewOfFile(mappedData_);
    mappedData_ = nullptr;
  }
  if (mappingHandle_) {
    CloseHandle(static_cast<HANDLE>(mappingHandle_));
    mappingHandle_ = nullptr;
  }
  if (fileHandle_ && fileHandle_ != INVALID_HANDLE_VALUE) {
    CloseHandle(static_cast<HANDLE>(fileHandle_));
    fileHandle_ = nullptr;
  }
  isMapped_ = false;
}

bool Scanner::loadFile(const std::filesystem::path &path) {
  cleanupMapping();
  buffer_.clear();
  buffer_view_ = {};
  fileSize_ = 0;

  std::error_code ec;
  fileSize_ = std::filesystem::file_size(path, ec);
  if (ec) {
    std::cerr << "Не удалось получить размер файла: " << ec.message() << "\n";
    return false;
  }

  if (fileSize_ == 0) {
    buffer_view_ = {};
    return true;
  }

  // Случай 1: Файл >= 4 ГБ -> Win32 Memory-Mapped File (лучшая практика из py_war)
  if (fileSize_ >= MMF_THRESHOLD) {
    HANDLE hFile = CreateFileW(
        path.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, // Подсказка ОС оптимизировать предвыборку страниц
        nullptr);

    if (hFile == INVALID_HANDLE_VALUE) {
      std::cerr << "Ошибка CreateFileW для MMF: код " << GetLastError() << "\n";
      return false;
    }

    // В 64-битной Windows передача 0 в максимальный размер берет текущий размер файла
    HANDLE hMapping = CreateFileMappingW(hFile, nullptr, PAGE_READONLY, 0, 0, nullptr);
    if (!hMapping) {
      std::cerr << "Ошибка CreateFileMappingW для MMF: код " << GetLastError() << "\n";
      CloseHandle(hFile);
      return false;
    }

    const char *pData = static_cast<const char *>(MapViewOfFile(hMapping, FILE_MAP_READ, 0, 0, 0));
    if (!pData) {
      std::cerr << "Ошибка MapViewOfFile для MMF: код " << GetLastError() << "\n";
      CloseHandle(hMapping);
      CloseHandle(hFile);
      return false;
    }

    fileHandle_ = hFile;
    mappingHandle_ = hMapping;
    mappedData_ = pData;
    isMapped_ = true;

    // buffer_view_ указывает прямо на отображенную виртуальную память
    buffer_view_ = std::string_view(mappedData_, fileSize_);
    return true;
  }

  // Случай 2: Файл < 4 ГБ -> Чтение в RAM
  std::ifstream file(path, std::ios::binary);
  if (!file.is_open()) {
    return false;
  }

  try {
    buffer_.resize(fileSize_);
    file.read(buffer_.data(), static_cast<std::streamsize>(fileSize_));
    buffer_view_ = buffer_;
    isMapped_ = false;
    return true;
  } catch (const std::bad_alloc &) {
    // В случае нехватки ОЗУ при выделении буфера переключаемся на MMF
    HANDLE hFile = CreateFileW(
        path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return false;
    HANDLE hMapping = CreateFileMappingW(hFile, nullptr, PAGE_READONLY, 0, 0, nullptr);
    if (!hMapping) { CloseHandle(hFile); return false; }
    const char *pData = static_cast<const char *>(MapViewOfFile(hMapping, FILE_MAP_READ, 0, 0, 0));
    if (!pData) { CloseHandle(hMapping); CloseHandle(hFile); return false; }
    fileHandle_ = hFile;
    mappingHandle_ = hMapping;
    mappedData_ = pData;
    isMapped_ = true;
    buffer_view_ = std::string_view(mappedData_, fileSize_);
    return true;
  }
}

/// @brief Метод проверки на соответствие синтаксису Markdown
/// @param path Путь к проверяемому файлу
/// @return true, если файл соответствует синтаксису Markdown, иначе false
bool Scanner::checkValidation(const std::filesystem::path &path) {
  if (path.extension() != ".md" && path.extension() != ".markdown") {
    std::cout << "[Ошибка валидации]: Неверное расширение файла ("
              << path.extension().string()
              << "). Ожидается .md или .markdown!\n";
    return false;
  }

  Scanner scanner;
  if (!scanner.loadFile(path)) {
    std::cout << "[Ошибка валидации]: Не удалось открыть/отобразить файл для чтения: "
              << path.string() << "\n";
    return false;
  }

  std::string_view data = scanner.getBufferView();
  if (data.empty()) {
    std::cout << "[Ошибка валидации]: Файл пуст!\n";
    return false;
  }

  if (scanner.isMemoryMapped()) {
    std::cout << "[Инфо]: Файл (" << (scanner.getFileSize() / (1024ULL * 1024 * 1024))
              << " ГБ) успешно отображен через Win32 Memory-Mapped File (MMF).\n";
  }

  static const std::regex rawCodeRegex(R"(\w+\(.*\)|;\s*$)");
  static const std::regex foreignHtmlRegex(
      R"(<\/?(html|head|body|div|p|script|style|table|tr|td|span|h[1-6])\b[^>]*>)",
      std::regex::icase);

  bool inCodeBlock = false;
  bool hasAnyMarkdown = false;
  size_t lineNumber = 0;

  const char *p = data.data();
  const char *end = p + data.size();

  while (p < end) {
    const char *lineStart = p;
    while (p < end && *p != '\n') {
      ++p;
    }
    const char *lineEnd = p;
    if (lineEnd > lineStart && *(lineEnd - 1) == '\r') {
      --lineEnd;
    }
    if (p < end && *p == '\n') {
      ++p;
    }
    lineNumber++;

    std::string_view line(lineStart, lineEnd - lineStart);

    // Проверка на блок кода ```
    if (line.size() >= 3 && line[0] == '`' && line[1] == '`' && line[2] == '`' &&
        std::regex_match(line.data(), line.data() + line.size(), MarkdownRegex::codeBlock)) {
      inCodeBlock = !inCodeBlock;
      hasAnyMarkdown = true;
      continue;
    }

    if (inCodeBlock) {
      continue;
    }

    // Проверка на пустую строку
    bool isEmpty = true;
    for (char ch : line) {
      if (ch != ' ' && ch != '\t' && ch != '\r' && ch != '\v' && ch != '\f') {
        isEmpty = false;
        break;
      }
    }
    if (isEmpty) {
      continue;
    }

    // Проверка на чужую разметку (HTML-документ или теги)
    if (line.find('<') != std::string_view::npos) {
      if (std::regex_search(line.data(), line.data() + line.size(), foreignHtmlRegex)) {
        std::cout << "[Ошибка валидации] Строка " << lineNumber
                  << ": обнаружен тег чужого языка разметки HTML: \"" << line
                  << "\"\n";
        return false;
      }
    }

    // Проверка на чужую разметку (LaTeX)
    if (line.find('\\') != std::string_view::npos) {
      if (std::regex_search(line.data(), line.data() + line.size(), MarkdownRegex::latexCommand)) {
        std::cout << "[Ошибка валидации] Строка " << lineNumber
                  << ": обнаружены команды чужого языка разметки LaTeX: \""
                  << line << "\"\n";
        return false;
      }
    }

    // Проверка стандартных элементов Markdown
    bool isStandardMd = false;
    if (!line.empty() && line[0] == '#') {
      if (std::regex_match(line.data(), line.data() + line.size(), MarkdownRegex::header)) {
        isStandardMd = true;
      }
    } else {
      size_t nonSpace = line.find_first_not_of(" \t");
      if (nonSpace != std::string_view::npos) {
        char fc = line[nonSpace];
        if ((fc == '-' || fc == '*' || fc == '+') &&
            std::regex_match(line.data(), line.data() + line.size(), MarkdownRegex::bulletList)) {
          isStandardMd = true;
        } else if (fc >= '0' && fc <= '9' &&
                   std::regex_match(line.data(), line.data() + line.size(), MarkdownRegex::numberList)) {
          isStandardMd = true;
        }
      }
    }

    if (!isStandardMd && line.find('[') != std::string_view::npos && line.find(']') != std::string_view::npos) {
      if (std::regex_search(line.data(), line.data() + line.size(), MarkdownRegex::inlineLink)) {
        isStandardMd = true;
      }
    }

    if (isStandardMd) {
      hasAnyMarkdown = true;
      continue;
    }

    // Сырой исходный код вне блока ```
    if (line.find('(') != std::string_view::npos || (!line.empty() && line.back() == ';') || line.find(';') != std::string_view::npos) {
      if (std::regex_search(line.data(), line.data() + line.size(), rawCodeRegex)) {
        std::cout << "Строка " << lineNumber
                  << ": обнаружен программный код вне блока ```: \"" << line
                  << "\"\n";
        return false;
      }
    }
  }

  if (inCodeBlock) {
    std::cout << "Блок кода (```) не был закрыт до конца файла!\n";
    return false;
  }

  if (!hasAnyMarkdown) {
    std::cout << "Файл не содержит ни одного элемента разметки Markdown (заголовки, списки, ссылки, блоки кода)!\n";
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

  Scanner scanner;
  if (!scanner.loadFile(path)) {
    return stats;
  }

  std::string_view data = scanner.getBufferView();
  if (data.empty()) {
    return stats;
  }

  if (scanner.isMemoryMapped()) {
    std::cout << "[Инфо]: Анализ файла (" << (scanner.getFileSize() / (1024ULL * 1024 * 1024))
              << " ГБ) через Win32 Memory-Mapped File (MMF)...\n";
  }

  bool inCodeBlock = false;
  bool inList = false;
  std::string currentParagraph;

  // Ограничение на максимальное количество хранимых в STL-векторах строк
  // для предотвращения переполнения памяти (std::bad_alloc) при файлах в несколько гигабайт
  static constexpr size_t MAX_STORED_ELEMENTS = 50000;

  auto flushParagraph = [&]() {
    if (!currentParagraph.empty()) {
      stats.paragraphsCount++;
      if (stats.paragraphs.size() < MAX_STORED_ELEMENTS) {
        stats.paragraphs.push_back(currentParagraph);
      }
      currentParagraph.clear();
    }
  };

  const char *p = data.data();
  const char *end = p + data.size();

  while (p < end) {
    const char *lineStart = p;
    while (p < end && *p != '\n') {
      ++p;
    }
    const char *lineEnd = p;
    if (lineEnd > lineStart && *(lineEnd - 1) == '\r') {
      --lineEnd;
    }
    if (p < end && *p == '\n') {
      ++p;
    }

    std::string_view line(lineStart, lineEnd - lineStart);

    // Блок кода ```
    if (line.size() >= 3 && line[0] == '`' && line[1] == '`' && line[2] == '`' &&
        std::regex_match(line.data(), line.data() + line.size(), MarkdownRegex::codeBlock)) {
      flushParagraph();
      inCodeBlock = !inCodeBlock;
      inList = false;
      continue;
    }

    if (inCodeBlock) {
      continue;
    }

    // Пустая строка завершает текущий абзац и список
    bool isEmpty = true;
    for (char ch : line) {
      if (ch != ' ' && ch != '\t' && ch != '\r' && ch != '\v' && ch != '\f') {
        isEmpty = false;
        break;
      }
    }
    if (isEmpty) {
      flushParagraph();
      inList = false;
      continue;
    }

    // Заголовок (#{1,6})
    if (!line.empty() && line[0] == '#' &&
        std::regex_match(line.data(), line.data() + line.size(), MarkdownRegex::header)) {
      flushParagraph();
      stats.headersCount++;
      if (stats.headers.size() < MAX_STORED_ELEMENTS) {
        stats.headers.emplace_back(line);
      }
      inList = false;
      continue;
    }

    // Элемент списка (маркированный или нумерованный)
    bool isListItem = false;
    size_t nonSpace = line.find_first_not_of(" \t");
    if (nonSpace != std::string_view::npos) {
      char fc = line[nonSpace];
      if ((fc == '-' || fc == '*' || fc == '+') &&
          std::regex_match(line.data(), line.data() + line.size(), MarkdownRegex::bulletList)) {
        isListItem = true;
      } else if (fc >= '0' && fc <= '9' &&
                 std::regex_match(line.data(), line.data() + line.size(), MarkdownRegex::numberList)) {
        isListItem = true;
      }
    }

    if (isListItem) {
      flushParagraph();
      if (!inList) {
        stats.listsCount++;
        if (stats.lists.size() < MAX_STORED_ELEMENTS) {
          stats.lists.push_back({}); // Начался новый список
        }
        inList = true;
      }
      stats.listItemsCount++;
      if (!stats.lists.empty() && stats.lists.back().size() < MAX_STORED_ELEMENTS) {
        stats.lists.back().emplace_back(line);
      }
      continue;
    }

    // Обычный текст - накапливается как абзац
    inList = false;
    if (!currentParagraph.empty()) {
      currentParagraph += " ";
    }
    currentParagraph.append(line.data(), line.size());
  }

  flushParagraph();

  return stats;
}

} // namespace scanfile
