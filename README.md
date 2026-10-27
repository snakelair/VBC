# VBC - Visual Basic Compiler with Linear Arena Architecture

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

**VBC** — высокопроизводительный системный язык программирования, сочетающий лаконичный и понятный синтаксис **Visual Basic** с архитектурой памяти на линейных аренах (линейные стеки данных / bump-allocators, отсутствие сборщика мусора, нулевая фрагментация кучи и компиляция в нативный C99/C11 код через Clang/GCC с оптимизациями `-O2`/`-O3`).

> [!NOTE]
> Компилятор VBC является **полностью самодостаточным (100% Self-Hosted Pure Native)**. Зависимость от платформы .NET / .NET SDK полностью устранена. Сборка из исходников на любой платформе (Windows, Linux, macOS) выполняется без внешних пакетов — достаточно установленного компилятора Clang или GCC.

---

## Ключевые возможности

1. **Модель памяти на линейных аренах (Linear Data Stacks / Bump Allocators)**:
   - Полное отсутствие сборщика мусора (Zero GC) и пауз stop-the-world.
   - Освобождение всей памяти, выделенной внутри функции или итерации цикла, за **1 машинную инструкцию** ($O(1)$).
   - In-place построение возвращаемых значений напрямую в стек вызывающей стороны.
2. **Синтаксис Visual Basic и Объектно-Ориентированное Программирование (OOP)**:
   - `Class`, `Inherits`, `Overridable`, `Overrides`, `MyBase`, `Me`, `Sub New`.
   - `Structure` (типы значений на стеке данных) с методами и конструкторами.
   - Динамический полиморфизм через таблицы виртуальных функций (**VTable**) в стандарте C99.
   - Автоматическое детерминированное управление ресурсами через оператор `Using ... End Using` (RAII).
3. **HTTP-библиотека (`vbc_http`)**:
   - **HttpClient**: `client.Get(url)`, `client.Post(url, body, contentType)`.
   - **HttpServer**: Высокопроизводительный HTTP 1.1 сервер с роутингом (`server.OnGet(path, AddressOf handler)`) и zero-allocation парсингом запросов.
