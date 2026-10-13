#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace userdata {

/// @brief Структура для хранения данных пользователя (анкеты)
struct UserData {
  std::string lastName;   ///< Фамилия
  std::string firstName;  ///< Имя
  std::string middleName; ///< Отчество
  std::string birthDate;  ///< Дата рождения
  std::string phone;      ///< Телефон
  std::string email;      ///< Электронная почта

  /// @brief Преобразование в единую строку через пробел
  std::string toString() const;
};

/// @brief Класс для ввода, валидации, нормализации и генерации данных анкеты
class UserDataProcessor {
public:
  /// @brief 3.a. Функция ввода пользовательских данных из консоли
  static UserData inputUserData();

  /// @brief Разбор входной строки на компоненты UserData
  static UserData parseLine(const std::string &rawLine);

  /// @brief 3.b. Функция проверки и приведения введённых данных к целевому шаблону
  /// @param input Исходные данные
  /// @param output Нормализованные данные, соответствующие шаблону
  /// @param errors Список ошибок, если данные некорректны
  /// @return true, если данные успешно нормализованы
  static bool normalizeAndValidate(const UserData &input, UserData &output,
                                   std::vector<std::string> &errors);

  /// @brief Нормализация строки целиком (пример из задания: Сидоров ИваН ПетРОви1ч ...)
  static bool normalizeLine(const std::string &rawLine, std::string &normalizedLine,
                            std::vector<std::string> &errors);

  /// @brief Нормализация части ФИО (удаление цифр/мусора, регистр: первая заглавная)
  static bool normalizeNamePart(const std::string &raw, std::string &normalized);

  /// @brief Нормализация даты рождения к формату DD.MM.YYYY
  static bool normalizeBirthDate(const std::string &raw, std::string &normalized);

  /// @brief Нормализация номера телефона к формату +7XXXXXXXXXX
  static bool normalizePhone(const std::string &raw, std::string &normalized);

  /// @brief Нормализация email (валидация и нижний регистр)
  static bool normalizeEmail(const std::string &raw, std::string &normalized);

  /// @brief 3.c. Генерация случайной анкеты с использованием словарей
  /// @param withFlaws Добавлять ли реалистичные погрешности/различные форматы (для проверки п. 3.b)
  static UserData generateRandomUserData(bool withFlaws = true);

  /// @brief Генерация набора случайных анкет
  static std::vector<UserData> generateRandomDataset(size_t count, bool withFlaws = true);

  /// @brief Обработка файла с анкетами: валидация, нормализация и вывод
  static void processFile(const std::filesystem::path &filePath);
};

} // namespace userdata
