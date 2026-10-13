#include "checkuserdata.h"

#include <algorithm>
#include <cwctype>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <regex>
#include <sstream>
#include <windows.h>

namespace userdata {

namespace {

// Вспомогательные функции преобразования UTF-8 <-> UTF-16 для корректной работы
// с кириллицей в Windows
std::wstring utf8ToWide(const std::string &str) {
  if (str.empty())
    return L"";
  int sizeNeeded = MultiByteToWideChar(
      CP_UTF8, 0, str.data(), static_cast<int>(str.size()), nullptr, 0);
  std::wstring wstr(sizeNeeded, 0);
  MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.size()),
                      &wstr[0], sizeNeeded);
  return wstr;
}

std::string wideToUtf8(const std::wstring &wstr) {
  if (wstr.empty())
    return "";
  int sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, wstr.data(),
                                       static_cast<int>(wstr.size()), nullptr,
                                       0, nullptr, nullptr);
  std::string str(sizeNeeded, 0);
  WideCharToMultiByte(CP_UTF8, 0, wstr.data(), static_cast<int>(wstr.size()),
                      &str[0], sizeNeeded, nullptr, nullptr);
  return str;
}

// Вспомогательные функции преобразования регистра и проверки букв для кириллицы
// и латиницы
wchar_t toLowerUnicode(wchar_t ch) {
  if (ch >= L'A' && ch <= L'Z')
    return ch + (L'a' - L'A');
  if (ch >= 0x0410 && ch <= 0x042F)
    return ch + 0x20; // А-Я -> а-я
  if (ch == 0x0401)
    return 0x0451; // Ё -> ё
  return ch;
}

wchar_t toUpperUnicode(wchar_t ch) {
  if (ch >= L'a' && ch <= L'z')
    return ch - (L'a' - L'A');
  if (ch >= 0x0430 && ch <= 0x044F)
    return ch - 0x20; // а-я -> А-Я
  if (ch == 0x0451)
    return 0x0401; // ё -> Ё
  return ch;
}

bool isAlphaUnicode(wchar_t ch) {
  if ((ch >= L'A' && ch <= L'Z') || (ch >= L'a' && ch <= L'z'))
    return true;
  if ((ch >= 0x0410 && ch <= 0x044F) || ch == 0x0401 || ch == 0x0451)
    return true;
  return false;
}

// Словари для генерации реалистичных данных (мужские и женские)
const std::vector<std::string> maleLastNames = {
    "Иванов",  "Смирнов", "Кузнецов", "Попов",    "Васильев",
    "Петров",  "Соколов", "Михайлов", "Новиков",  "Федоров",
    "Морозов", "Волков",  "Алексеев", "Лебедев",  "Семенов",
    "Егоров",  "Павлов",  "Козлов",   "Степанов", "Николаев"};

const std::vector<std::string> femaleLastNames = {
    "Иванова",  "Смирнова", "Кузнецова", "Попова",    "Васильева",
    "Петрова",  "Соколова", "Михайлова", "Новикова",  "Федорова",
    "Морозова", "Волкова",  "Алексеева", "Лебедева",  "Семенова",
    "Егорова",  "Павлова",  "Козлова",   "Степанова", "Николаева"};

const std::vector<std::string> maleFirstNames = {
    "Александр", "Михаил", "Максим", "Артем",  "Даниил",
    "Дмитрий",   "Иван",   "Матвей", "Илья",   "Роман",
    "Кирилл",    "Егор",   "Никита", "Андрей", "Алексей"};

const std::vector<std::string> femaleFirstNames = {
    "Анастасия", "Мария",     "Анна",    "Виктория", "Полина",
    "Елизавета", "Екатерина", "Ксения",  "Дарья",    "Александра",
    "София",     "Алиса",     "Варвара", "Вероника", "Валерия"};

const std::vector<std::string> maleMiddleNames = {
    "Иванович",   "Петрович",   "Сергеевич",  "Александрович", "Дмитриевич",
    "Алексеевич", "Андреевич",  "Михайлович", "Владимирович",  "Николаевич",
    "Павлович",   "Максимович", "Егорович",   "Артемович"};