4. **Инструменты разработчика, ИИ-интроспекция и IntelliSense**:
   - Быстрая проверка синтаксиса и AST без вызова Clang: `vbc --check <file.vbc>`.
   - Дамп лексем и токенов для отладки: `vbc --dump-tokens <file.vbc>`.
   - Визуализация дерева AST с отступами: `vbc --dump-ast <file.vbc>`.
   - Встроенный сервер протокола языкового сервера (**LSP 3.17**) в `vbc lsp`.
   - Встроенный нативный тест-раннер: `vbc test [samples_dir]`.
   - База знаний и руководство для ИИ-ассистентов: [AI_GUIDE.md](file:///c:/Projects%20My/VBC/AI_GUIDE.md) и `.cursorrules`.
   - Расширение для **Visual Studio Code** (`editors/vscode/vbc-language-support-1.0.0.vsix`).
   - Расширение для **Visual Studio 2022 / 2026 Enterprise** (`editors/visualstudio/VbcExtension.vsix`) на базе `ILanguageClient`.
5. **Чистый C99 Bootstrap и самохостинг**:
   - Автономный файл C99-бутстрапа `bootstrap/vbc_bootstrap.c`.
   - Скрипты сборки `build.bat` (Windows) и `build.sh` (Linux/macOS) для сборки компилятора за 3 секунды.

---

## Структура репозитория

```
c:\Projects My\VBC\
├── AI_GUIDE.md                     # Исчерпывающий гайд для ИИ (грамматика, модель памяти, AST)
├── .cursorrules                    # Системные правила для AI-ассистентов (Cursor/Windsurf/Copilot)
│
├── bootstrap/                      # Автономный C99 бутстрап (Zero-dependency)
│   └── vbc_bootstrap.c             # Сгенерированный C99 исходник компилятора
│
├── runtime/                        # Нативный C рантайм и BCL движок
│   ├── include/
│   │   ├── vbc_runtime.h           # Линейные стеки данных (Linear Arena), строки, срезы
│   │   ├── vbc_bcl.h               # Base Class Library (.NET 10 совместимые типы)
│   │   ├── vbc_socket.h            # Кроссплатформенные сокеты (Winsock2 / POSIX)
│   │   └── vbc_http.h              # Заголовки HTTP клиента и сервера
│   └── src/
│       ├── vbc_socket.c            # Реализация сокетов
│       ├── vbc_http.c              # Zero-allocation HTTP парсер и сервер
│       └── vbc_async.c             # Асинхронный рантайм
│
├── src/                            # Модуляризированный исходный код компилятора VBC
│   ├── Common.vbc                  # Строковые утилиты и хелперы
│   ├── Token.vbc                   # Лексические токены и таблица ключевых слов
│   ├── Ast.vbc                     # Arena AST ноды и документированные контракты полей
│   ├── Lexer.vbc                   # Лексический анализатор
│   ├── Parser.vbc                  # Синтаксический анализатор (Recursive Descent)
│   ├── EmitterCore.vbc             # Кодогенерация: символы, типы, временные переменные
│   ├── EmitterTypes.vbc            # Кодогенерация: структуры, классы, VTable, интерфейсы
│   ├── EmitterStmt.vbc             # Кодогенерация: операторы, циклы, Using, Try/Catch
│   ├── EmitterExpr.vbc             # Кодогенерация: выражения, литералы, вызовы, массивы
│   ├── Emitter.vbc                 # Главный оркестратор генерации C99 кода
│   ├── Driver.vbc                  # Вызов Clang / GCC драйвера
│   ├── Lsp.vbc                     # LSP 3.17 сервер
│   └── Program.vbc                 # CLI точка входа, ИИ-интроспекция и Test Runner
│
├── editors/
│   ├── vscode/                     # Расширение для VS Code (.vsix)
│   └── visualstudio/               # Расширение для Visual Studio 2022/2026 (.vsix)
│
├── samples/                        # 20 образцов и комплексных тестов языка VBC
│   ├── 01_HelloWorld/              # Базовая программа
│   ├── 02_MemoryEfficiency/        # Бенчмарк: 1,000,000 аллокаций без GC
│   ├── 12_ClassesAndOop/           # ООП: Классы, наследование, VTable, Using (RAII)
│   ├── 13_NamespacesAndConcurrency/# Пространства имен, SyncLock, Dictionary
│   ├── 14_BitwiseAndHex/           # Битовые сдвиги, маски, шестнадцатеричные литералы
│   ├── 15_DotNet10Namespaces/      # Криптография (SHA256, MD5), Base64, GZip, Watcher
│   └── ...                         # 16_Bcl, 17_Xml, 18_Interfaces, 19_Nullable
│
├── build.bat                       # Сборка и самохостинг vbc.exe (Clang/GCC)
└── build.sh                        # Сборка ./vbc на Linux/macOS
```

---

## Сборка компилятора из исходников

Для сборки компилятора **не требуется .NET SDK**. Достаточно компилятора Clang или GCC.

### На Windows:
```cmd
build.bat
```

### На Linux / macOS:
```bash
chmod +x build.sh
./build.sh
```

### Самокомпиляция (Self-Hosting Rebuild):
```cmd
build.bat rebuild
```

---

## Быстрый старт

### Запуск и тестирование программ на VBC:
```powershell
# Запуск примера в один шаг
vbc run samples/01_HelloWorld/Program.vbc

# Компиляция в нативный .exe
vbc compile samples/12_ClassesAndOop/Program.vbc -o oop_demo.exe

# Запуск встроенного нативного тест-раннера
vbc test samples
```

### Пример ООП и детерминированного освобождения ресурсов:
```vb
Class Animal
    Public Name As String

    Public Sub New(name As String)
        Me.Name = name
    End Sub

    Public Overridable Function Speak() As String
        Return "..."
    End Function
End Class

Class Dog Inherits Animal
    Public Breed As String

    Public Sub New(name As String, breed As String)
        MyBase.New(name)
        Me.Breed = breed
    End Sub

    Public Overrides Function Speak() As String
        Return "Woof! I am " & Breed
    End Function
End Class

Module Program
    Sub Announce(animal As Animal)
        Console.WriteLine(animal.Name & " says: " & animal.Speak())
    End Sub

    Sub Main()
        Dim dog = New Dog("Rex", "German Shepherd")
        Announce(dog) ' Динамический полиморфизм через VTable
    End Sub
End Module
```

---

## Установка расширений редактора

- **Visual Studio Code**:
  В VS Code перейдите в меню Extensions (`Ctrl+Shift+X`), выберите `... -> Install from VSIX...` и укажите файл:
  `c:\Projects My\VBC\editors\vscode\vbc-language-support-1.0.0.vsix`
- **Visual Studio 2022 / 2026**:
  Дважды кликните по файлу:
  `c:\Projects My\VBC\editors\visualstudio\VbcExtension.vsix`
  и подтвердите установку в окне VSIX Installer.

---

## Лицензия

Проект распространяется под открытой лицензией [MIT](LICENSE).
Copyright (c) 2026 Евгений Пащенко.

