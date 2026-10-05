// Gui.h - Interfaz de usuario en modo inmediato dibujada con OpenGL
// (paneles, botones, campos numéricos arrastrables, combos, menús, selector de color)
#pragma once
#include "core/Math3D.h"
#include <string>
#include <vector>
#include <functional>
#include <cstdint>

struct Rect {
    float x, y, w, h;
    Rect() : x(0), y(0), w(0), h(0) {}
    Rect(float a, float b, float c, float d) : x(a), y(b), w(c), h(d) {}
    bool contains(float px, float py) const { return px >= x && px < x + w && py >= y && py < y + h; }
    float right() const { return x + w; }
    float bottom() const { return y + h; }
};

namespace Theme {
const uint32_t WindowBg = 0x141518;
const uint32_t PanelBg = 0x212327;
const uint32_t HeaderBg = 0x2a2d32;
const uint32_t PopupBg = 0x2b2e34;
const uint32_t WidgetBg = 0x17181b;
const uint32_t WidgetHover = 0x30333a;
const uint32_t Border = 0x3b3f46;
const uint32_t Accent = 0x1f5fbf;
const uint32_t AccentHover = 0x2b72de;
const uint32_t Text = 0xdee0e4;
const uint32_t TextDim = 0x8f96a0;
const uint32_t Button = 0x363940;
const uint32_t ButtonHover = 0x43474f;
const uint32_t ButtonActive = 0x50555e;
const uint32_t Orange = 0xf0a030;
}

struct FontAtlas {
    unsigned int tex;
    int texW, texH, cols;
    float cellW, cellH, height;
    float adv[256];
    FontAtlas() : tex(0), texW(1), texH(1), cols(16), cellW(1), cellH(1), height(1) {
        for (int i = 0; i < 256; ++i) adv[i] = 0;
    }
};

class Gui {
public:
    // estado de entrada (coordenadas lógicas, ya divididas por la escala DPI)
    float scale = 1.0f;
    float W = 0, H = 0;
    int physW = 1, physH = 1;
    float mx = 0, my = 0;
    bool mouseDown[3] = {false, false, false};
    bool mousePressed[3] = {false, false, false};
    bool mouseReleased[3] = {false, false, false};
    bool doubleClick = false;
    float wheel = 0;
    bool ctrl = false, shift = false, alt = false;
    bool consumed = false;
    float time = 0;
    int activeId = 0, focusId = 0, popupId = 0;
    Rect lastPopupRect;
    std::function<void()> onEditBegin; // se llama justo antes de que un widget modifique un valor

    bool init(void* hdc, float uiScale);
    void newFrame(int pw, int ph, float dt);
    void begin2D();
    void endFrame();
    void resetInput();

    void inputMouseMove(int px, int py);
    void inputMouseButton(int b, bool down, bool dbl);
    void inputWheel(float d);
    void inputChar(unsigned int c);
    void inputKey(int vk);
    bool keyPressed(int vk) const;
    const std::vector<int>& keys() const { return keysThisFrame; }

    // dibujo
    void rect(float x, float y, float w, float h, uint32_t c, float a = 1.0f);
    void rect(const Rect& r, uint32_t c, float a = 1.0f) { rect(r.x, r.y, r.w, r.h, c, a); }
    void rectOutline(const Rect& r, uint32_t c, float a = 1.0f, float lw = 1.0f);
    void line(float x1, float y1, float x2, float y2, uint32_t c, float lw = 1.0f, float a = 1.0f);
    void triangle(float x1, float y1, float x2, float y2, float x3, float y3, uint32_t c, float a = 1.0f);
    void circle(float cx, float cy, float r, uint32_t c, bool filled = true, float a = 1.0f, float lw = 1.0f);
    void text(float x, float y, const std::string& s, uint32_t c = Theme::Text, bool bold = false, float a = 1.0f);
    void textCentered(const Rect& r, const std::string& s, uint32_t c = Theme::Text, bool bold = false);
    void textLeft(const Rect& r, const std::string& s, uint32_t c = Theme::Text, float pad = 6.0f);
    float textWidth(const std::string& s, bool bold = false) const;
    float textHeight() const;
    std::string fitText(const std::string& s, float maxW) const;
    void image(unsigned int tex, float x, float y, float w, float h);
    void pushClip(const Rect& r);
    void popClip();

    // interacción
    bool mouseIn(const Rect& r) const;
    bool clickIn(const Rect& r, int button = 0);
    void consume() { consumed = true; }
    bool isTyping() const { return focusId != 0; }
    bool isPopupOpen() const { return popupId != 0; }

    // widgets
    bool button(int id, const Rect& r, const std::string& label, bool selected = false, bool enabled = true);
    bool dragFloat(int id, const Rect& r, float& v, float speed = 0.05f, const char* fmt = "%.2f",
                   float mn = -1e9f, float mx = 1e9f, uint32_t accent = 0);
    bool inputText(int id, const Rect& r, std::string& s);
    bool checkbox(int id, float x, float y, const std::string& label, bool& v);
    bool slider(int id, const Rect& r, float& v, float mn, float mx, const char* fmt = "%.1f");
    bool combo(int id, const Rect& r, const std::vector<std::string>& items, int& idx);
    bool colorEdit(int id, const Rect& r, Vec3& c);
    int menu(int id, const Rect& r, const std::string& label, const std::vector<std::string>& items);

    // ventanas emergentes (se dibujan al final del cuadro, encima de todo)
    void openPopup(int id, const Rect& anchor, bool isMenu = false);
    void closePopup();
    bool beginPopup(int id, Rect r);
    void endPopup();

private:
    FontAtlas font, fontBold;
    std::vector<Rect> clips, savedClips;
    std::vector<int> keysThisFrame;
    std::string typed;
    std::string editBuf;
    bool selectAll = false;
    bool activeSeen = false, focusSeen = false;
    float dragStartX = 0, dragStartVal = 0;
    bool dragMoved = false;
    bool inOverlay = false, overlayPending = false, popupDrawn = false, popupJustOpened = false, popupIsMenu = false;
    Rect popupRect, popupAnchor;
    float popupScroll = 0;
    unsigned int overlayList = 0;

    void applyClip();
    bool editField(int id, const Rect& r, bool numeric, std::string& outCommitted);
};
