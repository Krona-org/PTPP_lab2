#pragma once

#include <functional>
#include <iostream>
#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace menu {

class ConsoleMenu {
public:
  ConsoleMenu() = default;
  ~ConsoleMenu() = default;

  /// @brief Установить заголовок меню
  void setTitle(std::string title) { title_ = std::move(title); }

  /// @brief Установить список отображаемых пунктов меню
  void setOptions(std::vector<std::string_view> options) {
    options_ = std::move(options);
  }

  /// @brief Единый метод привязки действия (лямбда, функция, метод любого класса)
  void bindAction(int choice, std::function<void()> action) {
    actions_[choice] = std::move(action);
  }

  /// @brief Запуск классического цикла меню через потоки ввода/вывода
  void showMenu(std::ostream &os = std::cout, std::istream &is = std::cin);

private:
  std::string title_{"--- Меню ---"};
  std::vector<std::string_view> options_;
  std::map<int, std::function<void()>> actions_;
};

inline void ConsoleMenu::showMenu(std::ostream &os, std::istream &is) {
  int choice = -1;

  while (true) {
    os << "\n" << title_ << "\n";
    for (const auto &option : options_) {
      os << option << "\n";
    }
    os << "Выберите пункт: ";

    // Проверка на корректность ввода числа
    if (!(is >> choice)) {
      is.clear();
      is.ignore(10000, '\n');
      os << "Ошибка: введите корректный номер пункта!\n";
      continue;
    }

    // Если выбран пункт выхода (0)
    if (choice == 0) {
      auto it = actions_.find(0);
      if (it != actions_.end() && it->second) {
        it->second();
      }
      os << "Выход из программы.\n";
      break;
    }

    // Поиск и вызов привязанного действия
    auto it = actions_.find(choice);
    if (it != actions_.end() && it->second) {
      os << "\n";
      it->second();
    } else {
      os << "Неверный пункт меню! Попробуйте снова.\n";
    }
  }
}

} // namespace menu