const std::vector<std::string> femaleMiddleNames = {
    "Ивановна",   "Петровна",   "Сергеевна",  "Александровна", "Дмитриевна",
    "Алексеевна", "Андреевна",  "Михайловна", "Владимировна",  "Николаевна",
    "Павловна",   "Максимовна", "Егоровна",   "Артемовна"};

const std::vector<std::string> emailDomains = {
    "pochta.ru", "mail.ru", "yandex.ru", "gmail.com", "bk.ru", "rambler.ru"};

} // namespace

std::string UserData::toString() const {
  return lastName + " " + firstName + " " + middleName + " " + birthDate + " " +
         phone + " " + email;
}

/// @brief Разбор строки на 6 полей анкеты
UserData UserDataProcessor::parseLine(const std::string &rawLine) {
  UserData data;
  std::istringstream iss(rawLine);
  iss >> data.lastName >> data.firstName >> data.middleName >> data.birthDate >>
      data.phone >> data.email;
  return data;
}

/// @brief Нормализация отдельной части имени (фамилия/имя/отчество)
/// Удаляет цифры и лишние знаки
/// приводит к регистру: первая заглавная, остальные строчные.
bool UserDataProcessor::normalizeNamePart(const std::string &raw,
                                          std::string &normalized) {
  if (raw.empty())
    return false;

  std::wstring wide = utf8ToWide(raw);
  std::wstring cleaned;

  // Оставляем только буквы и дефис для составных фамилий/имен
  for (wchar_t ch : wide) {
    if (isAlphaUnicode(ch) || ch == L'-') {
      cleaned.push_back(ch);
    }
  }

  if (cleaned.empty())
    return false;

  // Капитализация: первая заглавная, остальные строчные (с учетом дефисов)
  bool capitalizeNext = true;
  for (size_t i = 0; i < cleaned.size(); ++i) {
    if (cleaned[i] == L'-') {
      capitalizeNext = true;
    } else if (capitalizeNext) {
      cleaned[i] = toUpperUnicode(cleaned[i]);
      capitalizeNext = false;
    } else {
      cleaned[i] = toLowerUnicode(cleaned[i]);
    }
  }

  normalized = wideToUtf8(cleaned);
  return true;
}

/// @brief Нормализация даты рождения к целевому шаблону DD.MM.YYYY
/// Поддерживает форматы: YYYY-MM-DD, YYYY.MM.DD, YYYY/MM/DD, DD.MM.YYYY,
/// DD/MM/YYYY, DD-MM-YYYY
bool UserDataProcessor::normalizeBirthDate(const std::string &raw,
                                           std::string &normalized) {
  if (raw.empty())
    return false;

  static const std::regex ymdRegex(R"(^(\d{4})[-/.](\d{1,2})[-/.](\d{1,2})$)");
  static const std::regex dmyRegex(R"(^(\d{1,2})[-/.](\d{1,2})[-/.](\d{4})$)");

  std::smatch match;
  int day = 0, month = 0, year = 0;

  if (std::regex_match(raw, match, ymdRegex)) {
    year = std::stoi(match[1].str());
    month = std::stoi(match[2].str());
    day = std::stoi(match[3].str());
  } else if (std::regex_match(raw, match, dmyRegex)) {
    day = std::stoi(match[1].str());
    month = std::stoi(match[2].str());
    year = std::stoi(match[3].str());
  } else {
    return false;
  }

  // Проверка корректности даты
  if (year < 1900 || year > 2026 || month < 1 || month > 12 || day < 1 ||
      day > 31) {
    return false;
  }

  // Проверка дней в месяце
  static const int daysInMonth[] = {0,  31, 29, 31, 30, 31, 30,
                                    31, 31, 30, 31, 30, 31};
  if (day > daysInMonth[month]) {
    return false;
  }

  std::ostringstream oss;
  oss << std::setw(2) << std::setfill('0') << day << "." << std::setw(2)
      << std::setfill('0') << month << "." << std::setw(4) << std::setfill('0')
      << year;

  normalized = oss.str();
  return true;
}

