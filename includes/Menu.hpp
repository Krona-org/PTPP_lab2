#pragma once

#include <functional>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace menu {

/// @brief Элемент меню с идентификатором, заголовком и функцией действия
struct MenuItem {
  int choice{0};
  std::string title;
  std::function<void()> action;
};

class ConsoleMenu {
public:
  /// @brief Установить заголовок меню
  void setTitle(std::string title) { title_ = std::move(title); }

  /// @brief Добавить пункт меню с привязкой действия
  void addItem(int choice, std::string title, std::function<void()> action) {
    items_.push_back({choice, std::move(title), std::move(action)});
  }

  /// @brief Запуск классического цикла меню через потоки ввода/вывода
  void showMenu(std::ostream &os = std::cout, std::istream &is = std::cin);

private:
  const MenuItem *find(int choice) const {
    for (const auto &item : items_) {
      if (item.choice == choice) return &item;
    }
    return nullptr;
  }

  std::string title_{"--- Меню ---"};
  std::vector<MenuItem> items_;
};

inline void ConsoleMenu::showMenu(std::ostream &os, std::istream &is) {
  while (true) {
    os << "\n" << title_ << "\n";
    for (const auto &item : items_) {
      os << "  " << item.choice << ". " << item.title << "\n";
    }
    os << "Выберите пункт: ";

    // Проверка на корректность ввода числа
    int choice = -1;
    if (!(is >> choice)) {
      is.clear();
      is.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
      os << "Ошибка: введите корректный номер пункта!\n";
      continue;
    }

    const MenuItem *item = find(choice);

    // Если выбран пункт выхода (0)
    if (choice == 0) {
      if (item && item->action) item->action();
      os << "Выход из программы.\n";
      break;
    }

    if (item && item->action) {
      os << "\n";
      item->action();
    } else {
      os << "Неверный пункт меню! Попробуйте снова.\n";
    }
  }
}

} // namespace menu