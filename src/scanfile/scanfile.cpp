#include "scanfile.h"

#include <windows.h>
#include <io.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <regex>
#include <sstream>
#include <string_view>
#include <thread>
#include <vector>

namespace scanfile {
namespace {

/// @brief Шаблоны регулярных выражений
namespace re {
const std::regex header{R"(^(#{1,6})\s+(.*))"};
const std::regex bulletList{R"(^\s*[-*+]\s+(.*))"};
const std::regex numberList{R"(^\s*(\d+)\.\s+(.*))"};
const std::regex codeBlock{R"(^```(\w*))"};
const std::regex inlineLink{R"(\[([^\]]+)\]\(([^)]+)\))"};
const std::regex html{R"(<\/?(html|head|body|div|p|script|style|table|tr|td|span|h[1-6])\b[^>]*>)",
                      std::regex::icase};
const std::regex latex{R"(\\(documentclass|begin|section|subsection|usepackage)\b)"};
const std::regex rawCode{R"(\w+\(.*\)|;\s*$)"};
} // namespace re

/// @brief Read-only отображение файла в память (Win32 Memory-Mapped File).
class MappedFile {
public:
  explicit MappedFile(const std::filesystem::path &path) {
    file_ = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    LARGE_INTEGER size{};
    if (file_ == INVALID_HANDLE_VALUE || !GetFileSizeEx(file_, &size)) {
      return;
    }
    if (size.QuadPart == 0) {
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
  size_t size() const { return view_.size(); }

private:
  HANDLE file_{INVALID_HANDLE_VALUE};
  HANDLE mapping_{nullptr};
  std::string_view view_;
  bool ok_{false};
};

/// @brief Извлекает очередную строку без аллокаций
inline bool nextLine(std::string_view &text, std::string_view &line) {
  if (text.empty()) return false;
  const void *res = memchr(text.data(), '\n', text.size());
  size_t eol = res ? (static_cast<const char *>(res) - text.data()) : text.size();
  line = text.substr(0, eol);
  text.remove_prefix(eol < text.size() ? eol + 1 : eol);
  if (!line.empty() && line.back() == '\r') {
    line.remove_suffix(1);
  }
  return true;
}

inline bool isBlank(std::string_view s) {
  return s.find_first_not_of(" \t\r\v\f") == std::string_view::npos;
}

inline bool isWordChar(char c) {
  return (static_cast<unsigned char>(c) >= 'a' && static_cast<unsigned char>(c) <= 'z') ||
         (static_cast<unsigned char>(c) >= 'A' && static_cast<unsigned char>(c) <= 'Z') ||
         (static_cast<unsigned char>(c) >= '0' && static_cast<unsigned char>(c) <= '9') ||
         c == '_';
}

inline bool isCodeFence(std::string_view s) {
  if (s.size() < 3 || s[0] != '`' || s[1] != '`' || s[2] != '`') return false;
  for (size_t i = 3; i < s.size(); ++i) {
    char c = s[i];
    if (!isWordChar(c)) {
      return std::regex_match(s.data(), s.data() + s.size(), re::codeBlock);
    }
  }
  return true;
}

inline bool isHeader(std::string_view s) {
  if (s.empty() || s[0] != '#') return false;
  size_t hashes = 0;
  while (hashes < s.size() && s[hashes] == '#') {
    ++hashes;
  }
  if (hashes < 1 || hashes > 6 || hashes >= s.size()) return false;
  char next = s[hashes];
  if (next != ' ' && next != '\t') return false;
  return std::regex_match(s.data(), s.data() + s.size(), re::header);
}

inline bool isListItem(std::string_view s) {
  size_t i = s.find_first_not_of(" \t");
  if (i == std::string_view::npos) return false;
  char c = s[i];
  if (c == '-' || c == '*' || c == '+') {
    if (i + 1 < s.size() && (s[i + 1] == ' ' || s[i + 1] == '\t')) {
      return std::regex_match(s.data(), s.data() + s.size(), re::bulletList);
    }
    return false;
  }
  if (c >= '0' && c <= '9') {
    size_t d = i + 1;
    while (d < s.size() && s[d] >= '0' && s[d] <= '9') {
      ++d;
    }
    if (d < s.size() && s[d] == '.' && d + 1 < s.size() && (s[d + 1] == ' ' || s[d + 1] == '\t')) {
      return std::regex_match(s.data(), s.data() + s.size(), re::numberList);
    }
    return false;
  }
  return false;
}

inline bool hasLink(std::string_view s) {
  size_t openBracket = s.find('[');
  if (openBracket == std::string_view::npos) return false;
  size_t linkJoin = s.find("](", openBracket + 1);
  if (linkJoin == std::string_view::npos) return false;
  return std::regex_search(s.data(), s.data() + s.size(), re::inlineLink);
}

inline bool hasHtml(std::string_view s) {
  size_t p = s.find('<');
  while (p != std::string_view::npos) {
    if (p + 1 < s.size()) {
      char c = s[p + 1];
      if (c == '/' && p + 2 < s.size()) {
        char c2 = s[p + 2];
        if ((c2 >= 'a' && c2 <= 'z') || (c2 >= 'A' && c2 <= 'Z')) {
          return std::regex_search(s.data(), s.data() + s.size(), re::html);
        }
      } else if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) {
        return std::regex_search(s.data(), s.data() + s.size(), re::html);
      }
    }
    p = s.find('<', p + 1);
  }
  return false;
}

inline bool hasLatex(std::string_view s) {
  size_t p = s.find('\\');
  while (p != std::string_view::npos) {
    if (p + 1 < s.size()) {
      char c = s[p + 1];
      if (c == 'd' || c == 'b' || c == 's' || c == 'u') {
        return std::regex_search(s.data(), s.data() + s.size(), re::latex);
      }
    }
    p = s.find('\\', p + 1);
  }
  return false;
}

inline bool hasRawCode(std::string_view s) {
  size_t last = s.find_last_not_of(" \t\r\v\f");
  if (last != std::string_view::npos && s[last] == ';') {
    return std::regex_search(s.data(), s.data() + s.size(), re::rawCode);
  }
  size_t p = s.find('(');
  while (p != std::string_view::npos) {
    if (p > 0 && isWordChar(s[p - 1])) {
      return std::regex_search(s.data(), s.data() + s.size(), re::rawCode);
    }
    p = s.find('(', p + 1);
  }
  return false;
}

/// @brief Консольный анимированный прогресс-бар
class ProgressBar {
public:
  ProgressBar(uint64_t totalBytes, std::string_view title, unsigned int threads)
      : totalBytes_(totalBytes), title_(title), threads_(threads),
        enabled_(totalBytes >= 1024 * 1024),
        startTime_(std::chrono::steady_clock::now()), isTty_(_isatty(_fileno(stdout))) {
    if (enabled_) {
      active_.store(true);
      monitorThread_ = std::thread([this]() { run(); });
    }
  }

