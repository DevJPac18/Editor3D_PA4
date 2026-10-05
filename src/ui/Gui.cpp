#include "ui/Gui.h"
#include "render/GL.h"
#include "core/Utils.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <algorithm>

namespace {

void setColor(uint32_t c, float a) {
    glColor4f(((c >> 16) & 255) / 255.0f, ((c >> 8) & 255) / 255.0f, (c & 255) / 255.0f, a);
}

uint32_t vecToHex(const Vec3& c) {
    int r = (int)(clampf(c.x, 0, 1) * 255 + 0.5f), g = (int)(clampf(c.y, 0, 1) * 255 + 0.5f),
        b = (int)(clampf(c.z, 0, 1) * 255 + 0.5f);
    return (uint32_t)((r << 16) | (g << 8) | b);
}

unsigned decodeUtf8(const std::string& s, size_t& i) {
    unsigned char c = (unsigned char)s[i];
    if (c < 0x80) { i++; return c; }
    if ((c & 0xE0) == 0xC0 && i + 1 < s.size()) {
        unsigned cp = ((c & 0x1Fu) << 6) | ((unsigned char)s[i + 1] & 0x3Fu);
        i += 2;
        return cp;
    }
    if ((c & 0xF0) == 0xE0 && i + 2 < s.size()) {
        unsigned cp = ((c & 0x0Fu) << 12) | (((unsigned char)s[i + 1] & 0x3Fu) << 6) | ((unsigned char)s[i + 2] & 0x3Fu);
        i += 3;
        return cp;
    }
    if ((c & 0xF8) == 0xF0) { i += 4; return '?'; }
    i++;
    return '?';
}

// Rasteriza los glifos Latin-1 con GDI (antialiasados) en una textura alfa
bool buildFont(HDC dc, FontAtlas& f, int px, int weight) {
    HDC mem = CreateCompatibleDC(dc);
    if (!mem) return false;
    HFONT hf = CreateFontW(-px, 0, 0, 0, weight, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_TT_PRECIS,
                           CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
    HGDIOBJ oldFont = SelectObject(mem, hf);
    TEXTMETRICW tm;
    GetTextMetricsW(mem, &tm);
    INT widths[256];
    if (!GetCharWidth32W(mem, 0, 255, widths))
        for (int i = 0; i < 256; ++i) widths[i] = tm.tmAveCharWidth;
    int maxW = 1;
    for (int c = 32; c < 256; ++c) maxW = std::max(maxW, (int)widths[c]);
    int cw = maxW + 4, ch = tm.tmHeight + 2;
    int cols = 16, rows = 14;
    int W = 64, H = 64;
    while (W < cols * cw) W *= 2;
    while (H < rows * ch) H *= 2;
    BITMAPINFO bi;
    ZeroMemory(&bi, sizeof(bi));
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = W;
    bi.bmiHeader.biHeight = -H;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HBITMAP bmp = CreateDIBSection(mem, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
    if (!bmp || !bits) {
        SelectObject(mem, oldFont);
        DeleteObject(hf);
        DeleteDC(mem);
        return false;
    }
    HGDIOBJ oldBmp = SelectObject(mem, bmp);
    memset(bits, 0, (size_t)W * H * 4);
    SetBkMode(mem, TRANSPARENT);
    SetTextColor(mem, RGB(255, 255, 255));
    SetTextAlign(mem, TA_TOP | TA_LEFT);
    for (int c = 32; c < 256; ++c) {
        int i = c - 32;
        wchar_t wc = (wchar_t)c;
        TextOutW(mem, (i % cols) * cw + 2, (i / cols) * ch + 1, &wc, 1);
    }
    GdiFlush();
    std::vector<unsigned char> alpha((size_t)W * H);
    const unsigned char* p = (const unsigned char*)bits;
    for (size_t i = 0; i < alpha.size(); ++i)
        alpha[i] = std::max(p[i * 4 + 2], std::max(p[i * 4 + 1], p[i * 4]));
    glGenTextures(1, &f.tex);
    glBindTexture(GL_TEXTURE_2D, f.tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, W, H, 0, GL_ALPHA, GL_UNSIGNED_BYTE, alpha.data());
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glBindTexture(GL_TEXTURE_2D, 0);
    f.texW = W;
    f.texH = H;
    f.cols = cols;
    f.cellW = (float)cw;
    f.cellH = (float)ch;
    f.height = (float)tm.tmHeight;
    for (int c = 0; c < 256; ++c) f.adv[c] = (float)widths[c];
    SelectObject(mem, oldBmp);
    SelectObject(mem, oldFont);
    DeleteObject(bmp);
    DeleteObject(hf);
    DeleteDC(mem);
    return true;
}

} // namespace

// ---------------------------------------------------------------- ciclo de cuadro

bool Gui::init(void* hdc, float uiScale) {
    scale = uiScale > 0.5f ? uiScale : 1.0f;
    int px = (int)(13.0f * scale + 0.5f);
    bool ok = buildFont((HDC)hdc, font, px, FW_NORMAL);
    if (!buildFont((HDC)hdc, fontBold, px, FW_BOLD)) fontBold = font;
    overlayList = glGenLists(1);
    return ok;
}

void Gui::newFrame(int pw, int ph, float dt) {
    physW = std::max(1, pw);
    physH = std::max(1, ph);
    W = physW / scale;
    H = physH / scale;
    time += dt;
    ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
    shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
    alt = (GetKeyState(VK_MENU) & 0x8000) != 0;
    activeSeen = false;
    focusSeen = false;
    popupDrawn = false;
}

void Gui::begin2D() {
    glViewport(0, 0, physW, physH);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glDisable(GL_CULL_FACE);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_SCISSOR_TEST);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glMatrixMode(GL_TEXTURE);
    glLoadIdentity();
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, physW, physH, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glScalef(scale, scale, 1.0f);
    clips.clear();
}

void Gui::endFrame() {
    clips.clear();
    applyClip();
    if (overlayPending) {
        glCallList(overlayList);
        overlayPending = false;
    }
    if (popupId && !popupDrawn) popupId = 0;
    if (popupId && mousePressed[0] && !popupJustOpened && !popupRect.contains(mx, my) && !popupAnchor.contains(mx, my))
        popupId = 0;
    if (popupId && keyPressed(VK_ESCAPE)) popupId = 0;
    lastPopupRect = popupId ? popupRect : Rect();
    if (focusId && !focusSeen) focusId = 0;
    if (activeId && !activeSeen) activeId = 0;
    popupJustOpened = false;
    for (int b = 0; b < 3; ++b) mousePressed[b] = mouseReleased[b] = false;
    doubleClick = false;
    wheel = 0;
    typed.clear();
    keysThisFrame.clear();
    consumed = false;
}

void Gui::resetInput() {
    for (int b = 0; b < 3; ++b) mouseDown[b] = mousePressed[b] = mouseReleased[b] = false;
    activeId = 0;
    doubleClick = false;
}

// ---------------------------------------------------------------- entrada

void Gui::inputMouseMove(int px, int py) {
    mx = px / scale;
    my = py / scale;
}

void Gui::inputMouseButton(int b, bool down, bool dbl) {
    if (b < 0 || b > 2) return;
    if (down) {
        mouseDown[b] = true;
        mousePressed[b] = true;
        if (dbl) doubleClick = true;
    } else {
        mouseDown[b] = false;
        mouseReleased[b] = true;
    }
}

void Gui::inputWheel(float d) { wheel += d; }

void Gui::inputChar(unsigned int c) {
    if (c < 32 || c == 127) return;
    util::appendUtf8(typed, c);
}

void Gui::inputKey(int vk) { keysThisFrame.push_back(vk); }

bool Gui::keyPressed(int vk) const {
    return std::find(keysThisFrame.begin(), keysThisFrame.end(), vk) != keysThisFrame.end();
}

// ---------------------------------------------------------------- dibujo

void Gui::applyClip() {
    if (clips.empty()) {
        glDisable(GL_SCISSOR_TEST);
        return;
    }
    const Rect& r = clips.back();
    int x0 = (int)std::floor(r.x * scale), y0 = (int)std::floor(r.y * scale);
    int x1 = (int)std::ceil(r.right() * scale), y1 = (int)std::ceil(r.bottom() * scale);
    glEnable(GL_SCISSOR_TEST);
    glScissor(x0, physH - y1, std::max(0, x1 - x0), std::max(0, y1 - y0));
}

void Gui::pushClip(const Rect& r) {
    Rect c = r;
    if (!clips.empty()) {
        const Rect& p = clips.back();
        float x0 = std::max(c.x, p.x), y0 = std::max(c.y, p.y);
        float x1 = std::min(c.right(), p.right()), y1 = std::min(c.bottom(), p.bottom());
        c = Rect(x0, y0, std::max(0.0f, x1 - x0), std::max(0.0f, y1 - y0));
    }
    clips.push_back(c);
    applyClip();
}

void Gui::popClip() {
    if (!clips.empty()) clips.pop_back();
    applyClip();
}

void Gui::rect(float x, float y, float w, float h, uint32_t c, float a) {
    setColor(c, a);
    glBegin(GL_QUADS);
    glVertex2f(x, y);
    glVertex2f(x + w, y);
    glVertex2f(x + w, y + h);
    glVertex2f(x, y + h);
    glEnd();
}

void Gui::rectOutline(const Rect& r, uint32_t c, float a, float lw) {
    float t = lw / scale * std::max(1.0f, std::floor(scale));
    rect(r.x, r.y, r.w, t, c, a);
    rect(r.x, r.bottom() - t, r.w, t, c, a);
    rect(r.x, r.y, t, r.h, c, a);
    rect(r.right() - t, r.y, t, r.h, c, a);
}

void Gui::line(float x1, float y1, float x2, float y2, uint32_t c, float lw, float a) {
    setColor(c, a);
    glLineWidth(lw * scale);
    glBegin(GL_LINES);
    glVertex2f(x1, y1);
    glVertex2f(x2, y2);
    glEnd();
    glLineWidth(1.0f);
}

void Gui::triangle(float x1, float y1, float x2, float y2, float x3, float y3, uint32_t c, float a) {
    setColor(c, a);
    glBegin(GL_TRIANGLES);
    glVertex2f(x1, y1);
    glVertex2f(x2, y2);
    glVertex2f(x3, y3);
    glEnd();
}

void Gui::circle(float cx, float cy, float r, uint32_t c, bool filled, float a, float lw) {
    setColor(c, a);
    const int N = 24;
    if (filled) {
        glBegin(GL_TRIANGLE_FAN);
        glVertex2f(cx, cy);
    } else {
        glLineWidth(lw * scale);
        glBegin(GL_LINE_LOOP);
    }
    for (int i = 0; i <= N; ++i) {
        if (!filled && i == N) break;
        float t = 2.0f * PI_F * i / N;
        glVertex2f(cx + std::cos(t) * r, cy + std::sin(t) * r);
    }
    glEnd();
    glLineWidth(1.0f);
}

void Gui::text(float x, float y, const std::string& s, uint32_t c, bool bold, float a) {
    const FontAtlas& f = bold ? fontBold : font;
    if (!f.tex || s.empty()) return;
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, f.tex);
    setColor(c, a);
    float inv = 1.0f / scale;
    float cx = std::floor(x * scale + 0.5f), cy = std::floor(y * scale + 0.5f);
    glBegin(GL_QUADS);
    size_t i = 0;
    while (i < s.size()) {
        unsigned cp = decodeUtf8(s, i);
        if (cp < 32 || cp > 255) cp = '?';
        int gi = (int)cp - 32;
        float gx = (float)(gi % f.cols) * f.cellW, gy = (float)(gi / f.cols) * f.cellH;
        float u0 = gx / f.texW, v0 = gy / f.texH, u1 = (gx + f.cellW) / f.texW, v1 = (gy + f.cellH) / f.texH;
        float qx = (cx - 2) * inv, qy = (cy - 1) * inv, qw = f.cellW * inv, qh = f.cellH * inv;
        glTexCoord2f(u0, v0); glVertex2f(qx, qy);
        glTexCoord2f(u1, v0); glVertex2f(qx + qw, qy);
        glTexCoord2f(u1, v1); glVertex2f(qx + qw, qy + qh);
        glTexCoord2f(u0, v1); glVertex2f(qx, qy + qh);
        cx += f.adv[cp];
    }
    glEnd();
    glDisable(GL_TEXTURE_2D);
}

