#include <Windows.h>
#include <filesystem>

namespace fileDialog {

/// @brief Функция вызова диалогового окна выбора файла
/// @return Путь к выбранному файлу
inline std::filesystem::path OpenFileDialog() {
  OPENFILENAMEA ofn; /// Структура для хранения параметров диалогового окна
  char szFile[MAX_PATH] = {0}; /// Массив для хранения пути к файлу

  ZeroMemory(&ofn, sizeof(ofn)); /// Обнуление структуры
  ofn.lStructSize = sizeof(ofn); /// Размер структуры
  ofn.hwndOwner = NULL;          /// Владелец окна
  ofn.lpstrFile = szFile;        /// Пусть к файлу
  ofn.nMaxFile = sizeof(szFile); /// Максимальный размер пути
  ofn.lpstrFilter =
      "Text Files (*.txt)\0*.txt\0All Files (*.*)\0*.*\0"; /// Фильтр
                                                           /// показываемых
                                                           /// файлов
  ofn.nFilterIndex = 2;           /// Индекс фильтра (1 = .txt)
  ofn.Flags = OFN_PATHMUSTEXIST   /// Путь должен существовать
              | OFN_FILEMUSTEXIST /// Файл должен существовать
              | OFN_NOCHANGEDIR;  /// Не менять текущую директорию

  if (GetOpenFileNameA(&ofn)) {
    return std::filesystem::path(szFile);
  }
  return {};
}
} // namespace fileDialog