  ~ProgressBar() {
    stop();
  }

  void add(uint64_t bytes) {
    if (enabled_) {
      processedBytes_.fetch_add(bytes, std::memory_order_relaxed);
    }
  }

  void stop() {
    if (enabled_ && active_.exchange(false)) {
      if (monitorThread_.joinable()) {
        monitorThread_.join();
      }
      render(true);
      std::cout << "\n";
    }
  }

private:
  void run() {
    while (active_.load()) {
      render(false);
      std::this_thread::sleep_for(std::chrono::milliseconds(150));
    }
  }

  void render(bool finish) {
    uint64_t current = finish ? totalBytes_ : processedBytes_.load(std::memory_order_relaxed);
    if (current > totalBytes_) current = totalBytes_;

    double progress = totalBytes_ > 0 ? static_cast<double>(current) / totalBytes_ : 1.0;
    auto now = std::chrono::steady_clock::now();
    double elapsedSec = std::chrono::duration<double>(now - startTime_).count();
    double speedMBs = (elapsedSec > 0.05) ? (static_cast<double>(current) / (1024.0 * 1024.0)) / elapsedSec : 0.0;
    double remainingSec = (speedMBs > 0.01 && progress < 0.999)
                              ? (static_cast<double>(totalBytes_ - current) / (1024.0 * 1024.0)) / speedMBs
                              : 0.0;

    int barWidth = 30;
    int filled = static_cast<int>(progress * barWidth);
    if (filled > barWidth) filled = barWidth;

    std::ostringstream ss;
    ss << title_ << " [";
    for (int i = 0; i < barWidth; ++i) {
      if (i < filled) ss << "=";
      else if (i == filled) ss << ">";
      else ss << " ";
    }
    ss << "] " << std::fixed << std::setprecision(1) << (progress * 100.0) << "% | "
       << std::setprecision(2) << (static_cast<double>(current) / (1024.0 * 1024.0 * 1024.0)) << " / "
       << (static_cast<double>(totalBytes_) / (1024.0 * 1024.0 * 1024.0)) << " ГБ | "
       << std::setprecision(1) << speedMBs << " МБ/с | ";

    if (finish) {
      ss << "Завершено за " << std::setprecision(1) << elapsedSec << "с";
    } else {
      ss << "ETA: " << static_cast<int>(remainingSec) << "с [" << threads_ << " потоков]";
    }

    if (isTty_) {
      std::cout << "\r" << ss.str() << std::flush;
    } else if (finish) {
      std::cout << ss.str() << std::endl;
    }
  }

