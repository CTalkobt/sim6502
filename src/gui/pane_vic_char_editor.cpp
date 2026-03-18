#include "pane_vic_char_editor.h"
#include <wx/sizer.h>
#include <wx/artprov.h>
#include <wx/tooltip.h>
#include <wx/filedlg.h>
#include <wx/button.h>
#include "vic2.h"

// Helper to get C64 color from index
static wxColour GetC64Color(int ci) {
    if (ci < 0 || ci > 15) return *wxBLACK;
    return wxColour(vic2_palette[ci][0], vic2_palette[ci][1], vic2_palette[ci][2]);
}

// --- CharSelector ---

CharSelector::CharSelector(PaneVICChars* parent, sim_session_t* sim)
    : wxGLCanvas(parent, wxID_ANY, NULL, wxDefaultPosition, wxSize(256, 256)),
      m_pane(parent), m_sim(sim), m_texture(0), m_glInitialized(false)
{
    m_context = new wxGLContext(this);
    m_pixels = new uint8_t[128 * 128 * 4](); // 16x16 chars, each 8x8
    Bind(wxEVT_PAINT, &CharSelector::OnPaint, this);
    Bind(wxEVT_LEFT_DOWN, &CharSelector::OnMouseLeftDown, this);
    Bind(wxEVT_KEY_DOWN, &CharSelector::OnKeyDown, this);
}

CharSelector::~CharSelector() {
    delete[] m_pixels;
    delete m_context;
}

void CharSelector::InitGL() {
    SetCurrent(*m_context);
    glGenTextures(1, &m_texture);
    glBindTexture(GL_TEXTURE_2D, m_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    m_glInitialized = true;
}

void CharSelector::OnPaint(wxPaintEvent& WXUNUSED(event)) {
    wxPaintDC dc(this);
    SetCurrent(*m_context);
    if (!m_glInitialized) InitGL();

    uint16_t base = m_pane->GetCharSetAddr();
    bool mcm = m_pane->IsMCM();
    uint8_t c0 = m_pane->GetBGColor();
    uint8_t c1 = m_pane->GetMC1Color();
    uint8_t c2 = m_pane->GetMC2Color();
    uint8_t c3 = m_pane->GetCharColor();

    for (int cy = 0; cy < 16; cy++) {
        for (int cx = 0; cx < 16; cx++) {
            int idx = cy * 16 + cx;
            uint8_t char_buf[8 * 8 * 4];
            sim_vic_render_char(m_sim, base, idx, mcm, c0, c1, c2, c3, char_buf);
            
            // Copy to selector pixels
            for (int y = 0; y < 8; y++) {
                for (int x = 0; x < 8; x++) {
                    int src = (y * 8 + x) * 4;
                    int dst = ((cy * 8 + y) * 128 + (cx * 8 + x)) * 4;
                    memcpy(&m_pixels[dst], &char_buf[src], 4);
                }
            }
        }
    }

    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    
    wxSize sz = GetClientSize();
    double scale = GetContentScaleFactor();
    glViewport(0, 0, (int)(sz.x * scale), (int)(sz.y * scale));
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, m_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 128, 128, 0, GL_RGBA, GL_UNSIGNED_BYTE, m_pixels);

    glBegin(GL_QUADS);
    glTexCoord2f(0, 1); glVertex2f(-1, -1);
    glTexCoord2f(1, 1); glVertex2f(1, -1);
    glTexCoord2f(1, 0); glVertex2f(1, 1);
    glTexCoord2f(0, 0); glVertex2f(-1, 1);
    glEnd();

    // Selection highlight
    int sel = m_pane->GetSelectedChar();
    int sx = sel % 16;
    int sy = sel / 16;
    float fx = -1.0f + (sx * 2.0f / 16.0f);
    float fy = 1.0f - ((sy + 1) * 2.0f / 16.0f);
    float fw = 2.0f / 16.0f;
    float fh = 2.0f / 16.0f;

    glDisable(GL_TEXTURE_2D);
    // Semi-transparent yellow fill
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(1.0f, 1.0f, 0.0f, 0.25f);
    glBegin(GL_QUADS);
    glVertex2f(fx, fy);
    glVertex2f(fx + fw, fy);
    glVertex2f(fx + fw, fy + fh);
    glVertex2f(fx, fy + fh);
    glEnd();
    glDisable(GL_BLEND);
    // Solid yellow border
    glLineWidth(2.0f);
    glColor3f(1.0f, 1.0f, 0.0f);
    glBegin(GL_LINE_LOOP);
    glVertex2f(fx, fy);
    glVertex2f(fx + fw, fy);
    glVertex2f(fx + fw, fy + fh);
    glVertex2f(fx, fy + fh);
    glEnd();
    glLineWidth(1.0f);

    SwapBuffers();
}

void CharSelector::OnMouseLeftDown(wxMouseEvent& event) {
    SetFocus();
    wxSize sz = GetClientSize();
    if (sz.x <= 0 || sz.y <= 0) { event.Skip(); return; }
    int cx = event.GetX() * 16 / sz.x;
    int cy = event.GetY() * 16 / sz.y;
    if (cx >= 0 && cx < 16 && cy >= 0 && cy < 16) {
        m_pane->SetSelectedChar(cy * 16 + cx);
    }
    event.Skip();
}

void CharSelector::OnKeyDown(wxKeyEvent& event) {
    if (event.ControlDown()) {
        switch (event.GetKeyCode()) {
        case 'C': m_pane->CopyChar();  return;
        case 'V': m_pane->PasteChar(); return;
        }
    }
    event.Skip();
}

// --- CharBigEditor ---

