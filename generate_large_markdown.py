#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Скрипт генерации большого Markdown-файла (по умолчанию 16 ГБ) с валидным русским текстом в кодировке UTF-8.
Использует блочную буферизованную запись для максимальной скорости генерации (обычно 1-2 ГБ/с на SSD).
"""

import sys
import time
import argparse
from pathlib import Path

# Включаем корректный вывод UTF-8 в терминал Windows
if hasattr(sys.stdout, "reconfigure"):
    try:
        sys.stdout.reconfigure(encoding="utf-8")
    except Exception:
        pass


def build_markdown_chunk() -> bytes:
    """
    Генерирует разнообразный блок Markdown с заголовками, списками и абзацами на русском языке.
    Возвращает предварительно закодированные байты в UTF-8.
    """
    sections = [
        """
# Глава: Анализ регулярных выражений и обработка текстов

В данной главе рассматриваются теоретические и практические аспекты построения синтаксических анализаторов,
обработки файлов разметки Markdown, а также нормализации пользовательских анкетных данных.
Регулярные выражения представляют собой формальный аппарат теории автоматов и формальных языков,
позволяющий с высокой точностью осуществлять сопоставление с шаблоном (pattern matching).

## Свойства разметки Markdown и правила синтаксиса

Синтаксис Markdown был разработан Джоном Грубером с целью сделать разметку документов
максимально естественной и удобной для чтения человеком даже в исходном текстовом виде.

Основные правила структурирования документа:
- Заголовки обозначаются символами решётки от первого до шестого уровня;
- Абзацы отделяются друг от друга одной или несколькими пустыми строками;
- Списки могут быть маркированными (маркеры: дефис, звездочка или плюс) или нумерованными;
- Блоки исходного кода обрамляются тройными обратными апострофами;
- Внутристрочные ссылки оформляются с использованием квадратных и круглых скобок.

1. Этап лексического анализа: построчный разбор входного потока символов.
2. Этап валидации структуры: отслеживание состояния вложенности блоков кода.
3. Этап извлечения статистики: наполнение контейнеров STL (std::vector, std::map).
4. Этап нормализации: приведение телефонных номеров, дат и почтовых адресов к стандарту.

### Пример кода обработки регулярных выражений на C++

```cpp
#include <iostream>
#include <regex>
#include <string>

bool validateHeader(const std::string& line) {
    static const std::regex headerRegex(R"(^(#{1,6})\\s+(.*))");
    return std::regex_match(line, headerRegex);
}
```

## Алгоритмы нормализации пользовательских анкет

При обработке пользовательских анкет часто встречаются опечатки, различный регистр символов
и вариации разделителей в номерах телефонов и датах рождения.
Применение регулярных выражений позволяет:
- Удалять посторонние шумовые символы и цифры из фамилий, имён и отчеств;
- Преобразовывать дату в строгий стандарт ДД.ММ.ГГГГ;
- Приводить мобильные номера к единому международному формату E.164 (+7XXXXXXXXXX);
- Валидировать синтаксическую корректность доменного имени адреса электронной почты.

- Фамилия: Сидоров
- Имя: Иван
- Отчество: Петрович
- Дата рождения: 12.05.1995
- Телефон: +79161234567
- Email: ivan.sidorov@example.com

Обработка больших объемов данных требует эффективного использования потоков ввода-вывода
и исключения лишних операций выделения динамической памяти при разборе строк.
"""
    ]

    combined_text = "\n".join(sections).strip() + "\n\n"
    # Повторяем секцию для создания блока размером ~1-2 МБ в памяти
    target_block_size = 2 * 1024 * 1024  # 2 МБ
    encoded_unit = combined_text.encode("utf-8")
    repeat_count = max(1, target_block_size // len(encoded_unit))

    return encoded_unit * repeat_count


def generate_markdown_file(output_path: Path, target_bytes: int):
    """
    Записывает файл заданного размера чанками для максимальной скорости.
    """
    chunk = build_markdown_chunk()
    chunk_size = len(chunk)
    bytes_written = 0

    print(f"Целевой размер: {target_bytes / (1024 ** 3):.2f} ГБ ({target_bytes:,} байт)")
    print(f"Выходной файл:   {output_path.resolve()}")
    print(f"Кодировка:      UTF-8 (без BOM)")
    print("Генерация начата...\n")

    start_time = time.time()
    last_report_time = start_time

    # Заголовочная часть файла
    header = (
        "# Большой набор тестовых данных Markdown (16 ГБ)\n\n"
        "> Данный файл сгенерирован автоматически для нагрузочного тестирования парсера.\n"
        "> Кодировка: UTF-8. Содержит заголовки, списки, абзацы и блоки кода на русском языке.\n\n"
    ).encode("utf-8")

    # Буфер записи 16 МБ для высокой скорости ввода-вывода
    with open(output_path, "wb", buffering=16 * 1024 * 1024) as f:
        f.write(header)
        bytes_written += len(header)

        while bytes_written < target_bytes:
            remaining = target_bytes - bytes_written
            to_write = chunk if remaining >= chunk_size else chunk[:remaining]
            f.write(to_write)
            bytes_written += len(to_write)

            now = time.time()
            if now - last_report_time >= 1.0 or bytes_written >= target_bytes:
                elapsed = now - start_time
                progress = (bytes_written / target_bytes) * 100
                speed_mb_s = (bytes_written / (1024 * 1024)) / elapsed if elapsed > 0 else 0
                gb_written = bytes_written / (1024 ** 3)
                total_gb = target_bytes / (1024 ** 3)

                sys.stdout.write(
                    f"\rПрогресс: {progress:5.1f}% | "
                    f"Записано: {gb_written:5.2f} / {total_gb:.2f} ГБ | "
                    f"Скорость: {speed_mb_s:6.1f} МБ/с | "
                    f"Прошло: {elapsed:.1f} сек"
                )
                sys.stdout.flush()
                last_report_time = now

    total_time = time.time() - start_time
    print(f"\n\nУспешно завершено за {total_time:.1f} сек!")
    print(f"Итоговый размер: {output_path.stat().st_size / (1024 ** 3):.2f} ГБ")


def main():
    parser = argparse.ArgumentParser(
        description="Генератор тестового Markdown-файла большого объёма (по умолчанию 16 ГБ)"
    )
    parser.add_argument(
        "-o",
        "--output",
        type=Path,
        default=Path("large_sample_16gb.md"),
        help="Имя или путь к выходному файлу (по умолчанию: large_sample_16gb.md)",
    )
    parser.add_argument(
        "-s",
        "--size-gb",
        type=float,
        default=16.0,
        help="Размер файла в гигабайтах (по умолчанию: 16.0)",
    )

    args = parser.parse_args()
    target_bytes = int(args.size_gb * 1024 * 1024 * 1024)

    try:
        generate_markdown_file(args.output, target_bytes)
    except KeyboardInterrupt:
        print("\nПрервано пользователем.")
    except Exception as e:
        print(f"\nОшибка при генерации: {e}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
