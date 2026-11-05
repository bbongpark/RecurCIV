@echo off
rem Run from a Visual Studio x64 Native Tools command prompt, after python build.py.
cd /d "%~dp0.."
py tests\test_runner.py
if not "%errorlevel%"=="0" exit /b 1
cl /nologo /std:c11 /utf-8 /O2 /W3 /D_CRT_SECURE_NO_WARNINGS /Iinclude /Ithird_party /Ibuild /Febuild\test_text.exe /Fobuild\ tests\test_text.c source\text.c source\view.c source\ui.c source\controller_monitor.c source\live_keyboard.c
if not "%errorlevel%"=="0" exit /b 1
build\test_text.exe
if not "%errorlevel%"=="0" exit /b 1
cl /nologo /std:c11 /utf-8 /O2 /W3 /D_CRT_SECURE_NO_WARNINGS /Iinclude /Febuild\test_autoindent.exe /Fobuild\ tests\test_autoindent.c source\text.c
if not "%errorlevel%"=="0" exit /b 1
build\test_autoindent.exe
if not "%errorlevel%"=="0" exit /b 1
cl /nologo /std:c11 /utf-8 /O2 /W3 /Iinclude /Febuild\test_typer.exe /Fobuild\ tests\test_typer.c source\typer.c
if not "%errorlevel%"=="0" exit /b 1
build\test_typer.exe
if not "%errorlevel%"=="0" exit /b 1
cl /nologo /std:c11 /utf-8 /O2 /W3 /WX /D_CRT_SECURE_NO_WARNINGS /Iinclude /Ibuild /Febuild\test_civ_runner.exe /Fobuild\ tests\test_civ_runner.c source\civ_runner.c source\recurciv_payload.c source\text.c
if not "%errorlevel%"=="0" exit /b 1
build\test_civ_runner.exe
if not "%errorlevel%"=="0" exit /b 1
py tests\test_civ_runner.py
if not "%errorlevel%"=="0" exit /b 1
cl /nologo /std:c11 /utf-8 /O2 /W3 /WX /D_CRT_SECURE_NO_WARNINGS /Itests\mock /Iinclude /Ithird_party /Ibuild /Febuild\test_civ_usb.exe /Fobuild\ tests\test_civ_usb.c source\civ_usb.c source\civ_runner.c source\recurciv_payload.c source\usb_keyboard.c source\controller_route.c source\controller_mode.c source\controller_monitor.c source\view.c source\ui.c source\text.c source\live_keyboard.c
if not "%errorlevel%"=="0" exit /b 1
build\test_civ_usb.exe
if not "%errorlevel%"=="0" exit /b 1
py tests\test_civ_usb.py
if not "%errorlevel%"=="0" exit /b 1
cl /nologo /std:c11 /utf-8 /O2 /W3 /WX /D_CRT_SECURE_NO_WARNINGS /Itests\mock /Iinclude /Ithird_party /Ibuild /Febuild\test_civ_ui.exe /Fobuild\ tests\test_civ_ui.c source\civ_route.c source\civ_sleep.c source\civ_usb.c source\civ_runner.c source\recurciv_payload.c source\usb_keyboard.c source\controller_route.c source\controller_mode.c source\controller_monitor.c source\view.c source\ui.c source\text.c source\live_keyboard.c
if not "%errorlevel%"=="0" exit /b 1
build\test_civ_ui.exe
if not "%errorlevel%"=="0" exit /b 1
cl /nologo /std:c11 /utf-8 /O2 /W3 /WX /Itests\mock /Iinclude /Febuild\test_civ_sleep.exe /Fobuild\ tests\test_civ_sleep.c source\civ_sleep.c
if not "%errorlevel%"=="0" exit /b 1
build\test_civ_sleep.exe
if not "%errorlevel%"=="0" exit /b 1
cl /nologo /std:c11 /utf-8 /O2 /W3 /WX /D_CRT_SECURE_NO_WARNINGS /Itests\mock /Iinclude /Ithird_party /Ibuild /Febuild\test_keyboard_ui.exe /Fobuild\ tests\test_keyboard_ui.c source\controller_route.c source\controller_mode.c source\controller_monitor.c source\view.c source\ui.c source\text.c source\live_keyboard.c
if not "%errorlevel%"=="0" exit /b 1
build\test_keyboard_ui.exe
if not "%errorlevel%"=="0" exit /b 1
cl /nologo /std:c11 /utf-8 /O2 /W3 /D_CRT_SECURE_NO_WARNINGS /Itests\mock /Iinclude /Ithird_party /Ibuild /Febuild\test_usb.exe /Fobuild\ tests\test_usb.c source\usb_keyboard.c source\controller_route.c source\controller_mode.c source\controller_monitor.c source\view.c source\ui.c source\text.c source\live_keyboard.c
if not "%errorlevel%"=="0" exit /b 1
build\test_usb.exe
if not "%errorlevel%"=="0" exit /b 1
cl /nologo /std:c11 /utf-8 /O2 /W3 /Iinclude /Febuild\test_controller.exe /Fobuild\ tests\test_controller.c source\controller_monitor.c
if not "%errorlevel%"=="0" exit /b 1
build\test_controller.exe
if not "%errorlevel%"=="0" exit /b 1
cl /nologo /std:c11 /utf-8 /O2 /W3 /Itests\mock /Iinclude /Febuild\test_controller_mode.exe /Fobuild\ tests\test_controller_mode.c source\controller_mode.c source\controller_monitor.c
if not "%errorlevel%"=="0" exit /b 1
build\test_controller_mode.exe
if not "%errorlevel%"=="0" exit /b 1
cl /nologo /std:c11 /utf-8 /O2 /W3 /D_CRT_SECURE_NO_WARNINGS /Itests\mock /Iinclude /Ithird_party /Ibuild /Febuild\test_controller_route.exe /Fobuild\ tests\test_controller_route.c source\controller_route.c source\controller_mode.c source\controller_monitor.c source\view.c source\ui.c source\text.c source\live_keyboard.c
if not "%errorlevel%"=="0" exit /b 1
build\test_controller_route.exe
if not "%errorlevel%"=="0" exit /b 1
cl /nologo /std:c11 /utf-8 /O2 /W3 /Iinclude /Febuild\test_macro_model.exe /Fobuild\ tests\test_macro_model.c source\macro_model.c
if not "%errorlevel%"=="0" exit /b 1
build\test_macro_model.exe
if not "%errorlevel%"=="0" exit /b 1
cl /nologo /std:c11 /utf-8 /O2 /W3 /D_CRT_SECURE_NO_WARNINGS /Iinclude /Febuild\test_macro_editor.exe /Fobuild\ tests\test_macro_editor.c source\macro_model.c source\macro_storage.c source\macro_editor.c
if not "%errorlevel%"=="0" exit /b 1
build\test_macro_editor.exe
if not "%errorlevel%"=="0" exit /b 1
cl /nologo /std:c11 /utf-8 /O2 /W3 /WX /D_CRT_SECURE_NO_WARNINGS /Itests\mock /Iinclude /Febuild\test_macro_runtime.exe /Fobuild\ tests\test_macro_runtime.c source\macro_runtime.c source\macro_model.c source\macro_storage.c
if not "%errorlevel%"=="0" exit /b 1
build\test_macro_runtime.exe
if not "%errorlevel%"=="0" exit /b 1
cl /nologo /std:c11 /utf-8 /O2 /W3 /WX /D_CRT_SECURE_NO_WARNINGS /Itests\mock /Iinclude /Ithird_party /Ibuild /Febuild\test_macro_app.exe /Fobuild\ tests\test_macro_app.c source\macro_app.c source\macro_runtime.c source\macro_editor.c source\macro_model.c source\macro_storage.c source\macro_files.c source\usb_keyboard.c source\controller_route.c source\controller_mode.c source\controller_monitor.c source\view.c source\ui.c source\text.c source\live_keyboard.c
if not "%errorlevel%"=="0" exit /b 1
build\test_macro_app.exe
if not "%errorlevel%"=="0" exit /b 1
cl /nologo /std:c11 /utf-8 /O2 /W3 /D_CRT_SECURE_NO_WARNINGS /DMACRO_STORAGE_TESTING /Iinclude /Febuild\test_macro_storage.exe /Fobuild\ tests\test_macro_storage.c source\macro_model.c source\macro_storage.c source\macro_files.c
if not "%errorlevel%"=="0" exit /b 1
build\test_macro_storage.exe
if not "%errorlevel%"=="0" exit /b 1
cl /nologo /std:c11 /utf-8 /O2 /W3 /Iinclude /Febuild\test_compose.exe /Fobuild\ tests\test_compose.c source\compose.c source\text.c source\typer.c
if not "%errorlevel%"=="0" exit /b 1
build\test_compose.exe
if not "%errorlevel%"=="0" exit /b 1
cl /nologo /std:c11 /utf-8 /O2 /W3 /Iinclude /Febuild\test_live_keyboard.exe /Fobuild\ tests\test_live_keyboard.c source\live_keyboard.c source\text.c
if not "%errorlevel%"=="0" exit /b 1
build\test_live_keyboard.exe
if not "%errorlevel%"=="0" exit /b 1
cl /nologo /std:c11 /utf-8 /O2 /W3 /D_CRT_SECURE_NO_WARNINGS /Iinclude /Ithird_party /Ibuild /Febuild\test_live_visual.exe /Fobuild\ tests\test_live_visual.c source\text.c source\view.c source\ui.c source\controller_monitor.c source\live_keyboard.c
if not "%errorlevel%"=="0" exit /b 1
build\test_live_visual.exe
if not "%errorlevel%"=="0" exit /b 1
cl /nologo /std:c11 /utf-8 /O2 /W3 /D_CRT_SECURE_NO_WARNINGS /Iinclude /Ithird_party /Ibuild /Febuild\test_macro_view.exe /Fobuild\ tests\test_macro_view.c source\text.c source\view.c source\ui.c source\controller_monitor.c source\live_keyboard.c
if not "%errorlevel%"=="0" exit /b 1
build\test_macro_view.exe
if not "%errorlevel%"=="0" exit /b 1
cl /nologo /std:c11 /utf-8 /O2 /W3 /WX /D_CRT_SECURE_NO_WARNINGS /Itests\mock /Iinclude /Ithird_party /Ibuild /Febuild\test_ui_recovery.exe /Fobuild\ tests\test_ui_recovery.c source\macro_app.c source\macro_runtime.c source\macro_editor.c source\macro_model.c source\macro_storage.c source\macro_files.c source\controller_route.c source\controller_mode.c source\controller_monitor.c source\view.c source\ui.c source\text.c source\live_keyboard.c
if not "%errorlevel%"=="0" exit /b 1
build\test_ui_recovery.exe
if not "%errorlevel%"=="0" exit /b 1
cl /nologo /std:c11 /utf-8 /O2 /W3 /D_CRT_SECURE_NO_WARNINGS /Iinclude /Ithird_party /Ibuild /Febuild\preview.exe /Fobuild\ tests\preview.c source\text.c source\view.c source\ui.c source\controller_monitor.c source\live_keyboard.c
if not "%errorlevel%"=="0" exit /b 1
cd build
preview.exe
if not "%errorlevel%"=="0" exit /b 1