CharBigEditor::CharBigEditor(PaneVICChars* parent, sim_session_t* sim)
    : wxGLCanvas(parent, wxID_ANY, NULL, wxDefaultPosition, wxSize(256, 256)),
      m_pane(parent), m_sim(sim), m_index(0), m_texture(0), m_glInitialized(false),
      m_last_px(-1), m_last_py(-1), m_last_width(256), m_last_height(256),
      m_hover_px(-1), m_hover_py(-1)
{
    m_context = new wxGLContext(this);
    m_pixels = new uint8_t[8 * 8 * 4]();
    Bind(wxEVT_PAINT, &CharBigEditor::OnPaint, this);
    Bind(wxEVT_LEFT_DOWN, &CharBigEditor::OnMouseLeftDown, this);
    Bind(wxEVT_MOTION, &CharBigEditor::OnMouseMove, this);
    Bind(wxEVT_LEAVE_WINDOW, &CharBigEditor::OnMouseLeave, this);
    Bind(wxEVT_KEY_DOWN, &CharBigEditor::OnKeyDown, this);
}

CharBigEditor::~CharBigEditor() {
    delete[] m_pixels;
    delete m_context;
}

void CharBigEditor::InitGL() {
    SetCurrent(*m_context);
    glGenTextures(1, &m_texture);
    glBindTexture(GL_TEXTURE_2D, m_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    m_glInitialized = true;
}

void CharBigEditor::OnPaint(wxPaintEvent& WXUNUSED(event)) {
    wxPaintDC dc(this);
    SetCurrent(*m_context);
    if (!m_glInitialized) InitGL();

    uint16_t base = m_pane->GetCharSetAddr();
    bool mcm = m_pane->IsMCM();
    uint8_t c0 = m_pane->GetBGColor();
    uint8_t c1 = m_pane->GetMC1Color();
    uint8_t c2 = m_pane->GetMC2Color();
    uint8_t c3 = m_pane->GetCharColor();

    sim_vic_render_char(m_sim, base, m_index, mcm, c0, c1, c2, c3, m_pixels);

    glClearColor(0.05f, 0.05f, 0.05f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    
    wxSize sz = GetClientSize();
    m_last_width = sz.x;
    m_last_height = sz.y;
    double scale = GetContentScaleFactor();
    glViewport(0, 0, (int)(sz.x * scale), (int)(sz.y * scale));
    
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, m_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 8, 8, 0, GL_RGBA, GL_UNSIGNED_BYTE, m_pixels);

    glBegin(GL_QUADS);
    glTexCoord2f(0, 1); glVertex2f(-1, -1);
    glTexCoord2f(1, 1); glVertex2f(1, -1);
    glTexCoord2f(1, 0); glVertex2f(1, 1);
    glTexCoord2f(0, 0); glVertex2f(-1, 1);
    glEnd();

    glDisable(GL_TEXTURE_2D);

    // MCM column-pair tints (drawn under grid so lines remain visible)
    bool mcm_overlay = m_pane->IsMCM() && m_pane->ShowMCMGrid();
    if (mcm_overlay) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        for (int pair = 0; pair < 4; pair++) {
            float bx = -1.0f + pair * (4.0f / 8.0f);
            float bw = 4.0f / 8.0f;
            if (pair % 2 == 0)
                glColor4f(0.3f, 0.6f, 1.0f, 0.12f);
            else
                glColor4f(1.0f, 0.6f, 0.2f, 0.12f);
            glBegin(GL_QUADS);
            glVertex2f(bx, -1.0f); glVertex2f(bx + bw, -1.0f);
            glVertex2f(bx + bw,  1.0f); glVertex2f(bx,  1.0f);
            glEnd();
        }
        glDisable(GL_BLEND);
    }

    // Pixel grid (thin, all 8 boundaries)
    glColor3f(0.3f, 0.3f, 0.3f);
    glBegin(GL_LINES);
    for (int i = 0; i <= 8; i++) {
        float f = -1.0f + (i * 2.0f / 8.0f);
        glVertex2f(f, -1); glVertex2f(f, 1);
        glVertex2f(-1, f); glVertex2f(1, f);
    }
    glEnd();

    // MCM thick boundary lines at 2-pixel column boundaries
    if (mcm_overlay) {
        glLineWidth(2.0f);
        glColor3f(0.3f, 0.6f, 1.0f);
        glBegin(GL_LINES);
        for (int i = 0; i <= 8; i += 2) {
            float f = -1.0f + (i * 2.0f / 8.0f);
            glVertex2f(f, -1); glVertex2f(f, 1);
        }
        glEnd();
        glLineWidth(1.0f);
    }

    // Hover highlight
    if (m_hover_px >= 0 && m_hover_px < 8 && m_hover_py >= 0 && m_hover_py < 8) {
        bool mcm = m_pane->IsMCM();
        int hpx = mcm ? (m_hover_px & ~1) : m_hover_px;
        int hcols = mcm ? 2 : 1;
        float hx = -1.0f + (hpx * 2.0f / 8.0f);
        float hy = 1.0f - ((m_hover_py + 1) * 2.0f / 8.0f);
        float hw = hcols * 2.0f / 8.0f;
        float hh = 2.0f / 8.0f;

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glColor4f(1.0f, 1.0f, 1.0f, 0.18f);
        glBegin(GL_QUADS);
        glVertex2f(hx, hy); glVertex2f(hx + hw, hy);
        glVertex2f(hx + hw, hy + hh); glVertex2f(hx, hy + hh);
        glEnd();
        glDisable(GL_BLEND);
        glLineWidth(2.0f);
        glColor3f(1.0f, 1.0f, 0.3f);
        glBegin(GL_LINE_LOOP);
        glVertex2f(hx, hy); glVertex2f(hx + hw, hy);
        glVertex2f(hx + hw, hy + hh); glVertex2f(hx, hy + hh);
        glEnd();
        glLineWidth(1.0f);
    }

    SwapBuffers();
}

