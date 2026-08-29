# Local patches

Локальные правки, не входящие в апстрим FreeRDP. Применять после клонирования
или обновления дерева:

    git apply patches/0001-wfreerdp-borderless-window.patch

Проверить, применится ли, не трогая файлы:

    git apply --check patches/0001-wfreerdp-borderless-window.patch

Снять:

    git apply --reverse patches/0001-wfreerdp-borderless-window.patch

## 0001-wfreerdp-borderless-window.patch

Чинит `-decorations` в нативном windows-клиенте (`wfreerdp`). Клиент нужен
вместо SDL-клиента потому, что только в нём работает копирование файлов через
буфер обмена (`wf_cliprdr.c`, OLE IDataObject/IStream); в SDL-клиенте файловый
буфер реализован лишь для linux через FUSE.

Три проблемы, все в окне без декораций:

1. `wf_client.c` — стиль окна был захардкожен как `WS_CHILD | WS_BORDER`.
   Без родительского окна (`/parent-window` не задан) `CreateWindowEx`
   падает с `ERROR_TLW_WITH_WSCHILD` (1406) и клиент не стартует вообще.
   Заменено на `WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX` —
   тот же набор, что использует SDL (`STYLE_BORDERLESS_WINDOWED` в
   `src/video/windows/SDL_windowswindow.c`). `WS_CAPTION` и `WS_SYSMENU`
   нужны, чтобы у окна осталось системное меню (пункт Move) и нормальное
   поведение панели задач.

2. `wf_event.c` — добавлен обработчик `WM_NCCALCSIZE`, сообщающий нулевую
   неклиентскую область. Иначе `WS_CAPTION` из п.1 нарисовал бы заголовок.
   Так же поступает SDL (`SDL_windowsevents.c`). Ветка с `SW_MAXIMIZE`
   не даёт развёрнутому окну перекрыть панель задач.

3. `wf_event.c` — низкоуровневый клавиатурный хук `wf_ll_kbd_proc`
   перехватывает все клавиши и шлёт их в RDP-сессию, возвращая 1. Из-за
   этого модальный цикл Move (он управляется стрелками и enter/escape)
   не получал ни одного нажатия. Флаг `g_sizemove`, выставляемый по
   `WM_ENTERSIZEMOVE` и снимаемый по `WM_EXITSIZEMOVE`, на время цикла
   пропускает клавиатуру в систему.

4. `wf_gdi.c` — тот же `WS_CHILD` при выходе из полноэкранного режима,
   исправлен аналогично п.1.

Собирать: `cmake --build build --target wfreerdp`, затем скопировать
`build/client/Windows/wfreerdp-client3.dll` и
`build/client/Windows/cli/wfreerdp.exe` туда, откуда запускается клиент.

## 0002-wfreerdp-remember-window-position.patch

Накладывать после 0001. Даёт `wfreerdp` возможность запоминать положение
окна между запусками, чтобы не заворачивать его в PowerShell-обёртку.

1. `wf_gdi.c` — в ветке окна без декораций учитываются `DesktopPosX/PosY`,
   то есть штатный ключ `/window-position:<x>x<y>` наконец работает и с
   `-decorations`. Раньше координаты читались только в ветке окна с
   декорациями, а здесь стояло жёсткое `SetWindowPos(..., 0, 0, ...)`.

2. `wfreerdp.c` — новый ключ `+window-remember`: при старте позиция
   читается из файла и кладётся в `DesktopPosX/PosY`, при выходе
   сохраняется из `wfc->client_x/client_y`. Явный `/window-position`
   имеет приоритет над сохранённым. Опция объявлена локально в клиенте
   через `freerdp_client_settings_parse_command_line_ex`, общий парсер
   `client/common/cmdline.h` не затронут.

Файл с позицией — `wfreerdp-window-position` в каталоге конфигурации
FreeRDP (`FreeRDP_ConfigPath`, рядом с `known_hosts`), две координаты
через пробел. Отрицательные значения не сохраняются: свёрнутое окно
отдаёт -32000. Верхняя граница 65535 — та же, что у `/window-position`.

## 0003-win32-clipboard-shared-and-sdl-window-position.patch

Накладывать после 0002. Две связанные вещи.

**Копирование файлов через буфер обмена в SDL-клиенте.** Реализация из
`client/Windows/wf_cliprdr.c` перенесена в `client/common/client_cliprdr_win32.c`
и стала частью `freerdp-client`, публичный заголовок —
`include/freerdp/client/client_cliprdr_win32.h`. Перенос возможен потому, что
файл ни от чего в windows-клиенте не зависел: он поднимает собственное
скрытое окно и собственный поток, а наружу общается только через
`CliprdrClientContext`; единственное поле `wfc` присваивалось и никогда не
читалось. Логика внутри файла не изменена, только сигнатуры двух публичных
функций:

    wf_cliprdr_init(wfContext*, CliprdrClientContext*)
      -> cliprdr_win32_context_new(rdpContext*, CliprdrClientContext*)
    wf_cliprdr_uninit(wfContext*, CliprdrClientContext*)
      -> cliprdr_win32_context_free(CliprdrWin32Context*, CliprdrClientContext*)

SDL-клиент на Windows использует этот контекст вместо `sdlClip`
(`sdl_channels.cpp`). Конфликта за владение буфером нет: `sdlClip` там не
инициализируется, `SDL_SetClipboardData` не вызывается. На linux и macOS
всё как было. Тег журнала сменился на `com.freerdp.client.win32.cliprdr`.

**Позиция окна в SDL-клиенте.** `SdlWindow::create()` принимает
необязательную позицию, она подставляется при создании окна. Ключ
`+window-remember` объявлен локально в `_args` рядом с
`sdl-allow-screensaver`, имя совпадает с одноимённым ключом wfreerdp из
патча 0002. Восстановление — в `createWindows()`, сохранение — в
`SdlContext::cleanup()`. Файл `sdl-freerdp-window-position` в
`FreeRDP_ConfigPath`, отдельный от файла wfreerdp. При `/multimon`
позиция не применяется.

## 0004-args-from-file-comments.patch

Накладывать после 0003. Позволяет держать длинную командную строку в
файле вместе с пояснениями.

`/args-from:file:<path>` читал файл построчно, считая каждую строку
аргументом: комментарии были невозможны, а пустая строка обрывала
чтение. Теперь `args_from_fp()` принимает флаг `comments`: строки,
начинающиеся с '#', пропускаются, пустые строки не прерывают разбор, а
просто игнорируются.

Флаг включён только для `file:`. Для `stdin` и `fd:` поведение прежнее
намеренно: там пустая строка служит признаком конца ввода.

Пример файла:

    # --- connection ---
    /v:192.168.108.1
    /u:user@example.com

    # --- window ---
    /w:2570
    /h:1440
    +window-remember

Запуск: `sdl-freerdp.exe /args-from:file:C:\path\client.args`
Ключ по-прежнему несовместим с любыми другими аргументами.