float Gui::textWidth(const std::string& s, bool bold) const {
    const FontAtlas& f = bold ? fontBold : font;
    float w = 0;
    size_t i = 0;
    while (i < s.size()) {
        unsigned cp = decodeUtf8(s, i);
        if (cp < 32 || cp > 255) cp = '?';
        w += f.adv[cp];
    }
    return w / scale;
}

float Gui::textHeight() const { return font.height / scale; }

void Gui::textCentered(const Rect& r, const std::string& s, uint32_t c, bool bold) {
    float w = textWidth(s, bold);
    text(r.x + (r.w - w) * 0.5f, r.y + (r.h - textHeight()) * 0.5f, s, c, bold);
}

void Gui::textLeft(const Rect& r, const std::string& s, uint32_t c, float pad) {
    text(r.x + pad, r.y + (r.h - textHeight()) * 0.5f, s, c);
}

std::string Gui::fitText(const std::string& s, float maxW) const {
    if (textWidth(s) <= maxW) return s;
    std::string r = s;
    while (!r.empty() && textWidth(r + "..") > maxW) util::popUtf8(r);
    return r + "..";
}

void Gui::image(unsigned int tex, float x, float y, float w, float h) {
    if (!tex) {
        rect(x, y, w, h, 0x404040);
        return;
    }
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, tex);
    glColor4f(1, 1, 1, 1);
    glBegin(GL_QUADS);
    glTexCoord2f(0, 1); glVertex2f(x, y);
    glTexCoord2f(1, 1); glVertex2f(x + w, y);
    glTexCoord2f(1, 0); glVertex2f(x + w, y + h);
    glTexCoord2f(0, 0); glVertex2f(x, y + h);
    glEnd();
    glDisable(GL_TEXTURE_2D);
}