void CharBigEditor::OnMouseLeftDown(wxMouseEvent& event) {
    SetFocus();
    if (m_pane->IsLocked()) return;
    int px = event.GetX() * 8 / m_last_width;
    int py = event.GetY() * 8 / m_last_height;
    m_last_px = px; m_last_py = py;
    PushUndo();
    DoEdit(px, py);
}

void CharBigEditor::OnMouseMove(wxMouseEvent& event) {
    int px = event.GetX() * 8 / m_last_width;
    int py = event.GetY() * 8 / m_last_height;

    if (px != m_hover_px || py != m_hover_py) {
        m_hover_px = px;
        m_hover_py = py;
        Refresh();
    }

    if (!m_pane->IsLocked() && event.LeftIsDown() && (px != m_last_px || py != m_last_py)) {
        DoEdit(px, py);
        m_last_px = px; m_last_py = py;
    }
}

void CharBigEditor::DoEdit(int x, int y) {
    if (x < 0 || x >= 8 || y < 0 || y >= 8) return;
    uint16_t addr = m_pane->GetCharSetAddr() + m_index * 8 + y;
    uint8_t val = sim_mem_read_byte(m_sim, addr);

    bool mcm = m_pane->IsMCM();
    if (!mcm) {
        val ^= (1 << (7 - x));
    } else {
        // Multi-color edit: cycles through 00, 01, 10, 11
        int shift = 6 - (x & 6);
        int bits = (val >> shift) & 0x3;
        bits = (bits + 1) & 0x3;
        val = (val & ~(0x3 << shift)) | (bits << shift);
    }
    sim_mem_write_byte(m_sim, addr, val);
    m_pane->RefreshAll();
}

void CharBigEditor::PushUndo() {
    UndoEntry entry;
    entry.charIndex = m_index;
    uint16_t base = m_pane->GetCharSetAddr() + m_index * 8;
    for (int i = 0; i < 8; i++)
        entry.bytes[i] = sim_mem_read_byte(m_sim, base + i);
    m_undoStack.push_back(entry);
    if (m_undoStack.size() > 50)
        m_undoStack.erase(m_undoStack.begin());
}

void CharBigEditor::DoUndo() {
    if (m_undoStack.empty()) return;
    UndoEntry entry = m_undoStack.back();
    m_undoStack.pop_back();
    uint16_t base = m_pane->GetCharSetAddr() + entry.charIndex * 8;
    for (int i = 0; i < 8; i++)
        sim_mem_write_byte(m_sim, base + i, entry.bytes[i]);
    m_index = entry.charIndex;
    m_pane->RefreshAll();
}

void CharBigEditor::OnMouseLeave(wxMouseEvent& WXUNUSED(event)) {
    m_hover_px = -1;
    m_hover_py = -1;
    Refresh();
}

void CharBigEditor::OnKeyDown(wxKeyEvent& event) {
    if (event.ControlDown()) {
        switch (event.GetKeyCode()) {
        case 'Z': DoUndo();          return;
        case 'C': m_pane->CopyChar();  return;
        case 'V': m_pane->PasteChar(); return;
        }
    }
    event.Skip();
}

// --- CharVirtualScreen ---

CharVirtualScreen::CharVirtualScreen(PaneVICChars* parent, sim_session_t* sim)
    : wxGLCanvas(parent, wxID_ANY, NULL, wxDefaultPosition, wxSize(320, 200)),
      m_pane(parent), m_sim(sim), m_texture(0), m_glInitialized(false),
      m_last_width(320), m_last_height(200)
{
    m_context = new wxGLContext(this);
    m_pixels = new uint8_t[320 * 200 * 4]();
    Bind(wxEVT_PAINT, &CharVirtualScreen::OnPaint, this);
    Bind(wxEVT_LEFT_DOWN, &CharVirtualScreen::OnMouseLeftDown, this);
    Bind(wxEVT_MOTION, &CharVirtualScreen::OnMouseMove, this);
}

CharVirtualScreen::~CharVirtualScreen() {
    delete[] m_pixels;
    delete m_context;
}

void CharVirtualScreen::InitGL() {
    SetCurrent(*m_context);
    glGenTextures(1, &m_texture);
    glBindTexture(GL_TEXTURE_2D, m_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    m_glInitialized = true;
}

void CharVirtualScreen::OnPaint(wxPaintEvent& WXUNUSED(event)) {
    wxPaintDC dc(this);
    SetCurrent(*m_context);
    if (!m_glInitialized) InitGL();

    uint16_t base = m_pane->GetCharSetAddr();
    bool mcm = m_pane->IsMCM();
    uint8_t c0 = m_pane->GetBGColor();
    uint8_t c1 = m_pane->GetMC1Color();
    uint8_t c2 = m_pane->GetMC2Color();
    uint8_t c3 = m_pane->GetCharColor();

    for (int sy = 0; sy < 25; sy++) {
        for (int sx = 0; sx < 40; sx++) {
            uint8_t idx = m_pane->GetScratchpadChar(sx, sy);
            uint8_t char_buf[8 * 8 * 4];
            sim_vic_render_char(m_sim, base, idx, mcm, c0, c1, c2, c3, char_buf);
            
            for (int y = 0; y < 8; y++) {
                for (int x = 0; x < 8; x++) {
                    int src = (y * 8 + x) * 4;
                    int dst = ((sy * 8 + y) * 320 + (sx * 8 + x)) * 4;
                    memcpy(&m_pixels[dst], &char_buf[src], 4);
                }
            }
        }
    }

    glClear(GL_COLOR_BUFFER_BIT);
    wxSize sz = GetClientSize();
    m_last_width = sz.x; m_last_height = sz.y;
    double scale = GetContentScaleFactor();
    glViewport(0, 0, (int)(sz.x * scale), (int)(sz.y * scale));
    
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, m_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 320, 200, 0, GL_RGBA, GL_UNSIGNED_BYTE, m_pixels);

    glBegin(GL_QUADS);
    glTexCoord2f(0, 1); glVertex2f(-1, -1);
    glTexCoord2f(1, 1); glVertex2f(1, -1);
    glTexCoord2f(1, 0); glVertex2f(1, 1);
    glTexCoord2f(0, 0); glVertex2f(-1, 1);
    glEnd();

    SwapBuffers();
}

