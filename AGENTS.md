# Tremotesf fork — правила работы

Проект: `C:\Users\SERGEYBO\Documents\Projects\tremotesf` (desktop + Android).
Репозиторий: `https://github.com/SNBogdanov/tremotesf-monorepo`.
Ветка для работы и релизов: `feature/upstream-base`.
Android-устройство: SM-N975F (`RF8N30P85JJ`), adb `adb-RF8N30P85JJ-gTxt6H._adb-tls-connect._tcp`.

## Триггер «сборка release»

Когда пользователь говорит **«сборка release»**, выполняется полный цикл:

1. **Поднять patch-версию на 1** (например, 3.0.2 → 3.0.3):
   - `desktop/CMakeLists.txt`: `project(tremotesf VERSION X.Y.Z ...)`;
   - `android/app/build.gradle.kts`: `versionName = "X.Y.Z"`,
     `versionCode = major * 10000 + minor * 100 + patch`
     (3.0.3 → 30003);
   - добавить запись релиза в `desktop/data/org.equeim.Tremotesf.appdata.xml.in`.
2. Закоммитить и запушить изменения через субагента `git-commit`
   (не force, не amend, без секретов и артефактов).
3. Собрать последовательно, через глобальный `build-lock.ps1`:
   - desktop MSI `tremotesf-X.Y.Z-Windows-x86_64-Release.msi`;
   - Android `googleRelease` APK, подписанный постоянным release-ключом.
4. Создать аннотированный тег `vX.Y.Z` на релизном коммите и запушить его.
5. Опубликовать GitHub Release (stable, не prerelease) с assets:
   - MSI;
   - APK (`tremotesf-X.Y.Z-android-google.apk`);
   - `update.json` (version, windows URL/SHA-256/size, android versionCode/URL/SHA-256/size);
   - `SHA256SUMS`.
6. Проверить, что `releases/latest/download/update.json` отдаёт новую версию.
7. Скопировать установочные файлы релиза в `Y:\Install`:
   - `tremotesf-X.Y.Z-Windows-x86_64-Release.msi`;
   - `tremotesf-X.Y.Z-android-google.apk`.
   Если `Y:` недоступен — сообщить об этом, не считать релиз полностью завершённым.

## После релиза

- До следующего «сборка release» версия **не меняется**.
- Все отладочные/промежуточные сборки Android идут с этой же версией
  (`versionName`/`versionCode` релиза).
- Debug-сборки подписываются постоянным release-ключом
  (`android/keystore.properties`, `keystore-release.jks`), имеют тот же
  `applicationId`, поэтому ставятся **поверх** установленной версии на Note
  без удаления и без потери данных.
- Установка на Note:
  `adb install -r <apk>` (при равном versionCode; при меньшем — `-d`).
- Debug APK с release-ключом не публиковать и не загружать в GitHub Releases.

## Release-подпись Android

- Keystore: `android/keystore-release.jks` (gitignored).
- Параметры: `android/keystore.properties` (gitignored).
- Сертификат: `CN=Tremotesf, OU=Android, O=SNBogdanov, C=RU`,
  SHA-256 `D8:F8:EB:F7:C0:99:1A:68:64:64:C6:07:EC:D5:76:9E:28:16:DE:FC:63:A5:D9:E5:DC:58:29:88:08:7D:E3:D5`.
- Бэкап ключа: `C:\Users\SERGEYBO\Documents\Projects\_opencode_tmp\tremotesf-release-keystore`.
- Ключ и пароли не выводить в чат и логи.

## Windows: PsExec и SAMSUNG-PC

- Удалённая машина: `samsung-pc` / `SAMSUNG-PC`.
- Учётка: `MicrosoftAccount\snbogdanov1968@gmail.com`; пароль брать из
  `_netrc` (`machine samsung-smb`), в чат и логи не выводить.
- PsExec: `C:\Users\SERGEYBO\Downloads\SysinternalsSuite\PsExec64.exe`.
- **`PsExec -c` копирует и запускает только программу (exe/cmd), но не
  MSI-файл.** Команда
  `psexec \\samsung-pc -s -c package.msi msiexec /i package.msi /qn`
  не работает: PsExec пытается запустить сам MSI (exit code 2).
- Рабочая схема для MSI:
  1. Скопировать MSI в `\\samsung-pc\c$\Windows\Temp\` через admin share
     (`New-PSDrive -Root \\samsung-pc\c$ -Credential <samsung-smb>`).
  2. Запустить:
     `PsExec64.exe \\samsung-pc -u "MicrosoftAccount\snbogdanov1968@gmail.com" -p <pass> -h msiexec.exe /i C:\Windows\Temp\package.msi /qn /norestart`.
- Проверено 27.11.2026:
  - административная установка (`msiexec /a`) — exit 0;
  - тихая установка (`msiexec /i ... /qn`) — exit 0, версия 3.0.3,
    установлено в `C:\Program Files\Tremotesf`.
- Из-под `LOCAL SERVICE` локальный `msiexec` возвращает 1601; для локальной
  машины нужен elevated-скрипт пользователя.

## Общие ограничения

- Работать только в этом проекте; другие проекты — read-only.
- Одна сборка одновременно; desktop и Android — строго последовательно.
- Секреты, keystore, `_opencode_tmp`, `out/build`, `.opencode/` не коммитить.
