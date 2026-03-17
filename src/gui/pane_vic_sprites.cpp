#include "pane_vic_sprites.h"
#include <wx/sizer.h>
#include <wx/artprov.h>
#include <wx/tooltip.h>
#include <wx/accel.h>
#include <wx/filedlg.h>
#include <wx/checkbox.h>
#include <wx/stattext.h>
#include <wx/button.h>
#include <wx/clrpicker.h>
#include "vic2.h"

// --- SpriteThumbnail ---

SpriteThumbnail::SpriteThumbnail(PaneVICSprites* parent, sim_session_t* sim, int index)
    : wxGLCanvas(parent, wxID_ANY, NULL, wxDefaultPosition, wxSize(48, 48)),
      m_pane(parent), m_sim(sim), m_index(index), m_texture(0), m_glInitialized(false), m_selected(false)
{
    m_context = new wxGLContext(this);
    m_pixels = new uint8_t[24 * 21 * 4](); // RGBA
    Bind(wxEVT_PAINT, &SpriteThumbnail::OnPaint, this);
    Bind(wxEVT_LEFT_DOWN, &SpriteThumbnail::OnMouseLeftDown, this);
    SetToolTip(wxString::Format("Sprite %d", index));
}

SpriteThumbnail::~SpriteThumbnail() {
    delete[] m_pixels;
    delete m_context;
}