void CharVirtualScreen::OnMouseLeftDown(wxMouseEvent& event) {
    int sx = event.GetX() * 40 / m_last_width;
    int sy = event.GetY() * 25 / m_last_height;
    if (sx >= 0 && sx < 40 && sy >= 0 && sy < 25) {
        m_pane->SetScratchpadChar(sx, sy, m_pane->GetSelectedChar());
    }
}

void CharVirtualScreen::OnMouseMove(wxMouseEvent& event) {
    if (event.LeftIsDown()) {
        int sx = event.GetX() * 40 / m_last_width;
        int sy = event.GetY() * 25 / m_last_height;
        if (sx >= 0 && sx < 40 && sy >= 0 && sy < 25) {
            m_pane->SetScratchpadChar(sx, sy, m_pane->GetSelectedChar());
        }
    }
}

enum {
    ID_CHAR_LOCK = wxID_HIGHEST + 1,
    ID_CHAR_CLEAR,
    ID_CHAR_LOAD,
    ID_CHAR_SAVE,
    ID_CHAR_SHIFT_UP,
    ID_CHAR_SHIFT_DOWN,
    ID_CHAR_SHIFT_LEFT,
    ID_CHAR_SHIFT_RIGHT,
    ID_CHAR_FLIP_H,
    ID_CHAR_FLIP_V,
    ID_CHAR_ROT_CW,
    ID_CHAR_ROT_CCW,
    ID_CHAR_MCM_GRID,
    ID_CHAR_EXPORT_ASM,
    ID_CHAR_EXPORT_PNG,
    ID_CHAR_COPY,
    ID_CHAR_PASTE
};

// --- PaneVICChars ---