// ---------------------------------------------------------------- interacción

bool Gui::mouseIn(const Rect& r) const {
    if (!r.contains(mx, my)) return false;
    if (!clips.empty() && !clips.back().contains(mx, my)) return false;
    if (!inOverlay && popupId != 0 && lastPopupRect.w > 0 && lastPopupRect.contains(mx, my)) return false;
    return true;
}

bool Gui::clickIn(const Rect& r, int b) {
    if (mousePressed[b] && !consumed && mouseIn(r)) {
        consumed = true;
        return true;
    }
    return false;
}

void Gui::openPopup(int id, const Rect& anchor, bool isMenu) {
    popupId = id;
    popupAnchor = anchor;
    popupJustOpened = true;
    popupIsMenu = isMenu;
    popupScroll = 0;
    focusId = 0;
}

void Gui::closePopup() { popupId = 0; }

bool Gui::beginPopup(int id, Rect r) {
    if (popupId != id) return false;
    if (r.right() > W) r.x = std::max(0.0f, W - r.w - 2);
    if (r.bottom() > H) r.y = std::max(0.0f, H - r.h - 2);
    popupRect = r;
    popupDrawn = true;
    savedClips = clips;
    clips.clear();
    glNewList(overlayList, GL_COMPILE); // el contenido se dibuja al final, encima de todo
    inOverlay = true;
    applyClip();
    rect(r.x + 3, r.y + 4, r.w, r.h, 0x000000, 0.35f);
    rect(r, Theme::PopupBg);
    rectOutline(r, Theme::Border);
    return true;
}

