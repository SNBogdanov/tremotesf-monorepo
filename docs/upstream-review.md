# Проверка upstream — 2026-11-26

## Что проверялось

- Desktop: `upstream-desktop/master` = `da452e88b31651d92b95c5a8b85033e2f81f4790`
- База форка (merge-base): `43b2637ad8eaff4aacfb906cdf3b03ca39e01d3f` (2026-11-10,
  «Fix out of bound access when torrent change its file count»)
- Android: `upstream-android/master` = `6f8590ccb38c4948ef76a8cfcc806a94b07c66bd`
  (совпадает с merge-base, новых изменений нет)

## Desktop: изменения после базы и решения

| Коммит | Изменение | Решение |
|---|---|---|
| `a6d9adb9` | Удалён флаг `mLoaded` из `TorrentFilesModel`; логика построения/сброса дерева упрощена | **Принято** |
| `cde969df` | Отмена незавершённого построения дерева при новом обновлении; удалён `mCreatingTree`; `beginResetModel()` после фактического построения | **Принято** |
| `f7f898c9` | `MainWindowStatusBar::mLayout`: `QLayout*` → `QHBoxLayout*` | **Принято** (адаптировано к форку) |
| `a05cbd22` | `widgetsAfter()` игнорирует не-виджетные элементы layout | **Принято** |
| `da452e88` | Исправлен устаревший комментарий про отступы статус-бара | **Принято** |
| `bd0d4196` | Обновление LLVM в Windows CI | **Не принято**: CI в форке не используется, на сборку/MSI не влияет |

Перенесённые изменения затронули:

- `desktop/src/ui/screens/torrentproperties/torrentfilesmodel.{h,cpp}`
- `desktop/src/ui/screens/mainwindow/mainwindowstatusbar.{h,cpp}`

Сборка и проверка после переноса на 2026-11-26 ещё не выполнялись.

## Android

`upstream-android/master` не изменился с `6f8590cc` (2026-08-19). Новых
коммитов для рассмотрения нет.
