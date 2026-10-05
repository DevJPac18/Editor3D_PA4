#include "core/Utils.h"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <cstdarg>
#include <algorithm>

namespace util {

std::string format(const char* fmt, ...) {
    char buf[2048];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    buf[sizeof(buf) - 1] = 0;
    return buf;
}

uint32_t hash(const char* s, uint32_t seed) {
    uint32_t h = 2166136261u ^ (seed * 16777619u + seed);
    while (*s) {
        h ^= (unsigned char)*s++;
        h *= 16777619u;
    }
    h ^= seed * 2654435761u;
    return h ? h : 1u;
}

std::wstring widen(const std::string& s) {
    if (s.empty()) return std::wstring();
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), NULL, 0);
    std::wstring w(n, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &w[0], n);
    return w;
}

std::string narrow(const std::wstring& w) {
    if (w.empty()) return std::string();
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), NULL, 0, NULL, NULL);
    std::string s(n, 0);
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), &s[0], n, NULL, NULL);
    return s;
}

FILE* openFile(const std::string& path, const char* mode) {
    return _wfopen(widen(path).c_str(), widen(mode).c_str());
}

bool fileExists(const std::string& path) {
    DWORD a = GetFileAttributesW(widen(path).c_str());
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

bool dirExists(const std::string& path) {
    DWORD a = GetFileAttributesW(widen(path).c_str());
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
}

void makeDirs(const std::string& path) {
    std::string p = normalizeSlashes(path);
    std::string cur;
    for (size_t i = 0; i <= p.size(); ++i) {
        if (i == p.size() || p[i] == '/') {
            if (!cur.empty() && cur.back() != ':' && !dirExists(cur))
                CreateDirectoryW(widen(cur).c_str(), NULL);
        }
        if (i < p.size()) cur += p[i];
    }
}

std::vector<std::string> listFiles(const std::string& dir, const std::vector<std::string>& exts) {
    std::vector<std::string> out;
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(widen(dir + "/*").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return out;
    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        std::string name = narrow(fd.cFileName);
        std::string e = extension(name);
        if (std::find(exts.begin(), exts.end(), e) != exts.end()) out.push_back(dir + "/" + name);
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    std::sort(out.begin(), out.end());
    return out;
}

std::string normalizeSlashes(const std::string& s) {
    std::string r = s;
    for (size_t i = 0; i < r.size(); ++i)
        if (r[i] == '\\') r[i] = '/';
    return r;
}

std::string fileName(const std::string& path) {
    std::string p = normalizeSlashes(path);
    size_t k = p.find_last_of('/');
    return k == std::string::npos ? p : p.substr(k + 1);
}

std::string stem(const std::string& path) {
    std::string f = fileName(path);
    size_t k = f.find_last_of('.');
    return k == std::string::npos ? f : f.substr(0, k);
}

std::string extension(const std::string& path) {
    std::string f = fileName(path);
    size_t k = f.find_last_of('.');
    return k == std::string::npos ? std::string() : toLower(f.substr(k + 1));
}

std::string toLower(const std::string& s) {
    std::string r = s;
    for (size_t i = 0; i < r.size(); ++i)
        if (r[i] >= 'A' && r[i] <= 'Z') r[i] = (char)(r[i] - 'A' + 'a');
    return r;
}

std::string currentDir() {
    wchar_t buf[MAX_PATH * 2];
    DWORD n = GetCurrentDirectoryW(MAX_PATH * 2, buf);
    return normalizeSlashes(narrow(std::wstring(buf, n)));
}

bool setCurrentDir(const std::string& dir) {
    return SetCurrentDirectoryW(widen(dir).c_str()) != 0;
}

std::string exeDir() {
    wchar_t buf[MAX_PATH * 2];
    DWORD n = GetModuleFileNameW(NULL, buf, MAX_PATH * 2);
    std::string p = normalizeSlashes(narrow(std::wstring(buf, n)));
    size_t k = p.find_last_of('/');
    return k == std::string::npos ? p : p.substr(0, k);
}

std::string makeRelative(const std::string& path) {
    std::string p = normalizeSlashes(path);
    std::string cwd = currentDir();
    if (!cwd.empty() && cwd.back() != '/') cwd += '/';
    if (p.size() > cwd.size() && toLower(p.substr(0, cwd.size())) == toLower(cwd))
        return p.substr(cwd.size());
    return p;
}

bool readTextFile(const std::string& path, std::string& out) {
    FILE* f = openFile(path, "rb");
    if (!f) return false;
    out.clear();
    char buf[8192];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) out.append(buf, n);
    fclose(f);
    return true;
}

bool writeTextFile(const std::string& path, const std::string& data) {
    FILE* f = openFile(path, "wb");
    if (!f) return false;
    size_t n = fwrite(data.data(), 1, data.size(), f);
    fclose(f);
    return n == data.size();
}

void appendUtf8(std::string& s, unsigned int cp) {
    if (cp < 0x80) s += (char)cp;
    else if (cp < 0x800) {
        s += (char)(0xC0 | (cp >> 6));
        s += (char)(0x80 | (cp & 0x3F));
    } else {
        s += (char)(0xE0 | (cp >> 12));
        s += (char)(0x80 | ((cp >> 6) & 0x3F));
        s += (char)(0x80 | (cp & 0x3F));
    }
}

void popUtf8(std::string& s) {
    while (!s.empty() && ((unsigned char)s.back() & 0xC0) == 0x80) s.pop_back();
    if (!s.empty()) s.pop_back();
}

} // namespace util