void Gui::endPopup() {
    inOverlay = false;
    glEndList();
    overlayPending = true;
    clips = savedClips;
    applyClip();
}

// ---------------------------------------------------------------- widgets

bool Gui::button(int id, const Rect& r, const std::string& label, bool selected, bool enabled) {
    (void)id;
    bool hov = enabled && mouseIn(r);
    bool clicked = enabled && clickIn(r);
    uint32_t bg = selected ? (hov ? Theme::AccentHover : Theme::Accent)
                           : (hov ? (mouseDown[0] ? Theme::ButtonActive : Theme::ButtonHover) : Theme::Button);
    rect(r, bg);
    rectOutline(r, selected ? Theme::AccentHover : Theme::Border, 0.8f);
    textCentered(r, fitText(label, r.w - 6), enabled ? Theme::Text : Theme::TextDim);
    return clicked;
}

bool Gui::editField(int id, const Rect& r, bool numeric, std::string& out) {
    focusSeen = true;
    if (!typed.empty()) {
        if (selectAll) { editBuf.clear(); selectAll = false; }
        for (size_t i = 0; i < typed.size(); ++i) {
            char c = typed[i];
            if (numeric && !((c >= '0' && c <= '9') || c == '.' || c == '-' || c == '+' || c == 'e' || c == 'E'))
                continue;
            editBuf += c;
        }
    }
    for (size_t k = 0; k < keysThisFrame.size(); ++k) {
        int vk = keysThisFrame[k];
        if (vk == VK_BACK) {
            if (selectAll) { editBuf.clear(); selectAll = false; }
            else util::popUtf8(editBuf);
        } else if (vk == VK_DELETE && selectAll) {
            editBuf.clear();
            selectAll = false;
        }
    }
    bool commit = keyPressed(VK_RETURN) || keyPressed(VK_TAB) || (mousePressed[0] && !r.contains(mx, my));
    bool cancel = keyPressed(VK_ESCAPE);
    rect(r, 0x101114);
    rectOutline(r, Theme::AccentHover);
    if (selectAll) {
        float tw = textWidth(editBuf);
        rect(r.x + 5, r.y + 3, std::min(tw + 2, r.w - 8), r.h - 6, Theme::Accent, 0.8f);
    }
    std::string shown = editBuf;
    float maxW = r.w - 14;
    while (!shown.empty() && textWidth(shown) > maxW) shown.erase(0, 1);
    textLeft(r, shown);
    if (std::fmod(time, 1.0f) < 0.55f && !selectAll) {
        float cx = r.x + 6 + textWidth(shown) + 1;
        rect(cx, r.y + 4, 1.2f, r.h - 8, Theme::Text);
    }
    (void)id;
    if (cancel) { focusId = 0; return false; }
    if (commit) {
        focusId = 0;
        out = editBuf;
        return true;
    }
    return false;
}