  uint64_t totalBytes_{0};
  std::string title_;
  unsigned int threads_{1};
  std::atomic<uint64_t> processedBytes_{0};
  std::atomic<bool> active_{false};
  std::chrono::steady_clock::time_point startTime_;
  bool isTty_{false};
  bool enabled_{false};
  std::thread monitorThread_;
};

struct Chunk {
  size_t index{0};
  const char *start{nullptr};
  const char *end{nullptr};
  size_t startLineNumber{1};
  size_t fenceCount{0};
  bool startInCodeBlock{false};
  MarkdownStats stats;

  // Данные для склейки элементов на стыках чанков
  bool startsWithBlank{false};
  bool endsWithBlank{false};
  std::string leadingParagraph;
  std::string trailingParagraph;
  bool startsWithList{false};
  bool endsWithList{false};
};

/// @brief Нарезает файл на выровненные по \n блоки
std::vector<Chunk> makeChunks(std::string_view data, size_t numChunks) {
  std::vector<Chunk> chunks;
  chunks.reserve(numChunks);
  const char *base = data.data();
  size_t total = data.size();
  size_t nominal = total / numChunks;

  const char *curr = base;
  const char *fileEnd = base + total;

  for (size_t i = 0; i < numChunks; ++i) {
    const char *chunkStart = curr;
    const char *chunkEnd = fileEnd;
    if (i + 1 < numChunks) {
      const char *target = base + (i + 1) * nominal;
      if (target < fileEnd) {
        const void *nl = memchr(target, '\n', fileEnd - target);
        if (nl) {
          chunkEnd = static_cast<const char *>(nl) + 1;
        }
      }
    }
    chunks.push_back({i, chunkStart, chunkEnd, 1, 0, false, {}, false, false, "", "", false, false});
    curr = chunkEnd;
  }
  return chunks;
}

/// @brief Быстрый параллельный расчет четности блоков кода (```) для каждого чанка
void computeCodeFenceParity(std::vector<Chunk> &chunks, unsigned int numThreads) {
  std::vector<std::thread> workers;
  std::atomic<size_t> chunkIdx{0};

  auto worker = [&]() {
    while (true) {
      size_t idx = chunkIdx.fetch_add(1, std::memory_order_relaxed);
      if (idx >= chunks.size()) break;
      auto &c = chunks[idx];
      const char *p = c.start;
      const char *end = c.end;
      size_t count = 0;

      // Проверка первой строки чанка
      if (end - p >= 3 && p[0] == '`' && p[1] == '`' && p[2] == '`') {
        const char *eol = static_cast<const char *>(memchr(p, '\n', end - p));
        size_t len = eol ? (eol - p) : (end - p);
        std::string_view line(p, len);
        if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
        if (isCodeFence(line)) ++count;
      }

      while (p < end) {
        const void *res = memchr(p, '\n', end - p);
        if (!res) break;
        const char *nl = static_cast<const char *>(res);
        p = nl + 1;
        if (end - p >= 3 && p[0] == '`' && p[1] == '`' && p[2] == '`') {
          const char *eol = static_cast<const char *>(memchr(p, '\n', end - p));
          size_t len = eol ? (eol - p) : (end - p);
          std::string_view line(p, len);
          if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
          if (isCodeFence(line)) ++count;
        }
      }
      c.fenceCount = count;
    }
  };

  for (unsigned int i = 0; i < numThreads; ++i) {
    workers.emplace_back(worker);
  }
  for (auto &w : workers) {
    w.join();
  }

  // Расчет четности перед каждым чанком
  size_t totalFences = 0;
  for (auto &c : chunks) {
    c.startInCodeBlock = (totalFences % 2 != 0);
    totalFences += c.fenceCount;
  }
}

} // namespace

/// @brief Метод проверки на соответствие синтаксису Markdown (многопоточный MMF)
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