PaneVICChars::PaneVICChars(wxWindow* parent, sim_session_t *sim)
    : SimPane(parent, sim), m_locked(true), m_selectedChar(0), m_lblEditChar(nullptr),
      m_showMCMGrid(true), m_hasClipboard(false)
{
    memset(m_scratchpad, 32, 1000); // Initialize with spaces (char 32)

    wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);
    
    m_toolBar = new wxToolBar(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTB_HORIZONTAL | wxTB_FLAT);
    m_toolBar->AddCheckTool(ID_CHAR_LOCK, "Enable Editing", wxArtProvider::GetBitmap(wxART_EDIT, wxART_TOOLBAR), wxNullBitmap, "Enable pixel editing of character data");
    m_toolBar->AddSeparator();
    m_toolBar->AddTool(ID_CHAR_CLEAR, "Clear Scratchpad", wxArtProvider::GetBitmap(wxART_DELETE, wxART_TOOLBAR), "Clear the 40x25 virtual screen below");
    m_toolBar->AddSeparator();
    m_toolBar->AddTool(ID_CHAR_LOAD, "Load Charset", wxArtProvider::GetBitmap(wxART_FILE_OPEN, wxART_TOOLBAR), "Load a 2KB binary charset into memory");
    m_toolBar->AddTool(ID_CHAR_SAVE, "Save Charset", wxArtProvider::GetBitmap(wxART_FILE_SAVE, wxART_TOOLBAR), "Save current 2KB charset to a binary file");

    m_toolBar->AddSeparator();
    m_toolBar->AddTool(ID_CHAR_EXPORT_ASM, "Export ASM", wxArtProvider::GetBitmap(wxART_REPORT_VIEW, wxART_TOOLBAR), "Export charset as KickAssembler .byte statements");
    m_toolBar->AddTool(ID_CHAR_EXPORT_PNG, "Export PNG", wxArtProvider::GetBitmap(wxART_NORMAL_FILE, wxART_TOOLBAR), "Export charset as a 128x128 PNG sprite sheet");
    m_toolBar->AddSeparator();
    m_toolBar->AddCheckTool(ID_CHAR_MCM_GRID, "MCM Grid", wxArtProvider::GetBitmap(wxART_LIST_VIEW, wxART_TOOLBAR), wxNullBitmap, "Toggle MCM 2-pixel column pair overlay on the editor");
    m_toolBar->ToggleTool(ID_CHAR_MCM_GRID, m_showMCMGrid);
    m_toolBar->Realize();
    mainSizer->Add(m_toolBar, 0, wxEXPAND);

    wxToolBar* editTB = new wxToolBar(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                      wxTB_HORIZONTAL | wxTB_FLAT | wxTB_TEXT | wxTB_NOICONS);
    editTB->AddControl(new wxStaticText(editTB, wxID_ANY, " Edit: "));
    editTB->AddTool(ID_CHAR_COPY,  wxT("Copy  \u2398"), wxNullBitmap, "Copy character data (Ctrl+C)");
    editTB->AddTool(ID_CHAR_PASTE, wxT("Paste \u2399"), wxNullBitmap, "Paste character data (Ctrl+V)");
    editTB->AddSeparator();
    editTB->AddControl(new wxStaticText(editTB, wxID_ANY, " Transform: "));
    editTB->AddTool(ID_CHAR_SHIFT_UP,    wxT("\u2191 Up"),    wxNullBitmap, "Shift pixels up (wrapping)");
    editTB->AddTool(ID_CHAR_SHIFT_DOWN,  wxT("\u2193 Down"),  wxNullBitmap, "Shift pixels down (wrapping)");
    editTB->AddTool(ID_CHAR_SHIFT_LEFT,  wxT("\u2190 Left"),  wxNullBitmap, "Shift pixels left (wrapping)");
    editTB->AddTool(ID_CHAR_SHIFT_RIGHT, wxT("\u2192 Right"), wxNullBitmap, "Shift pixels right (wrapping)");
    editTB->AddSeparator();
    editTB->AddTool(ID_CHAR_FLIP_H,   "Flip H",    wxNullBitmap, "Flip horizontally (mirror left\u2194right)");
    editTB->AddTool(ID_CHAR_FLIP_V,   "Flip V",    wxNullBitmap, "Flip vertically (mirror top\u2194bottom)");
    editTB->AddSeparator();
    editTB->AddTool(ID_CHAR_ROT_CW,  wxT("Rot \u21bb"),  wxNullBitmap, "Rotate 90\u00b0 clockwise");
    editTB->AddTool(ID_CHAR_ROT_CCW, wxT("Rot \u21ba"),  wxNullBitmap, "Rotate 90\u00b0 counter-clockwise");
    editTB->Realize();
    mainSizer->Add(editTB, 0, wxEXPAND);

    wxBoxSizer* topSizer = new wxBoxSizer(wxHORIZONTAL);
    
    wxBoxSizer* selectorSizer = new wxBoxSizer(wxVERTICAL);
    selectorSizer->Add(new wxStaticText(this, wxID_ANY, "Select Character:"), 0, wxALL, 2);
    m_selector = new CharSelector(this, sim);
    selectorSizer->Add(m_selector, 0, wxALL, 5);
    topSizer->Add(selectorSizer, 0, wxEXPAND);

    wxBoxSizer* editorSizer = new wxBoxSizer(wxVERTICAL);
    m_lblEditChar = new wxStaticText(this, wxID_ANY, "Pixel Editor - Char #0 ($00):");
    editorSizer->Add(m_lblEditChar, 0, wxALL, 2);
    m_bigEditor = new CharBigEditor(this, sim);
    editorSizer->Add(m_bigEditor, 0, wxALL, 5);
    topSizer->Add(editorSizer, 0, wxEXPAND);

    // Right side controls
    wxBoxSizer* ctrlSizer = new wxBoxSizer(wxVERTICAL);
    
    m_chkMCM = new wxCheckBox(this, wxID_ANY, "Multi-color Mode ($D016 bit 4)");
    ctrlSizer->Add(m_chkMCM, 0, wxALL, 5);

    // Address Mode
    wxStaticBoxSizer* addrBox = new wxStaticBoxSizer(wxVERTICAL, this, "Character Set Address");
    m_rbAddrAuto = new wxRadioButton(this, wxID_ANY, "Auto (from VIC-II regs)", wxDefaultPosition, wxDefaultSize, wxRB_GROUP);
    m_rbAddrManual = new wxRadioButton(this, wxID_ANY, "Manual Override:");
    m_txtAddrManual = new wxTextCtrl(this, wxID_ANY, "1000");
    m_txtAddrManual->Enable(false);
    
    addrBox->Add(m_rbAddrAuto, 0, wxALL, 2);
    wxBoxSizer* manualAddrSizer = new wxBoxSizer(wxHORIZONTAL);
    manualAddrSizer->Add(m_rbAddrManual, 0, wxALIGN_CENTER_VERTICAL | wxALL, 2);
    manualAddrSizer->Add(new wxStaticText(this, wxID_ANY, "$"), 0, wxALIGN_CENTER_VERTICAL);
    manualAddrSizer->Add(m_txtAddrManual, 1, wxALL, 2);
    addrBox->Add(manualAddrSizer, 0, wxEXPAND);
    
    m_lblAddr = new wxStaticText(this, wxID_ANY, "Effective: $1000");
    addrBox->Add(m_lblAddr, 0, wxALL, 2);
    ctrlSizer->Add(addrBox, 0, wxEXPAND | wxALL, 5);

    wxGridSizer* colGrid = new wxGridSizer(2, 5, 5);
    m_cpBG = new wxColourPickerCtrl(this, wxID_ANY, GetC64Color(sim_mem_read_byte(sim, 0xD021) & 0xF));
    colGrid->Add(new wxStaticText(this, wxID_ANY, "BG ($D021):"), 0, wxALIGN_CENTER_VERTICAL);
    colGrid->Add(m_cpBG, 0, wxEXPAND);

    m_cpMC1 = new wxColourPickerCtrl(this, wxID_ANY, GetC64Color(sim_mem_read_byte(sim, 0xD022) & 0xF));
    colGrid->Add(new wxStaticText(this, wxID_ANY, "MC1 ($D022):"), 0, wxALIGN_CENTER_VERTICAL);
    colGrid->Add(m_cpMC1, 0, wxEXPAND);

    m_cpMC2 = new wxColourPickerCtrl(this, wxID_ANY, GetC64Color(sim_mem_read_byte(sim, 0xD023) & 0xF));
    colGrid->Add(new wxStaticText(this, wxID_ANY, "MC2 ($D023):"), 0, wxALIGN_CENTER_VERTICAL);
    colGrid->Add(m_cpMC2, 0, wxEXPAND);

    m_cpChar = new wxColourPickerCtrl(this, wxID_ANY, GetC64Color(14)); /* light blue = C64 color RAM default */
    colGrid->Add(new wxStaticText(this, wxID_ANY, "Char Color:"), 0, wxALIGN_CENTER_VERTICAL);
    colGrid->Add(m_cpChar, 0, wxEXPAND);
    
    ctrlSizer->Add(colGrid, 0, wxEXPAND | wxALL, 5);

    m_lblHelp = new wxStaticText(this, wxID_ANY, 
        "Virtual Screen Scratchpad (40x25):\nClick/Drag to place selected character below.");
    m_lblHelp->SetForegroundColour(*wxBLUE);
    ctrlSizer->Add(m_lblHelp, 0, wxALL, 5);

    topSizer->Add(ctrlSizer, 1, wxEXPAND);
    mainSizer->Add(topSizer, 0, wxEXPAND);

    m_virtualScreen = new CharVirtualScreen(this, sim);
    mainSizer->Add(m_virtualScreen, 1, wxEXPAND | wxALL, 5);

    SetSizer(mainSizer);

    m_chkMCM->Bind(wxEVT_CHECKBOX, &PaneVICChars::OnToggleMCM, this);
    m_rbAddrAuto->Bind(wxEVT_RADIOBUTTON, &PaneVICChars::OnAddrModeChanged, this);
    m_rbAddrManual->Bind(wxEVT_RADIOBUTTON, &PaneVICChars::OnAddrModeChanged, this);
    m_txtAddrManual->Bind(wxEVT_TEXT, &PaneVICChars::OnAddrModeChanged, this);
    m_cpBG->Bind(wxEVT_COLOURPICKER_CHANGED, &PaneVICChars::OnColorChanged, this);
    m_cpMC1->Bind(wxEVT_COLOURPICKER_CHANGED, &PaneVICChars::OnColorChanged, this);
    m_cpMC2->Bind(wxEVT_COLOURPICKER_CHANGED, &PaneVICChars::OnColorChanged, this);
    m_cpChar->Bind(wxEVT_COLOURPICKER_CHANGED, &PaneVICChars::OnColorChanged, this);
    Bind(wxEVT_TOOL, &PaneVICChars::OnToggleLock, this, ID_CHAR_LOCK);
    Bind(wxEVT_TOOL, &PaneVICChars::OnClearScratchpad, this, ID_CHAR_CLEAR);
    Bind(wxEVT_TOOL, &PaneVICChars::OnLoadCharSet, this, ID_CHAR_LOAD);
    Bind(wxEVT_TOOL, &PaneVICChars::OnSaveCharSet, this, ID_CHAR_SAVE);
    Bind(wxEVT_TOOL, &PaneVICChars::OnCharTransform, this, ID_CHAR_SHIFT_UP);
    Bind(wxEVT_TOOL, &PaneVICChars::OnCharTransform, this, ID_CHAR_SHIFT_DOWN);
    Bind(wxEVT_TOOL, &PaneVICChars::OnCharTransform, this, ID_CHAR_SHIFT_LEFT);
    Bind(wxEVT_TOOL, &PaneVICChars::OnCharTransform, this, ID_CHAR_SHIFT_RIGHT);
    Bind(wxEVT_TOOL, &PaneVICChars::OnCharTransform, this, ID_CHAR_FLIP_H);
    Bind(wxEVT_TOOL, &PaneVICChars::OnCharTransform, this, ID_CHAR_FLIP_V);
    Bind(wxEVT_TOOL, &PaneVICChars::OnCharTransform, this, ID_CHAR_ROT_CW);
    Bind(wxEVT_TOOL, &PaneVICChars::OnCharTransform, this, ID_CHAR_ROT_CCW);
    Bind(wxEVT_TOOL, &PaneVICChars::OnToggleMCMGrid,  this, ID_CHAR_MCM_GRID);
    Bind(wxEVT_TOOL, &PaneVICChars::OnExportASM,      this, ID_CHAR_EXPORT_ASM);
    Bind(wxEVT_TOOL, &PaneVICChars::OnExportPNG,      this, ID_CHAR_EXPORT_PNG);
    Bind(wxEVT_TOOL, &PaneVICChars::OnCopyChar,       this, ID_CHAR_COPY);
    Bind(wxEVT_TOOL, &PaneVICChars::OnPasteChar,      this, ID_CHAR_PASTE);
}