bool Gui::dragFloat(int id, const Rect& r, float& v, float speed, const char* fmt, float mn, float hi, uint32_t accent) {
    if (focusId == id) {
        std::string out;
        if (editField(id, r, true, out) && !out.empty()) {
            float nv = clampf((float)atof(out.c_str()), mn, hi);
            if (nv != v) {
                if (onEditBegin) onEditBegin();
                v = nv;
                return true;
            }
        }
        return false;
    }
    bool hov = mouseIn(r);
    bool changed = false;
    if (hov && mousePressed[0] && !consumed) {
        consumed = true;
        activeId = id;
        dragStartX = mx;
        dragStartVal = v;
        dragMoved = false;
        if (onEditBegin) onEditBegin();
    }
    if (activeId == id) {
        activeSeen = true;
        if (mouseDown[0]) {
            float dx = mx - dragStartX;
            if (!dragMoved && std::fabs(dx) > 2.0f) dragMoved = true;
            if (dragMoved) {
                float sp = speed * (shift ? 0.1f : 1.0f) * (ctrl ? 10.0f : 1.0f);
                float nv = clampf(dragStartVal + dx * sp, mn, hi);
                if (nv != v) { v = nv; changed = true; }
            }
        } else {
            activeId = 0;
            if (!dragMoved) { // clic sin arrastre: edición por teclado
                focusId = id;
                char b[64];
                snprintf(b, sizeof(b), fmt, v);
                editBuf = b;
                selectAll = true;
                focusSeen = true;
            }
        }
    }
    uint32_t bg = activeId == id ? Theme::WidgetHover : (hov ? 0x25272c : Theme::WidgetBg);
    rect(r, bg);
    rectOutline(r, Theme::Border, 0.8f);
    if (accent) rect(r.x, r.y, 3.0f, r.h, accent);
    char b[64];
    snprintf(b, sizeof(b), fmt, v);
    textCentered(r, b);
    return changed;
}