/// @brief Нормализация номера телефона к шаблону +7XXXXXXXXXX
/// Обрабатывает: +7996-8432-431, 8 (996) 843-24-31, 89968432431, +7 996 843 24
/// 31, 79968432431
bool UserDataProcessor::normalizePhone(const std::string &raw,
                                       std::string &normalized) {
  if (raw.empty())
    return false;

  // Очистка от знаков разделителей (скобки, дефисы, пробелы, знаки плюс)
  static const std::regex nonDigits(R"([^\d])");
  std::string digits = std::regex_replace(raw, nonDigits, "");

  if (digits.length() == 11) {
    if (digits.front() == '8' || digits.front() == '7') {
      normalized = "+7" + digits.substr(1);
      return true;
    }
  } else if (digits.length() == 10) {
    normalized = "+7" + digits;
    return true;
  }

  return false;
}

/// @brief Нормализация email к нижнему регистру и проверка регулярным
/// выражением
bool UserDataProcessor::normalizeEmail(const std::string &raw,
                                       std::string &normalized) {
  if (raw.empty())
    return false;

  static const std::regex emailRegex(
      R"(^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,}$)");

  if (!std::regex_match(raw, emailRegex)) {
    return false;
  }

  std::string lower = raw;
  for (char &c : lower) {
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }

  normalized = lower;
  return true;
}

/// @brief 3.b. Полная валидация и нормализация анкеты
bool UserDataProcessor::normalizeAndValidate(const UserData &input,
                                             UserData &output,
                                             std::vector<std::string> &errors) {
  errors.clear();
  bool ok = true;

  if (!normalizeNamePart(input.lastName, output.lastName)) {
    errors.push_back("Некорректная фамилия: \"" + input.lastName + "\"");
    ok = false;
  }
  if (!normalizeNamePart(input.firstName, output.firstName)) {
    errors.push_back("Некорректное имя: \"" + input.firstName + "\"");
    ok = false;
  }
  if (!normalizeNamePart(input.middleName, output.middleName)) {
    errors.push_back("Некорректное отчество: \"" + input.middleName + "\"");
    ok = false;
  }
  if (!normalizeBirthDate(input.birthDate, output.birthDate)) {
    errors.push_back("Некорректная дата рождения: \"" + input.birthDate +
                     "\" (ожидаются форматы YYYY-MM-DD, DD.MM.YYYY и др.)");
    ok = false;
  }
  if (!normalizePhone(input.phone, output.phone)) {
    errors.push_back("Некорректный номер телефона: \"" + input.phone +
                     "\" (ожидается 10-11 цифр)");
    ok = false;
  }
  if (!normalizeEmail(input.email, output.email)) {
    errors.push_back("Некорректный email: \"" + input.email + "\"");
    ok = false;
  }

  return ok;
}

/// @brief Нормализация строки целиком
bool UserDataProcessor::normalizeLine(const std::string &rawLine,
                                      std::string &normalizedLine,
                                      std::vector<std::string> &errors) {
  UserData raw = parseLine(rawLine);
  UserData normalized;
  if (normalizeAndValidate(raw, normalized, errors)) {
    normalizedLine = normalized.toString();
    return true;
  }
  return false;
}

