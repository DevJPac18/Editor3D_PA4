#include "platform/FileDialog.h"
#include "core/Utils.h"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <commdlg.h>
#include <cwchar>

namespace FileDialog {

std::string open(void* hwnd, const wchar_t* filter, const wchar_t* title) {
    wchar_t buf[MAX_PATH * 2] = {0};
    OPENFILENAMEW ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = (HWND)hwnd;
    ofn.lpstrFilter = filter;
    ofn.lpstrFile = buf;
    ofn.nMaxFile = MAX_PATH * 2;
    ofn.lpstrTitle = title;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_HIDEREADONLY;
    if (GetOpenFileNameW(&ofn)) return util::normalizeSlashes(util::narrow(buf));
    return std::string();
}

std::string save(void* hwnd, const wchar_t* filter, const wchar_t* title, const wchar_t* defExt,
                 const std::string& initialName) {
    wchar_t buf[MAX_PATH * 2] = {0};
    std::wstring init = util::widen(initialName);
    if (init.size() < MAX_PATH) wcscpy(buf, init.c_str());
    OPENFILENAMEW ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = (HWND)hwnd;
    ofn.lpstrFilter = filter;
    ofn.lpstrFile = buf;
    ofn.nMaxFile = MAX_PATH * 2;
    ofn.lpstrTitle = title;
    ofn.lpstrDefExt = defExt;
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_HIDEREADONLY;
    if (GetSaveFileNameW(&ofn)) return util::normalizeSlashes(util::narrow(buf));
    return std::string();
}

bool confirm(void* hwnd, const std::string& message, const std::string& title) {
    return MessageBoxW((HWND)hwnd, util::widen(message).c_str(), util::widen(title).c_str(),
                       MB_YESNO | MB_ICONQUESTION) == IDYES;
}

void info(void* hwnd, const std::string& message, const std::string& title) {
    MessageBoxW((HWND)hwnd, util::widen(message).c_str(), util::widen(title).c_str(), MB_OK | MB_ICONINFORMATION);
}

} // namespace FileDialog