bool Gui::inputText(int id, const Rect& r, std::string& s) {
    if (focusId == id) {
        std::string out;
        if (editField(id, r, false, out)) {
            if (out != s) {
                s = out;
                return true;
            }
        }
        return false;
    }
    bool hov = mouseIn(r);
    if (clickIn(r)) {
        if (onEditBegin) onEditBegin();
        focusId = id;
        editBuf = s;
        selectAll = false;
        focusSeen = true;
    }
    rect(r, hov ? 0x25272c : Theme::WidgetBg);
    rectOutline(r, Theme::Border, 0.8f);
    textLeft(r, fitText(s, r.w - 12));
    return false;
}

bool Gui::checkbox(int id, float x, float y, const std::string& label, bool& v) {
    (void)id;
    float th = 21.0f;
    Rect box(x, y + 3, 15, 15);
    Rect hit(x, y, 15 + 7 + textWidth(label), th);
    bool hov = mouseIn(hit);
    bool changed = false;
    if (clickIn(hit)) {
        if (onEditBegin) onEditBegin();
        v = !v;
        changed = true;
    }
    rect(box, hov ? Theme::WidgetHover : Theme::WidgetBg);
    rectOutline(box, hov ? Theme::AccentHover : Theme::Border);
    if (v) {
        rect(box.x + 1, box.y + 1, box.w - 2, box.h - 2, Theme::Accent);
        line(box.x + 3.5f, box.y + 7.5f, box.x + 6.5f, box.y + 11, 0xffffff, 2.0f);
        line(box.x + 6.5f, box.y + 11, box.x + 12, box.y + 4, 0xffffff, 2.0f);
    }
    text(x + 22, y + (th - textHeight()) * 0.5f, label);
    return changed;
}

bool Gui::slider(int id, const Rect& r, float& v, float mn, float hi, const char* fmt) {
    bool hov = mouseIn(r);
    bool changed = false;
    if (hov && mousePressed[0] && !consumed) {
        consumed = true;
        activeId = id;
        if (onEditBegin) onEditBegin();
    }
    if (activeId == id) {
        activeSeen = true;
        if (mouseDown[0]) {
            float t = clampf((mx - r.x) / std::max(1.0f, r.w), 0, 1);
            float nv = mn + t * (hi - mn);
            if (nv != v) { v = nv; changed = true; }
        } else {
            activeId = 0;
        }
    }
    float t = hi > mn ? clampf((v - mn) / (hi - mn), 0, 1) : 0;
    rect(r, hov || activeId == id ? 0x25272c : Theme::WidgetBg);
    rect(r.x, r.y, r.w * t, r.h, Theme::Accent, 0.55f);
    rect(r.x + r.w * t - 1.5f, r.y, 3, r.h, 0x9fc4ff);
    rectOutline(r, Theme::Border, 0.8f);
    char b[64];
    snprintf(b, sizeof(b), fmt, v);
    textCentered(r, b);
    return changed;
}

