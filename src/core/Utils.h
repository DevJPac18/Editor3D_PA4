// Utils.h - Utilidades: cadenas, rutas y archivos (UTF-8)
#pragma once
#include <string>
#include <vector>
#include <cstdio>
#include <cstdint>

namespace util {

std::string format(const char* fmt, ...);
uint32_t hash(const char* s, uint32_t seed = 0);

std::wstring widen(const std::string& s);
std::string narrow(const std::wstring& s);

FILE* openFile(const std::string& path, const char* mode);
bool fileExists(const std::string& path);
bool dirExists(const std::string& path);
void makeDirs(const std::string& path);
std::vector<std::string> listFiles(const std::string& dir, const std::vector<std::string>& exts);

std::string fileName(const std::string& path);
std::string stem(const std::string& path);
std::string extension(const std::string& path); // minúsculas, sin punto
std::string toLower(const std::string& s);
std::string normalizeSlashes(const std::string& s);

std::string currentDir();
bool setCurrentDir(const std::string& dir);
std::string exeDir();
std::string makeRelative(const std::string& path);

bool readTextFile(const std::string& path, std::string& out);
bool writeTextFile(const std::string& path, const std::string& data);

void appendUtf8(std::string& s, unsigned int cp);
void popUtf8(std::string& s);

} // namespace util