  if (file.size() == 0) {
    std::cout << "[Ошибка валидации]: Файл пуст!\n";
    return false;
  }

  unsigned int numThreads = std::thread::hardware_concurrency();
  if (numThreads == 0) numThreads = 8;
  if (file.size() < 4 * 1024 * 1024) numThreads = 1;

  size_t numChunks = (numThreads == 1) ? 1 : (numThreads * 4);
  auto chunks = makeChunks(file.data(), numChunks);

  computeCodeFenceParity(chunks, numThreads);

  ProgressBar bar(file.size(), "Валидация Markdown", numThreads);

  std::atomic<bool> hasError{false};
  std::atomic<bool> hasAnyMarkdownGlobal{false};
  std::mutex errorMutex;
  std::string firstErrorMessage;

  std::vector<std::thread> workers;
  std::atomic<size_t> chunkIdx{0};

  auto worker = [&]() {
    while (!hasError.load(std::memory_order_relaxed)) {
      size_t idx = chunkIdx.fetch_add(1, std::memory_order_relaxed);
      if (idx >= chunks.size()) break;
      auto &c = chunks[idx];

      std::string_view text(c.start, c.end - c.start);
      size_t chunkSize = text.size();
      bool inCodeBlock = c.startInCodeBlock;
      bool hasAnyMarkdown = false;
      size_t lineNumber = 0; // локальный счетчик строк внутри чанка
      std::string_view line;

      auto reportError = [&](const char *reason, std::string_view l) {
        if (!hasError.exchange(true)) {
          std::lock_guard<std::mutex> lock(errorMutex);
          std::ostringstream ss;
          ss << "[Ошибка валидации] Чанк #" << (c.index + 1) << " (строка ~" << lineNumber
             << "): " << reason << ": \"" << l << "\"\n";
          firstErrorMessage = ss.str();
        }
      };

      while (!hasError.load(std::memory_order_relaxed) && nextLine(text, line)) {
        ++lineNumber;

        if (isCodeFence(line)) {
          inCodeBlock = !inCodeBlock;
          hasAnyMarkdown = true;
          continue;
        }
        if (inCodeBlock || isBlank(line)) {
          continue;
        }

        if (hasHtml(line)) {
          reportError("обнаружен тег чужого языка разметки HTML", line);
          break;
        }
        if (hasLatex(line)) {
          reportError("обнаружены команды чужого языка разметки LaTeX", line);
          break;
        }

        if (isHeader(line) || isListItem(line) || hasLink(line)) {
          hasAnyMarkdown = true;
          continue;
        }

        if (hasRawCode(line)) {
          reportError("обнаружен программный код вне блока ```", line);
          break;
        }
      }

      if (hasAnyMarkdown) {
        hasAnyMarkdownGlobal.store(true, std::memory_order_relaxed);
      }
      bar.add(chunkSize);
    }
  };

  for (unsigned int i = 0; i < numThreads; ++i) {
    workers.emplace_back(worker);
  }
  for (auto &w : workers) {
    w.join();
  }

  bar.stop();

  if (hasError.load()) {
    std::cout << firstErrorMessage;
    return false;
  }

  // Проверка незакрытого блока кода в самом конце файла
  size_t totalFences = 0;
  for (const auto &c : chunks) {
    totalFences += c.fenceCount;
  }
  if (totalFences % 2 != 0) {
    std::cout << "Блок кода (```) не был закрыт до конца файла!\n";
    return false;
  }

  if (!hasAnyMarkdownGlobal.load()) {
    std::cout << "Файл не содержит ни одного элемента разметки Markdown "
                 "(заголовки, списки, ссылки, блоки кода)!\n";
    return false;
  }