bool Gui::combo(int id, const Rect& r, const std::vector<std::string>& items, int& idx) {
    bool open = popupId == id;
    bool hov = mouseIn(r);
    if (clickIn(r)) {
        if (open) closePopup();
        else openPopup(id, r);
        open = popupId == id;
    }
    rect(r, hov || open ? Theme::WidgetHover : Theme::WidgetBg);
    rectOutline(r, open ? Theme::AccentHover : Theme::Border, 0.8f);
    std::string cur = (idx >= 0 && idx < (int)items.size()) ? items[idx] : std::string("-");
    textLeft(Rect(r.x, r.y, r.w - 16, r.h), fitText(cur, r.w - 26));
    float ax = r.right() - 12, ay = r.y + r.h * 0.5f;
    triangle(ax - 4, ay - 2, ax + 4, ay - 2, ax, ay + 3, Theme::TextDim);
    bool changed = false;
    if (open && !items.empty()) {
        const float ih = 21.0f;
        int n = (int)items.size();
        int vis = std::min(n, 14);
        float mw = r.w;
        for (int i = 0; i < n; ++i) mw = std::max(mw, textWidth(items[i]) + 26);
        Rect pr(r.x, r.bottom() + 1, mw, vis * ih + 4);
        if (pr.bottom() > H) pr.y = r.y - pr.h - 1;
        if (beginPopup(id, pr)) {
            float maxScroll = (n - vis) * ih;
            if (n > vis && pr.contains(mx, my) && wheel != 0) popupScroll -= wheel * ih * 3;
            popupScroll = clampf(popupScroll, 0, maxScroll);
            pushClip(Rect(pr.x, pr.y + 2, pr.w, pr.h - 4));
            for (int i = 0; i < n; ++i) {
                float iy = pr.y + 2 + i * ih - popupScroll;
                if (iy + ih < pr.y || iy > pr.bottom()) continue;
                Rect ir(pr.x + 2, iy, pr.w - 4, ih);
                bool h = mouseIn(ir);
                if (h) rect(ir, Theme::Accent);
                else if (i == idx) rect(ir, Theme::WidgetHover);
                textLeft(ir, items[i], Theme::Text, 8);
                if (clickIn(ir)) {
                    if (onEditBegin) onEditBegin();
                    idx = i;
                    changed = true;
                }
            }
            popClip();
            if (n > vis) {
                float sh = pr.h * vis / n, sy = pr.y + (pr.h - sh) * (maxScroll > 0 ? popupScroll / maxScroll : 0);
                rect(pr.right() - 4, sy, 3, sh, Theme::TextDim, 0.6f);
            }
            endPopup();
        }
        if (changed) closePopup();
    }
    return changed;
}

bool Gui::colorEdit(int id, const Rect& r, Vec3& c) {
    bool open = popupId == id;
    bool hov = mouseIn(r);
    if (clickIn(r)) {
        if (open) closePopup();
        else openPopup(id, r);
        open = popupId == id;
    }
    rect(r, vecToHex(c));
    rectOutline(r, hov || open ? Theme::Text : Theme::Border, 1.0f, 1.0f);
    bool changed = false;
    if (open) {
        Rect pr(r.x, r.bottom() + 2, 250, 168);
        if (pr.bottom() > H) pr.y = r.y - pr.h - 2;
        if (beginPopup(id, pr)) {
            float y = pr.y + 10;
            rect(pr.x + 10, y, 44, 66, vecToHex(c));
            rectOutline(Rect(pr.x + 10, y, 44, 66), Theme::Border);
            const char* names[3] = {"R", "G", "B"};
            const uint32_t cols[3] = {0xe05050, 0x50c050, 0x5080f0};
            for (int i = 0; i < 3; ++i) {
                float sy = y + i * 23;
                text(pr.x + 64, sy + 2, names[i], cols[i], true);
                float v = c[i];
                if (slider(id + 101 + i, Rect(pr.x + 80, sy, pr.w - 90, 19), v, 0.0f, 1.0f, "%.2f")) {
                    c[i] = v;
                    changed = true;
                }
            }
            y += 78;
            static const uint32_t presets[16] = {0xffffff, 0xc8c8c8, 0x808080, 0x303030, 0xd03030, 0xf07020,
                                                 0xf5c518, 0x40b040, 0x20a0a0, 0x3070e0, 0x7040c0, 0xd040a0,
                                                 0x8b5a2b, 0xf0d8b0, 0x1e3a5f, 0x9ad0ff};
            for (int k = 0; k < 16; ++k) {
                Rect cr(pr.x + 10 + (k % 8) * 29, y + (k / 8) * 26, 25, 22);
                rect(cr, presets[k]);
                bool h = mouseIn(cr);
                rectOutline(cr, h ? Theme::Text : Theme::Border);
                if (clickIn(cr)) {
                    if (onEditBegin) onEditBegin();
                    c = Vec3(((presets[k] >> 16) & 255) / 255.0f, ((presets[k] >> 8) & 255) / 255.0f,
                             (presets[k] & 255) / 255.0f);
                    changed = true;
                }
            }
            endPopup();
        }
    }
    return changed;
}

