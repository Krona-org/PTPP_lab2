#include <filesystem>
#include <regex>
#include <string>
#include <string_view>
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
};

class Scanner {
public:
  Scanner() = default;
  ~Scanner() = default;

  void scanFile();

  static bool checkValidation(const std::filesystem::path &path);

private:
};

} // namespace scanfile