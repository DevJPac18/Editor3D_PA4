// FileDialog.h - Diálogos nativos de Windows para abrir / guardar archivos
#pragma once
#include <string>

namespace FileDialog {
// filter: pares "Descripción\0*.ext\0" terminados en doble \0. Devuelve "" si se cancela.
std::string open(void* hwnd, const wchar_t* filter, const wchar_t* title);
std::string save(void* hwnd, const wchar_t* filter, const wchar_t* title, const wchar_t* defExt,
                 const std::string& initialName);
bool confirm(void* hwnd, const std::string& message, const std::string& title);
void info(void* hwnd, const std::string& message, const std::string& title);
}