int Gui::menu(int id, const Rect& r, const std::string& label, const std::vector<std::string>& items) {
    bool open = popupId == id;
    bool hov = mouseIn(r);
    if (hov && popupId != 0 && popupIsMenu && !open && !mouseDown[0]) {
        openPopup(id, r, true); // pasar de un menú a otro con el ratón
        open = true;
    }
    if (clickIn(r)) {
        if (open) closePopup();
        else openPopup(id, r, true);
        open = popupId == id;
    }
    rect(r, open ? Theme::Accent : (hov ? Theme::ButtonHover : Theme::HeaderBg));
    textCentered(r, label);
    int chosen = -1;
    if (open) {
        const float ih = 23.0f, sepH = 9.0f;
        float w = r.w, h = 8;
        for (size_t i = 0; i < items.size(); ++i) {
            if (items[i] == "-") { h += sepH; continue; }
            h += ih;
            w = std::max(w, textWidth(items[i]) + 90);
        }
        Rect pr(r.x, r.bottom(), w, h);
        if (beginPopup(id, pr)) {
            float y = pr.y + 4;
            for (size_t i = 0; i < items.size(); ++i) {
                const std::string& it = items[i];
                if (it == "-") {
                    rect(pr.x + 8, y + 4, pr.w - 16, 1, Theme::Border);
                    y += sepH;
                    continue;
                }
                std::string lbl = it, sc;
                int check = 0;
                if (!lbl.empty() && (lbl[0] == '!' || lbl[0] == '?')) {
                    check = lbl[0] == '!' ? 1 : 2;
                    lbl = lbl.substr(1);
                }
                size_t tab = lbl.find('\t');
                if (tab != std::string::npos) {
                    sc = lbl.substr(tab + 1);
                    lbl = lbl.substr(0, tab);
                }
                Rect ir(pr.x + 3, y, pr.w - 6, ih);
                bool h2 = mouseIn(ir);
                if (h2) rect(ir, Theme::Accent);
                if (check) {
                    Rect cb(ir.x + 7, y + 5, 13, 13);
                    rectOutline(cb, h2 ? Theme::Text : Theme::TextDim);
                    if (check == 1) {
                        line(cb.x + 2.5f, cb.y + 6.5f, cb.x + 5.5f, cb.y + 10, 0xffffff, 2.0f);
                        line(cb.x + 5.5f, cb.y + 10, cb.x + 11, cb.y + 3, 0xffffff, 2.0f);
                    }
                }
                text(ir.x + 28, y + (ih - textHeight()) * 0.5f, lbl);
                if (!sc.empty())
                    text(ir.right() - 10 - textWidth(sc), y + (ih - textHeight()) * 0.5f, sc,
                         h2 ? Theme::Text : Theme::TextDim);
                if (clickIn(ir)) chosen = (int)i;
                y += ih;
            }
            endPopup();
        }
        if (chosen >= 0) closePopup();
    }
    return chosen;
}