  return true;
}

/// @brief Анализ содержимого файла (многопоточный MMF с динамической балансировкой)
/// @param path Путь к Markdown-файлу
/// @return Структура со статистикой и найденными элементами MarkdownStats
MarkdownStats Scanner::analyzeMarkdown(const std::filesystem::path &path) {
  constexpr size_t kMaxStored = 50000;
  MarkdownStats totalStats;

  MappedFile file(path);
  if (!file.ok() || file.size() == 0) return totalStats;

  unsigned int numThreads = std::thread::hardware_concurrency();
  if (numThreads == 0) numThreads = 8;
  if (file.size() < 4 * 1024 * 1024) numThreads = 1;

  size_t numChunks = (numThreads == 1) ? 1 : (numThreads * 4);
  auto chunks = makeChunks(file.data(), numChunks);

  // Шаг 1: Быстрый параллельный расчет четности code fence
  computeCodeFenceParity(chunks, numThreads);

  ProgressBar bar(file.size(), "Анализ Markdown", numThreads);

  std::vector<std::thread> workers;
  std::atomic<size_t> chunkIdx{0};

  auto worker = [&]() {
    while (true) {
      size_t idx = chunkIdx.fetch_add(1, std::memory_order_relaxed);
      if (idx >= chunks.size()) break;
      auto &c = chunks[idx];

      std::string_view text(c.start, c.end - c.start);
      size_t chunkSize = text.size();
      bool inCodeBlock = c.startInCodeBlock;
      bool inList = false;
      bool storingList = false;
      std::string paragraph;

      auto flushParagraph = [&]() {
        if (paragraph.empty()) return;
        if (c.stats.paragraphsCount++ < kMaxStored) {
          c.stats.paragraphs.push_back(std::move(paragraph));
        }
        paragraph.clear();
      };

      std::string_view line;
      bool isFirstLine = true;

      while (nextLine(text, line)) {
        if (isCodeFence(line)) {
          flushParagraph();
          inCodeBlock = !inCodeBlock;
          inList = false;
          isFirstLine = false;
          continue;
        }
        if (inCodeBlock) {
          isFirstLine = false;
          continue;
        }

        if (isBlank(line)) {
          flushParagraph();
          inList = false;
          if (isFirstLine) c.startsWithBlank = true;
          isFirstLine = false;
          continue;
        }

        if (isHeader(line)) {
          flushParagraph();
          inList = false;
          if (c.stats.headersCount++ < kMaxStored) {
            c.stats.headers.emplace_back(line);
          }
          isFirstLine = false;
          continue;
        }

        if (isListItem(line)) {
          flushParagraph();
          if (isFirstLine && c.index > 0) {
            c.startsWithList = true;
          }
          if (!inList) {
            inList = true;
            storingList = c.stats.listsCount++ < kMaxStored;
            if (storingList) c.stats.lists.emplace_back();
          }
          ++c.stats.listItemsCount;
          if (storingList && c.stats.lists.back().size() < kMaxStored) {
            c.stats.lists.back().emplace_back(line);
          }
          isFirstLine = false;
          continue;
        }

        // Обычный абзац
        inList = false;
        if (isFirstLine && c.index > 0 && !c.startsWithBlank) {
          c.leadingParagraph = std::string(line);
        }
        if (!paragraph.empty()) paragraph += ' ';
        paragraph += line;
        isFirstLine = false;
      }

      if (!paragraph.empty()) {
        c.trailingParagraph = paragraph;
      }
      c.endsWithList = inList;
      flushParagraph();
      bar.add(chunkSize);
    }
  };

  for (unsigned int i = 0; i < numThreads; ++i) {
    workers.emplace_back(worker);
  }
  for (auto &w : workers) {
    w.join();
  }

  bar.stop();

  // Объединяем статистику из всех чанков с коррекцией стыков
  for (size_t i = 0; i < chunks.size(); ++i) {
    auto &c = chunks[i];

    // Склейка абзаца на стыке чанков
    if (i > 0 && !chunks[i - 1].trailingParagraph.empty() && !c.leadingParagraph.empty()) {
      if (totalStats.paragraphsCount > 0) {
        --totalStats.paragraphsCount;
      }
    }

    // Склейка списка на стыке чанков
    if (i > 0 && chunks[i - 1].endsWithList && c.startsWithList) {
      if (totalStats.listsCount > 0) {
        --totalStats.listsCount;
      }
    }

    totalStats.headersCount += c.stats.headersCount;
    totalStats.paragraphsCount += c.stats.paragraphsCount;
    totalStats.listsCount += c.stats.listsCount;
    totalStats.listItemsCount += c.stats.listItemsCount;

    for (auto &h : c.stats.headers) {
      if (totalStats.headers.size() < kMaxStored) totalStats.headers.push_back(std::move(h));
    }
    for (auto &p : c.stats.paragraphs) {
      if (totalStats.paragraphs.size() < kMaxStored) totalStats.paragraphs.push_back(std::move(p));
    }
    for (auto &l : c.stats.lists) {
      if (totalStats.lists.size() < kMaxStored) totalStats.lists.push_back(std::move(l));
    }
  }

  return totalStats;
}

} // namespace scanfile
