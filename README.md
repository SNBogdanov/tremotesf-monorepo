# Tremotesf (monorepo)

Объединённый проект из двух форков [equeim](https://github.com/equeim):

| Каталог | Проект | Стек | Оригинал |
| ------- | ------ | ---- | -------- |
| [`desktop/`](desktop/) | Tremotesf для рабочего стола | Qt 6 / C++ / QML, CMake + vcpkg | [equeim/tremotesf2](https://github.com/equeim/tremotesf2) |
| [`android/`](android/) | Tremotesf для Android | Kotlin, Jetpack Compose, Gradle | [equeim/tremotesf-android](https://github.com/equeim/tremotesf-android) |

Оба приложения — клиенты к Transmission RPC API. Код на разных языках, общего
кода между ними нет; монорепозиторий нужен для совместной работы над обоими
клиентами и единой истории изменений интерфейса/протокола.

## Git

- Истории обоих проектов сохранены полностью: каждый каталог получен через
  subtree-merge из соответствующего репозитория.
- Для синхронизации с апстримами используйте отдельные remote-репозитории,
  которые были переименованы при объединении:
  - бывший `desktop` → `upstream-desktop`
  - бывший `android` → `upstream-android`

```bash
git fetch upstream-desktop master
git merge -s ours --no-commit upstream-desktop/master
git read-tree --prefix=desktop/ -u upstream-desktop/master
git commit -m "Update desktop/ from upstream"
```

## Сабмодули

Оба исходных проекта использовали сабмодули по своим путям. При объединении
их описания перенесены в корневой `.gitmodules`:

```bash
git submodule update --init --recursive
```

- `desktop/src/3rdparty/cpp-httplib` — [yhirose/cpp-httplib](https://github.com/yhirose/cpp-httplib)
- `android/bencode` — [equeim/kotlinx-serialization-bencode](https://github.com/equeim/kotlinx-serialization-bencode)

## Лицензии

Исходные проекты распространяются под GPL-3.0; тексты лицензий и
SPDX-заголовки сохранены в `desktop/` и `android/` без изменений.