void PaneVICChars::RefreshPane(const SimSnapshot &snap) {
    (void)snap;
    RefreshAll();
}

void PaneVICChars::RefreshAll() {
    uint16_t addr = GetCharSetAddr();
    m_lblAddr->SetLabel(wxString::Format("Effective: $%04X (Selected Char @ $%04X)", addr, addr + m_selectedChar * 8));
    if (m_lblEditChar)
        m_lblEditChar->SetLabel(wxString::Format("Pixel Editor - Char #%d ($%02X)%s:",
            m_selectedChar, m_selectedChar, m_locked ? " [read-only]" : " [click to edit]"));
    
    // Update MCM checkbox to match sim if not just edited
    m_chkMCM->SetValue(IsMCM());

    m_selector->RefreshSelector();
    m_bigEditor->RefreshEditor();
    m_virtualScreen->RefreshScreen();
}

void PaneVICChars::SetSelectedChar(int index) {
    m_selectedChar = index;
    m_bigEditor->SetCharIndex(index);
    RefreshAll();
}

uint16_t PaneVICChars::GetCharSetAddr() {
    if (m_rbAddrManual->GetValue()) {
        long addr = 0;
        m_txtAddrManual->GetValue().ToLong(&addr, 16);
        return (uint16_t)(addr & 0xFFFF);
    }
    // TODO (vic2.h step 3): for Mega65, read CHARPTR ($D068 lo, $D069 mid, $D06A hi)
    // to form a 20-bit address instead of using the D018/DD00 VIC-II bank path.
    // The return type will also need widening to uint32_t at that point.
    uint8_t d018 = sim_mem_read_byte(m_sim, 0xD018);
    uint8_t dd00 = sim_mem_read_byte(m_sim, 0xDD00);
    uint32_t bank = (uint32_t)((~dd00) & 0x03) * 0x4000;
    uint32_t char_base = (uint32_t)((d018 >> 1) & 0x07) * 0x0800;
    return (uint16_t)((bank + char_base) & 0xFFFF);
}