/// @brief 3.a. Функция ввода пользовательских данных из консоли
UserData UserDataProcessor::inputUserData() {
  std::cout << "\n--- Ввод пользовательских данных ---\n";
  std::cout << "Выберите формат ввода:\n";
  std::cout << "  1. Ввести все данные одной строкой (пример: Сидоров ИваН "
               "ПетРОви1ч 2002-03-01 +7996-8432-431 sidorovVV@pochta.ru)\n";
  std::cout << "  2. Ввести каждое поле по отдельности\n";
  std::cout << "Ваш выбор: ";

  int mode = 1;
  if (!(std::cin >> mode)) {
    std::cin.clear();
    std::cin.ignore(10000, '\n');
    mode = 1;
  }
  std::cin.ignore(10000, '\n'); // очистка остатка строки

  UserData data;
  if (mode == 1) {
    std::cout << "\nВведите данные анкеты:\n> ";
    std::string line;
    std::getline(std::cin, line);
    data = parseLine(line);
  } else {
    std::cout << "Фамилия: ";
    std::getline(std::cin, data.lastName);
    std::cout << "Имя: ";
    std::getline(std::cin, data.firstName);
    std::cout << "Отчество: ";
    std::getline(std::cin, data.middleName);
    std::cout << "Дата рождения (например, 2002-03-01 или 01.03.2002): ";
    std::getline(std::cin, data.birthDate);
    std::cout << "Телефон (например, +7996-8432-431 или 8(996)843-24-31): ";
    std::getline(std::cin, data.phone);
    std::cout << "Email (например, sidorovVV@pochta.ru): ";
    std::getline(std::cin, data.email);
  }

  return data;
}

/// @brief 3.c. Генератор случайных пользовательских данных со словарями
UserData UserDataProcessor::generateRandomUserData(bool withFlaws) {
  static std::mt19937 rng(std::random_device{}());

  bool isMale = std::bernoulli_distribution(0.5)(rng);

  const auto &lastNames = isMale ? maleLastNames : femaleLastNames;
  const auto &firstNames = isMale ? maleFirstNames : femaleFirstNames;
  const auto &middleNames = isMale ? maleMiddleNames : femaleMiddleNames;

  std::uniform_int_distribution<size_t> lastDist(0, lastNames.size() - 1);
  std::uniform_int_distribution<size_t> firstDist(0, firstNames.size() - 1);
  std::uniform_int_distribution<size_t> midDist(0, middleNames.size() - 1);
  std::uniform_int_distribution<size_t> domainDist(0, emailDomains.size() - 1);

  std::string lName = lastNames[lastDist(rng)];
  std::string fName = firstNames[firstDist(rng)];
  std::string mName = middleNames[midDist(rng)];

  // Генерация даты
  std::uniform_int_distribution<int> yearDist(1980, 2006);
  std::uniform_int_distribution<int> monthDist(1, 12);
  std::uniform_int_distribution<int> dayDist(1, 28);

  int year = yearDist(rng);
  int month = monthDist(rng);
  int day = dayDist(rng);

  // Телефон
  std::uniform_int_distribution<int> codeDist(900, 999);
  std::uniform_int_distribution<int> num1Dist(100, 999);
  std::uniform_int_distribution<int> num2Dist(10, 99);
  std::uniform_int_distribution<int> num3Dist(10, 99);
  int code = codeDist(rng);
  int n1 = num1Dist(rng);
  int n2 = num2Dist(rng);
  int n3 = num3Dist(rng);

  // Email
  std::string domain = emailDomains[domainDist(rng)];

  UserData user;

  if (withFlaws) {
    // Вносим реалистичные искажения регистра и опечатки (как в примере задания)
    // Например: "Сидоров ИваН ПетРОви1ч"
    std::wstring wFName = utf8ToWide(fName);
    if (wFName.size() > 2) {
      wFName.back() =
          toUpperUnicode(wFName.back()); // последняя буква заглавная: "ИваН"
      fName = wideToUtf8(wFName);
    }

    std::wstring wMName = utf8ToWide(mName);
    if (wMName.size() > 4) {
      wMName[3] = toUpperUnicode(wMName[3]);
      wMName[4] = toUpperUnicode(wMName[4]);
      // Добавляем случайную цифру-опечатку в отчество как в "ПетРОви1ч"
      wMName.insert(wMName.end() - 1, L'1');
      mName = wideToUtf8(wMName);
    }

    // Случайный формат даты
    int dateFmt = std::uniform_int_distribution<int>(0, 3)(rng);
    std::ostringstream dateOss;
    if (dateFmt == 0) {
      // YYYY-MM-DD
      dateOss << year << "-" << std::setw(2) << std::setfill('0') << month
              << "-" << std::setw(2) << std::setfill('0') << day;
    } else if (dateFmt == 1) {
      // DD.MM.YYYY
      dateOss << std::setw(2) << std::setfill('0') << day << "." << std::setw(2)
              << std::setfill('0') << month << "." << year;
    } else if (dateFmt == 2) {
      // YYYY/MM/DD
      dateOss << year << "/" << std::setw(2) << std::setfill('0') << month
              << "/" << std::setw(2) << std::setfill('0') << day;
    } else {
      // DD/MM/YYYY
      dateOss << std::setw(2) << std::setfill('0') << day << "/" << std::setw(2)
              << std::setfill('0') << month << "/" << year;
    }
    user.birthDate = dateOss.str();

    // Случайный формат телефона
    int phoneFmt = std::uniform_int_distribution<int>(0, 3)(rng);
    std::ostringstream phoneOss;
    if (phoneFmt == 0) {
      phoneOss << "+7" << code << "-" << n1 << "-" << n2 << n3;
    } else if (phoneFmt == 1) {
      phoneOss << "8 (" << code << ") " << n1 << "-" << n2 << "-" << n3;
    } else if (phoneFmt == 2) {
      phoneOss << "+7 " << code << " " << n1 << " " << n2 << " " << n3;
    } else {
      phoneOss << "8" << code << n1 << n2 << n3;
    }
    user.phone = phoneOss.str();

    // Email со смешанным регистром
    std::string emailPrefix = "user" + std::to_string(code);
    if (emailPrefix.length() > 4) {
      emailPrefix[0] = static_cast<char>(std::toupper(emailPrefix[0]));
      emailPrefix[2] = static_cast<char>(std::toupper(emailPrefix[2]));
    }
    user.email = emailPrefix + "@" + domain;
  } else {
    std::ostringstream dateOss;
    dateOss << std::setw(2) << std::setfill('0') << day << "." << std::setw(2)
            << std::setfill('0') << month << "." << year;
    user.birthDate = dateOss.str();
    user.phone = "+7" + std::to_string(code) + std::to_string(n1) +
                 std::to_string(n2) + std::to_string(n3);
    user.email = "user" + std::to_string(code) + "@" + domain;
  }

  user.lastName = lName;
  user.firstName = fName;
  user.middleName = mName;

  return user;
}