void SpriteThumbnail::InitGL() {
    SetCurrent(*m_context);
    glGenTextures(1, &m_texture);
    glBindTexture(GL_TEXTURE_2D, m_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    m_glInitialized = true;
}

void SpriteThumbnail::RefreshSprite() {
    Refresh();
}

void SpriteThumbnail::OnPaint(wxPaintEvent& WXUNUSED(event)) {
    wxPaintDC dc(this);
    SetCurrent(*m_context);
    if (!m_glInitialized) InitGL();

    sim_vic_render_sprite(m_sim, m_index, m_pixels);

    if (m_selected) glClearColor(0.8f, 0.8f, 0.2f, 1.0f);
    else glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    
    wxSize sz = GetClientSize();
    double scale = GetContentScaleFactor();
    glViewport(0, 0, (int)(sz.x * scale), (int)(sz.y * scale));
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glBindTexture(GL_TEXTURE_2D, m_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 24, 21, 0, GL_RGBA, GL_UNSIGNED_BYTE, m_pixels);

    glBegin(GL_QUADS);
    glTexCoord2f(0, 1); glVertex2f(-0.9f, -0.9f);
    glTexCoord2f(1, 1); glVertex2f(0.9f, -0.9f);
    glTexCoord2f(1, 0); glVertex2f(0.9f, 0.9f);
    glTexCoord2f(0, 0); glVertex2f(-0.9f, 0.9f);
    glEnd();

    SwapBuffers();
}

void SpriteThumbnail::OnMouseLeftDown(wxMouseEvent& WXUNUSED(event)) {
    m_pane->SetSelectedSprite(m_index);
}

// --- SpriteBigEditor ---

SpriteBigEditor::SpriteBigEditor(PaneVICSprites* parent, sim_session_t* sim)
    : wxGLCanvas(parent, wxID_ANY, NULL, wxDefaultPosition, wxSize(384, 336)),
      m_pane(parent), m_sim(sim), m_index(0), m_texture(0), m_glInitialized(false),
      m_last_px(-1), m_last_py(-1), m_last_width(384), m_last_height(336)
{
    m_context = new wxGLContext(this);
    m_pixels = new uint8_t[24 * 21 * 4](); // RGBA
    Bind(wxEVT_PAINT, &SpriteBigEditor::OnPaint, this);
    Bind(wxEVT_LEFT_DOWN, &SpriteBigEditor::OnMouseLeftDown, this);
    Bind(wxEVT_MOTION, &SpriteBigEditor::OnMouseMove, this);
}

SpriteBigEditor::~SpriteBigEditor() {
    delete[] m_pixels;
    delete m_context;
}

void SpriteBigEditor::InitGL() {
    SetCurrent(*m_context);
    glGenTextures(1, &m_texture);
    glBindTexture(GL_TEXTURE_2D, m_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    m_glInitialized = true;
}

void SpriteBigEditor::OnPaint(wxPaintEvent& WXUNUSED(event)) {
    wxPaintDC dc(this);
    SetCurrent(*m_context);
    if (!m_glInitialized) InitGL();

    sim_vic_render_sprite(m_sim, m_index, m_pixels);

    glClearColor(0.05f, 0.05f, 0.05f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    
    wxSize sz = GetClientSize();
    m_last_width = sz.x;
    m_last_height = sz.y;
    double scale = GetContentScaleFactor();
    glViewport(0, 0, (int)(sz.x * scale), (int)(sz.y * scale));
    
    // Background for transparency
    glDisable(GL_TEXTURE_2D);
    glBegin(GL_QUADS);
    glColor3f(0.1f, 0.1f, 0.1f);
    glVertex2f(-1.0f, -1.0f); glVertex2f(1.0f, -1.0f);
    glVertex2f(1.0f, 1.0f); glVertex2f(-1.0f, 1.0f);
    glEnd();

    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

    glBindTexture(GL_TEXTURE_2D, m_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 24, 21, 0, GL_RGBA, GL_UNSIGNED_BYTE, m_pixels);

    glBegin(GL_QUADS);
    glTexCoord2f(0, 1); glVertex2f(-1.0f, -1.0f);
    glTexCoord2f(1, 1); glVertex2f(1.0f, -1.0f);
    glTexCoord2f(1, 0); glVertex2f(1.0f, 1.0f);
    glTexCoord2f(0, 0); glVertex2f(-1.0f, 1.0f);
    glEnd();

    // Grid
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);
    glColor3f(0.25f, 0.25f, 0.25f);
    glBegin(GL_LINES);
    for (int x = 0; x <= 24; x++) {
        float fx = -1.0f + (x * 2.0f / 24.0f);
        glVertex2f(fx, -1.0f); glVertex2f(fx, 1.0f);
    }
    for (int y = 0; y <= 21; y++) {
        float fy = -1.0f + (y * 2.0f / 21.0f);
        glVertex2f(-1.0f, fy); glVertex2f(1.0f, fy);
    }
    glEnd();

    SwapBuffers();
}

void SpriteBigEditor::OnMouseLeftDown(wxMouseEvent& event) {
    if (m_pane->IsLocked()) return;
    
    wxPoint pos = event.GetPosition();

    if (m_last_width <= 0 || m_last_height <= 0) return;

    int px = pos.x * 24 / m_last_width;
    int py = pos.y * 21 / m_last_height;

    m_last_px = px;
    m_last_py = py;
    DoEdit(px, py);
}

void SpriteBigEditor::OnMouseMove(wxMouseEvent& event) {
    if (m_pane->IsLocked() || !event.LeftIsDown()) {
        m_last_px = -1; m_last_py = -1;
        return;
    }
    
    wxPoint pos = event.GetPosition();
    if (m_last_width <= 0 || m_last_height <= 0) return;
    
    int px = pos.x * 24 / m_last_width;
    int py = pos.y * 21 / m_last_height;
    
    if (px != m_last_px || py != m_last_py) {
        DoEdit(px, py);
        m_last_px = px;
        m_last_py = py;
    }
}

// --- PaneVICSprites ---

static wxColour GetC64Color(int ci) {
    const uint8_t* p = vic2_palette[ci & 0xF];
    return wxColour(p[0], p[1], p[2]);
}

PaneVICSprites::PaneVICSprites(wxWindow* parent, sim_session_t* sim)
    : SimPane(parent, sim), m_locked(true), m_selectedSprite(0) 
{
    wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);
    
    m_toolBar = new wxToolBar(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTB_HORIZONTAL | wxTB_FLAT);
    m_toolBar->AddCheckTool(2001, "Lock", wxArtProvider::GetBitmap(wxART_TICK_MARK), wxNullBitmap, "Lock/Unlock editing");
    m_toolBar->AddTool(2002, "Undo", wxArtProvider::GetBitmap(wxART_UNDO), "Undo (Ctrl+Z)");
    m_toolBar->AddSeparator();
    m_toolBar->AddTool(2003, "Rotate 90", wxArtProvider::GetBitmap(wxART_REDO), "Rotate 90° (Clips to 21x21)");
    m_toolBar->AddTool(2004, "Rotate 180", wxArtProvider::GetBitmap(wxART_GO_DOWN), "Rotate 180°");
    m_toolBar->AddTool(2005, "Invert", wxArtProvider::GetBitmap(wxART_CROSS_MARK), "Invert all pixels");
    m_toolBar->AddSeparator();
    m_toolBar->AddTool(2006, "Load Blob", wxArtProvider::GetBitmap(wxART_FILE_OPEN), "Load 64-byte sprite blob");
    m_toolBar->AddTool(2007, "Save Blob", wxArtProvider::GetBitmap(wxART_FILE_SAVE), "Save 64-byte sprite blob");
    
    m_toolBar->ToggleTool(2001, true);
    m_toolBar->Realize();
    mainSizer->Add(m_toolBar, 0, wxEXPAND);

    wxBoxSizer* contentSizer = new wxBoxSizer(wxHORIZONTAL);

    // Left: Thumbnails
    wxBoxSizer* leftSizer = new wxBoxSizer(wxVERTICAL);
    for (int i = 0; i < 8; i++) {
        m_thumbnails[i] = new SpriteThumbnail(this, m_sim, i);
        leftSizer->Add(m_thumbnails[i], 0, wxALL, 2);
    }
    contentSizer->Add(leftSizer, 0, wxEXPAND | wxALL, 5);

    // Middle: Big Editor
    m_bigEditor = new SpriteBigEditor(this, m_sim);
    contentSizer->Add(m_bigEditor, 1, wxEXPAND | wxALL, 5);

    // Right: Controls
    wxBoxSizer* rightSizer = new wxBoxSizer(wxVERTICAL);
    
    rightSizer->Add(new wxStaticText(this, wxID_ANY, "Settings:"), 0, wxTOP, 5);
    m_chkMCM = new wxCheckBox(this, 3001, "Multicolour Mode (2bpp)");
    rightSizer->Add(m_chkMCM, 0, wxALL, 5);

    rightSizer->Add(new wxStaticText(this, wxID_ANY, "Colors:"), 0, wxTOP, 10);
    wxFlexGridSizer* colGrid = new wxFlexGridSizer(3, 2, 5, 5);
    
    colGrid->Add(new wxStaticText(this, wxID_ANY, "Sprite:"));
    m_cpSprite = new wxColourPickerCtrl(this, 3002);
    colGrid->Add(m_cpSprite);

    colGrid->Add(new wxStaticText(this, wxID_ANY, "MC0:"));
    m_cpMC0 = new wxColourPickerCtrl(this, 3003);
    colGrid->Add(m_cpMC0);

    colGrid->Add(new wxStaticText(this, wxID_ANY, "MC1:"));
    m_cpMC1 = new wxColourPickerCtrl(this, 3004);
    colGrid->Add(m_cpMC1);

    rightSizer->Add(colGrid, 0, wxALL, 5);

    m_txtAddr = new wxStaticText(this, wxID_ANY, "Addr: $0000");
    rightSizer->Add(m_txtAddr, 0, wxTOP | wxLEFT, 10);

    contentSizer->Add(rightSizer, 0, wxEXPAND | wxALL, 5);

    mainSizer->Add(contentSizer, 1, wxEXPAND);
    SetSizer(mainSizer);

    Bind(wxEVT_TOOL, &PaneVICSprites::OnToggleLock, this, 2001);
    Bind(wxEVT_TOOL, &PaneVICSprites::OnUndo, this, 2002);
    Bind(wxEVT_TOOL, &PaneVICSprites::OnRotate90, this, 2003);
    Bind(wxEVT_TOOL, &PaneVICSprites::OnRotate180, this, 2004);
    Bind(wxEVT_TOOL, &PaneVICSprites::OnInvert, this, 2005);
    Bind(wxEVT_TOOL, &PaneVICSprites::OnLoadBlob, this, 2006);
    Bind(wxEVT_TOOL, &PaneVICSprites::OnSaveBlob, this, 2007);

    Bind(wxEVT_CHECKBOX, &PaneVICSprites::OnToggleMCM, this, 3001);
    Bind(wxEVT_COLOURPICKER_CHANGED, &PaneVICSprites::OnColorChanged, this, 3002);
    Bind(wxEVT_COLOURPICKER_CHANGED, &PaneVICSprites::OnColorChanged, this, 3003);
    Bind(wxEVT_COLOURPICKER_CHANGED, &PaneVICSprites::OnColorChanged, this, 3004);

    wxAcceleratorEntry entries[1];
    entries[0].Set(wxACCEL_CTRL, (int)'Z', 2002);
    wxAcceleratorTable accel(1, entries);
    SetAcceleratorTable(accel);

    SetSelectedSprite(0);
}

void PaneVICSprites::SetSelectedSprite(int index) {
    m_selectedSprite = index;
    for (int i = 0; i < 8; i++) {
        m_thumbnails[i]->SetSelected(i == index);
    }
    m_bigEditor->SetSpriteIndex(index);
    UpdateControls();
}

uint32_t PaneVICSprites::GetSpriteDataBase(int sn) {
    uint8_t memsetup = sim_mem_read_byte(m_sim, 0xD018);
    uint8_t cia2a    = sim_mem_read_byte(m_sim, 0xDD00);
    uint32_t vic_bank = (uint32_t)((~cia2a) & 3) * 0x4000u;
    uint32_t screen_base = vic_bank + (uint32_t)((memsetup >> 4) & 0xF) * 1024u;
    uint16_t ptr_addr  = (uint16_t)((screen_base + 0x3F8u + (uint32_t)sn) & 0xFFFF);
    uint8_t  ptr       = sim_mem_read_byte(m_sim, ptr_addr);
    return (vic_bank + (uint32_t)ptr * 64u) & 0xFFFF;
}

void PaneVICSprites::UpdateControls() {
    uint32_t base = GetSpriteDataBase(m_selectedSprite);
    m_txtAddr->SetLabel(wxString::Format("Addr: $%04X", (unsigned int)base));

    bool mcm = (sim_mem_read_byte(m_sim, 0xD01C) & (1 << m_selectedSprite)) != 0;
    m_chkMCM->SetValue(mcm);

    m_cpSprite->SetColour(GetC64Color(sim_mem_read_byte(m_sim, 0xD027 + m_selectedSprite)));
    m_cpMC0->SetColour(GetC64Color(sim_mem_read_byte(m_sim, 0xD025)));
    m_cpMC1->SetColour(GetC64Color(sim_mem_read_byte(m_sim, 0xD026)));
}

void PaneVICSprites::RefreshPane(const SimSnapshot &snap) {
    if (snap.running) {
        if (!m_locked) {
            m_locked = true;
            m_toolBar->ToggleTool(2001, true);
        }
    }
    RefreshAll();
    UpdateControls();
}

void PaneVICSprites::RefreshAll() {
    for (int i = 0; i < 8; i++) m_thumbnails[i]->Refresh();
    m_bigEditor->Refresh();
}

void PaneVICSprites::PushEdit(const SpriteEdit& edit) {
    m_undoStack.push_back(edit);
    if (m_undoStack.size() > 100) m_undoStack.erase(m_undoStack.begin());
}

void PaneVICSprites::Undo() {
    if (m_undoStack.empty()) return;
    SpriteEdit edit = m_undoStack.back();
    m_undoStack.pop_back();
    sim_mem_write_byte(m_sim, edit.addr, edit.old_val);
    RefreshAll();
}

void PaneVICSprites::OnToggleLock(wxCommandEvent& event) { m_locked = event.IsChecked(); }
void PaneVICSprites::OnUndo(wxCommandEvent& WXUNUSED(event)) { Undo(); }

void SpriteBigEditor::DoEdit(int px, int py) {
    if (px < 0 || px >= 24 || py < 0 || py >= 21) return;

    uint32_t base = m_pane->GetSpriteDataBase(m_index);
    uint16_t addr = (uint16_t)((base + (uint32_t)py * 3u + (uint32_t)(px / 8)) & 0xFFFF);
    uint8_t val = sim_mem_read_byte(m_sim, addr);
    uint8_t old_val = val;

    bool mcm = (sim_mem_read_byte(m_sim, 0xD01C) & (1 << m_index)) != 0;

    if (mcm) {
        int bit_idx = 6 - (px % 8 / 2) * 2;
        int bits = (val >> bit_idx) & 3;
        bits = (bits + 1) & 3;
        val = (val & ~(3 << bit_idx)) | (bits << bit_idx);
    } else {
        int bit_idx = 7 - (px % 8);
        val ^= (1 << bit_idx);
    }

    if (val != old_val) {
        sim_mem_write_byte(m_sim, addr, val);
        m_pane->PushEdit({m_index, addr, old_val, val});
        m_pane->RefreshAll();
    }
}

void PaneVICSprites::OnRotate90(wxCommandEvent& WXUNUSED(event)) {
    if (m_locked) return;
    uint32_t base = GetSpriteDataBase(m_selectedSprite);
    uint8_t src[24*21];
    for (int y=0; y<21; y++) {
        for (int x=0; x<24; x++) {
            uint16_t addr = (uint16_t)(base + y*3 + x/8);
            src[y*24+x] = (sim_mem_read_byte(m_sim, addr) & (1<<(7-(x%8)))) ? 1 : 0;
        }
    }
    uint8_t dst[24*21];
    memset(dst, 0, sizeof(dst));
    // Rotate 90: (x,y) -> (20-y, x) inside 21x21
    for (int y=0; y<21; y++) {
        for (int x=0; x<21; x++) {
            int nx = 20 - y;
            int ny = x;
            dst[ny*24+nx] = src[y*24+x];
        }
    }
    // Write back
    for (int y=0; y<21; y++) {
        for (int b=0; b<3; b++) {
            uint8_t val = 0;
            for (int bit=0; bit<8; bit++) {
                if (dst[y*24 + b*8 + bit]) val |= (1<<(7-bit));
            }
            sim_mem_write_byte(m_sim, (uint16_t)(base + y*3 + b), val);
        }
    }
    RefreshAll();
}

void PaneVICSprites::OnRotate180(wxCommandEvent& WXUNUSED(event)) {
    if (m_locked) return;
    uint32_t base = GetSpriteDataBase(m_selectedSprite);
    uint8_t blob[63];
    for (int i=0; i<63; i++) blob[i] = sim_mem_read_byte(m_sim, (uint16_t)(base + i));
    
    for (int i=0; i<21; i++) {
        uint8_t* row = &blob[i*3];
        uint32_t r24 = (row[0] << 16) | (row[1] << 8) | row[2];
        uint32_t fr24 = 0;
        for (int j=0; j<24; j++) if (r24 & (1<<j)) fr24 |= (1<<(23-j));
        int target_row = 20 - i;
        sim_mem_write_byte(m_sim, (uint16_t)(base + target_row*3 + 0), (fr24 >> 16) & 0xFF);
        sim_mem_write_byte(m_sim, (uint16_t)(base + target_row*3 + 1), (fr24 >> 8) & 0xFF);
        sim_mem_write_byte(m_sim, (uint16_t)(base + target_row*3 + 2), fr24 & 0xFF);
    }
    RefreshAll();
}

void PaneVICSprites::OnInvert(wxCommandEvent& WXUNUSED(event)) {
    if (m_locked) return;
    uint32_t base = GetSpriteDataBase(m_selectedSprite);
    for (int i=0; i<63; i++) {
        uint8_t val = sim_mem_read_byte(m_sim, (uint16_t)(base + i));
        sim_mem_write_byte(m_sim, (uint16_t)(base + i), ~val);
    }
    RefreshAll();
}

void PaneVICSprites::OnSaveBlob(wxCommandEvent& WXUNUSED(event)) {
    wxFileDialog saveDir(this, "Save Sprite Blob", "", 
                         wxString::Format("sprite%d.bin", m_selectedSprite),
                         "Binary files (*.bin)|*.bin", wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (saveDir.ShowModal() == wxID_CANCEL) return;
    
    uint32_t base = GetSpriteDataBase(m_selectedSprite);
    uint8_t blob[64];
    for (int i=0; i<64; i++) blob[i] = sim_mem_read_byte(m_sim, (uint16_t)(base + i));
    
    FILE* f = fopen(saveDir.GetPath().mb_str(), "wb");
    if (f) { fwrite(blob, 1, 64, f); fclose(f); }
}

void PaneVICSprites::OnLoadBlob(wxCommandEvent& WXUNUSED(event)) {
    if (m_locked) return;
    wxFileDialog openDir(this, "Load Sprite Blob", "", "",
                         "Binary files (*.bin)|*.bin", wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (openDir.ShowModal() == wxID_CANCEL) return;

    FILE* f = fopen(openDir.GetPath().mb_str(), "rb");
    if (f) {
        uint8_t blob[64];
        size_t n = fread(blob, 1, 64, f);
        fclose(f);
        uint32_t base = GetSpriteDataBase(m_selectedSprite);
        for (size_t i=0; i<n && i<64; i++) sim_mem_write_byte(m_sim, (uint16_t)(base + i), blob[i]);
        RefreshAll();
    }
}

void PaneVICSprites::OnToggleMCM(wxCommandEvent& event) {
    if (m_locked) { UpdateControls(); return; }
    uint8_t val = sim_mem_read_byte(m_sim, 0xD01C);
    if (event.IsChecked()) val |= (1 << m_selectedSprite);
    else val &= ~(1 << m_selectedSprite);
    sim_mem_write_byte(m_sim, 0xD01C, val);
    RefreshAll();
}

void PaneVICSprites::OnColorChanged(wxCommandEvent& event) {
    if (m_locked) { UpdateControls(); return; }
    wxColour col = event.GetEventObject() == m_cpSprite ? m_cpSprite->GetColour() :
                  event.GetEventObject() == m_cpMC0 ? m_cpMC0->GetColour() : m_cpMC1->GetColour();
    
    int best_ci = 0;
    int min_dist = 1000000;
    for (int i=0; i<16; i++) {
        int dr = col.Red() - vic2_palette[i][0];
        int dg = col.Green() - vic2_palette[i][1];
        int db = col.Blue() - vic2_palette[i][2];
        int d = dr*dr + dg*dg + db*db;
        if (d < min_dist) { min_dist = d; best_ci = i; }
    }
    
    if (event.GetEventObject() == m_cpSprite) sim_mem_write_byte(m_sim, 0xD027 + m_selectedSprite, best_ci);
    else if (event.GetEventObject() == m_cpMC0) sim_mem_write_byte(m_sim, 0xD025, best_ci);
    else if (event.GetEventObject() == m_cpMC1) sim_mem_write_byte(m_sim, 0xD026, best_ci);
    RefreshAll();
}