uint8_t PaneVICChars::GetBGColor() {
    // We could read from $D021, but it's better to use the picker's state if we want to preview colors.
    // However, the prompt says "Allow all 256 characters to be implemented".
    // I'll stick to picking from the color picker for now, as it allows trying colors without changing sim state.
    // But usually these panes should reflect sim state.
    // Let's use the picker as an override or sync it.
    // For now, I'll just return the index that matches the picker's color if possible, or just read $D021.
    return sim_mem_read_byte(m_sim, 0xD021) & 0x0F;
}

uint8_t PaneVICChars::GetMC1Color() { return sim_mem_read_byte(m_sim, 0xD022) & 0x0F; }
uint8_t PaneVICChars::GetMC2Color() { return sim_mem_read_byte(m_sim, 0xD023) & 0x0F; }
uint8_t PaneVICChars::GetCharColor() {
    wxColour col = m_cpChar->GetColour();
    int best_ci = 1; // Default to white
    int min_dist = 1000000;
    for (int i=0; i<16; i++) {
        int dr = col.Red() - vic2_palette[i][0];
        int dg = col.Green() - vic2_palette[i][1];
        int db = col.Blue() - vic2_palette[i][2];
        int dist = dr*dr + dg*dg + db*db;
        if (dist < min_dist) { min_dist = dist; best_ci = i; }
    }
    return best_ci;
}

bool PaneVICChars::IsMCM() {
    return (sim_mem_read_byte(m_sim, 0xD016) & 0x10) != 0;
}

void PaneVICChars::OnAddrModeChanged(wxCommandEvent& WXUNUSED(event)) {
    m_txtAddrManual->Enable(m_rbAddrManual->GetValue());
    RefreshAll();
}

void PaneVICChars::OnToggleLock(wxCommandEvent& event) {
    m_locked = !event.IsChecked();
}

void PaneVICChars::OnToggleMCM(wxCommandEvent& WXUNUSED(event)) {
    // If we want to change the actual sim state:
    uint8_t d016 = sim_mem_read_byte(m_sim, 0xD016);
    if (m_chkMCM->IsChecked()) d016 |= 0x10;
    else d016 &= ~0x10;
    sim_mem_write_byte(m_sim, 0xD016, d016);
    RefreshAll();
}

void PaneVICChars::OnColorChanged(wxCommandEvent& event) {
    if (m_locked) return;
    wxColourPickerCtrl* cp = wxDynamicCast(event.GetEventObject(), wxColourPickerCtrl);
    if (!cp) return;
    wxColour col = cp->GetColour();
    
    int best_ci = 0;
    int min_dist = 1000000;
    for (int i=0; i<16; i++) {
        int dr = col.Red() - vic2_palette[i][0];
        int dg = col.Green() - vic2_palette[i][1];
        int db = col.Blue() - vic2_palette[i][2];
        int dist = dr*dr + dg*dg + db*db;
        if (dist < min_dist) { min_dist = dist; best_ci = i; }
    }

    if (cp == m_cpBG) sim_mem_write_byte(m_sim, 0xD021, best_ci);
    else if (cp == m_cpMC1) sim_mem_write_byte(m_sim, 0xD022, best_ci);
    else if (cp == m_cpMC2) sim_mem_write_byte(m_sim, 0xD023, best_ci);
    // m_cpChar is just for preview as char colors are per-char in Color RAM

    RefreshAll();
}

void PaneVICChars::OnCharTransform(wxCommandEvent& event) {
    ApplyTransform(event.GetId());
}

void PaneVICChars::ApplyTransform(int op) {
    if (m_locked) return;
    m_bigEditor->PushUndo();

    uint16_t base = GetCharSetAddr() + m_selectedChar * 8;
    uint8_t b[8], r[8];
    for (int i = 0; i < 8; i++)
        b[i] = sim_mem_read_byte(m_sim, base + i);

    switch (op) {
    case ID_CHAR_SHIFT_UP:
        for (int i = 0; i < 7; i++) r[i] = b[i + 1];
        r[7] = b[0];
        break;
    case ID_CHAR_SHIFT_DOWN:
        for (int i = 1; i < 8; i++) r[i] = b[i - 1];
        r[0] = b[7];
        break;
    case ID_CHAR_SHIFT_LEFT:
        for (int i = 0; i < 8; i++) r[i] = (b[i] << 1) | (b[i] >> 7);
        break;
    case ID_CHAR_SHIFT_RIGHT:
        for (int i = 0; i < 8; i++) r[i] = (b[i] >> 1) | (b[i] << 7);
        break;
    case ID_CHAR_FLIP_H:
        for (int i = 0; i < 8; i++) {
            uint8_t v = b[i], f = 0;
            for (int j = 0; j < 8; j++) f |= ((v >> j) & 1) << (7 - j);
            r[i] = f;
        }
        break;
    case ID_CHAR_FLIP_V:
        for (int i = 0; i < 8; i++) r[i] = b[7 - i];
        break;
    case ID_CHAR_ROT_CW:
        memset(r, 0, 8);
        for (int row = 0; row < 8; row++)
            for (int col = 0; col < 8; col++)
                if ((b[row] >> (7 - col)) & 1)
                    r[col] |= 1 << row; // new_row=col, new_col=7-row → bit (7-(7-row))=row
        break;
    case ID_CHAR_ROT_CCW:
        memset(r, 0, 8);
        for (int row = 0; row < 8; row++)
            for (int col = 0; col < 8; col++)
                if ((b[row] >> (7 - col)) & 1)
                    r[7 - col] |= 1 << (7 - row); // new_row=7-col, new_col=row → bit (7-row)
        break;
    default:
        return;
    }

    for (int i = 0; i < 8; i++)
        sim_mem_write_byte(m_sim, base + i, r[i]);
    RefreshAll();
}

void PaneVICChars::CopyChar() {
    uint16_t base = GetCharSetAddr() + m_selectedChar * 8;
    for (int i = 0; i < 8; i++)
        m_clipboard[i] = sim_mem_read_byte(m_sim, base + i);
    m_hasClipboard = true;
}