/// @brief Генерация массива случайных анкет
std::vector<UserData> UserDataProcessor::generateRandomDataset(size_t count,
                                                               bool withFlaws) {
  std::vector<UserData> dataset;
  dataset.reserve(count);
  for (size_t i = 0; i < count; ++i) {
    dataset.push_back(generateRandomUserData(withFlaws));
  }
  return dataset;
}

/// @brief Обработка анкет из файла
void UserDataProcessor::processFile(const std::filesystem::path &filePath) {
  if (filePath.empty()) {
    std::cout << "Файл не выбран!\n";
    return;
  }

  std::ifstream file(filePath);
  if (!file.is_open()) {
    std::cout << "Не удалось открыть файл: " << filePath.string() << "\n";
    return;
  }

  std::cout << "\n=== Обработка файла анкет: " << filePath.filename().u8string()
            << " ===\n";

  std::string line;
  size_t total = 0;
  size_t valid = 0;

  while (std::getline(file, line)) {
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }
    if (line.empty()) {
      continue;
    }

    total++;
    UserData raw = parseLine(line);
    UserData normalized;
    std::vector<std::string> errors;

    std::cout << "\n[Запись #" << total << "]\n";
    std::cout << "  Входные данные:  " << line << "\n";

    if (normalizeAndValidate(raw, normalized, errors)) {
      std::cout << "  Выходные данные: " << normalized.toString() << "\n";
      std::cout << "  Статус: [УСПЕШНО НОРМАЛИЗОВАНО]\n";
      valid++;
    } else {
      std::cout << "  Статус: [ОШИБКИ ВАЛИДАЦИИ]\n";
      for (const auto &err : errors) {
        std::cout << "    * " << err << "\n";
      }
    }
  }

  std::cout << "\nИтого обработано: " << total << ", корректных: " << valid
            << ", с ошибками: " << (total - valid) << "\n";
}

} // namespace userdata