void PaneVICChars::PasteChar() {
    if (!m_hasClipboard || m_locked) return;
    m_bigEditor->PushUndo();
    uint16_t base = GetCharSetAddr() + m_selectedChar * 8;
    for (int i = 0; i < 8; i++)
        sim_mem_write_byte(m_sim, base + i, m_clipboard[i]);
    RefreshAll();
}

void PaneVICChars::OnCopyChar(wxCommandEvent& WXUNUSED(event)) {
    CopyChar();
}

void PaneVICChars::OnPasteChar(wxCommandEvent& WXUNUSED(event)) {
    PasteChar();
}


void PaneVICChars::OnToggleMCMGrid(wxCommandEvent& event) {
    m_showMCMGrid = event.IsChecked();
    m_bigEditor->RefreshEditor();
}

void PaneVICChars::OnExportASM(wxCommandEvent& WXUNUSED(event)) {
    wxFileDialog dlg(this, "Export Charset as Assembly", "", "charset.asm",
                     "Assembly files (*.asm)|*.asm|All files (*)|*",
                     wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (dlg.ShowModal() == wxID_CANCEL) return;

    FILE* f = fopen(dlg.GetPath().mb_str(), "w");
    if (!f) return;

    uint16_t base = GetCharSetAddr();
    fprintf(f, "; VIC-II Character Set - exported from 6502 Simulator\n");
    fprintf(f, "; Base address: $%04X  (%d characters x 8 bytes)\n\n", base, 256);
    fprintf(f, "charSet:\n");
    for (int ch = 0; ch < 256; ch++) {
        fprintf(f, "    ; Char $%02X\n    .byte", ch);
        for (int row = 0; row < 8; row++) {
            uint8_t v = sim_mem_read_byte(m_sim, (uint16_t)(base + ch * 8 + row));
            fprintf(f, "%s$%02x", row == 0 ? " " : ",$", v);
        }
        fputc('\n', f);
    }
    fclose(f);
}

void PaneVICChars::OnExportPNG(wxCommandEvent& WXUNUSED(event)) {
    wxFileDialog dlg(this, "Export Charset as PNG", "", "charset.png",
                     "PNG files (*.png)|*.png|All files (*)|*",
                     wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (dlg.ShowModal() == wxID_CANCEL) return;

    if (!wxImage::FindHandler(wxBITMAP_TYPE_PNG))
        wxImage::AddHandler(new wxPNGHandler());

    // Render full 256-char sheet at 8x scale → 1024x1024
    const int SCALE = 8;
    const int W = 128 * SCALE, H = 128 * SCALE;
    uint16_t base = GetCharSetAddr();
    bool mcm = IsMCM();
    uint8_t c0 = GetBGColor(), c1 = GetMC1Color(), c2 = GetMC2Color(), c3 = GetCharColor();

    uint8_t* rgb = new uint8_t[W * H * 3]();
    for (int cy = 0; cy < 16; cy++) {
        for (int cx = 0; cx < 16; cx++) {
            uint8_t char_buf[8 * 8 * 4];
            sim_vic_render_char(m_sim, base, cy * 16 + cx, mcm, c0, c1, c2, c3, char_buf);
            for (int py = 0; py < 8; py++) {
                for (int px = 0; px < 8; px++) {
                    uint8_t r = char_buf[(py * 8 + px) * 4 + 0];
                    uint8_t g = char_buf[(py * 8 + px) * 4 + 1];
                    uint8_t b = char_buf[(py * 8 + px) * 4 + 2];
                    int base_x = (cx * 8 + px) * SCALE;
                    int base_y = (cy * 8 + py) * SCALE;
                    for (int sy = 0; sy < SCALE; sy++)
                        for (int sx = 0; sx < SCALE; sx++) {
                            int idx = ((base_y + sy) * W + (base_x + sx)) * 3;
                            rgb[idx] = r; rgb[idx+1] = g; rgb[idx+2] = b;
                        }
                }
            }
        }
    }

    wxImage img(W, H, rgb); // wxImage takes ownership of rgb
    img.SaveFile(dlg.GetPath(), wxBITMAP_TYPE_PNG);
}

void PaneVICChars::OnClearScratchpad(wxCommandEvent& WXUNUSED(event)) {
    memset(m_scratchpad, 32, 1000);
    RefreshAll();
}

void PaneVICChars::OnLoadCharSet(wxCommandEvent& WXUNUSED(event)) {
    if (m_locked) return;
    wxFileDialog openDir(this, "Load Character Set", "", "",
                         "Binary files (*.bin)|*.bin", wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (openDir.ShowModal() == wxID_CANCEL) return;

    FILE* f = fopen(openDir.GetPath().mb_str(), "rb");
    if (f) {
        uint8_t buf[2048];
        size_t n = fread(buf, 1, 2048, f);
        fclose(f);
        uint16_t base = GetCharSetAddr();
        for (size_t i=0; i<n && i<2048; i++) sim_mem_write_byte(m_sim, (uint16_t)(base + i), buf[i]);
        RefreshAll();
    }
}

void PaneVICChars::OnSaveCharSet(wxCommandEvent& WXUNUSED(event)) {
    wxFileDialog saveDir(this, "Save Character Set", "", "charset.bin",
                         "Binary files (*.bin)|*.bin", wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (saveDir.ShowModal() == wxID_CANCEL) return;

    uint16_t base = GetCharSetAddr();
    uint8_t buf[2048];
    for (int i=0; i<2048; i++) buf[i] = sim_mem_read_byte(m_sim, (uint16_t)(base + i));

    FILE* f = fopen(saveDir.GetPath().mb_str(), "wb");
    if (f) {
        fwrite(buf, 1, 2048, f);
        fclose(f);
    }
}
