#include "pane_vic_sprites.h"
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <GL/gl.h>

SpriteCanvas::SpriteCanvas(wxWindow* parent, sim_session_t* sim, int index)
    : wxGLCanvas(parent, wxID_ANY, NULL, wxDefaultPosition, wxSize(48, 42)), // 2x scaled for visibility
      m_sim(sim), m_index(index), m_glInitialized(false)
{
    m_context = new wxGLContext(this);
    m_pixels = (uint8_t*)malloc(24 * 21 * 4);
    memset(m_pixels, 0, 24 * 21 * 4);

    Bind(wxEVT_PAINT, &SpriteCanvas::OnPaint, this);
}

SpriteCanvas::~SpriteCanvas() {
    delete m_context;
    free(m_pixels);
    if (m_glInitialized) {
        glDeleteTextures(1, &m_texture);
    }
}

void SpriteCanvas::InitGL() {
    SetCurrent(*m_context);
    glGenTextures(1, &m_texture);
    glBindTexture(GL_TEXTURE_2D, m_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 24, 21, 0, GL_RGBA, GL_UNSIGNED_BYTE, m_pixels);
    m_glInitialized = true;
}

void SpriteCanvas::RefreshSprite() {
    sim_vic_render_sprite(m_sim, m_index, m_pixels);
    Refresh();
}

void SpriteCanvas::OnPaint(wxPaintEvent& WXUNUSED(event)) {
    wxPaintDC dc(this);
    SetCurrent(*m_context);

    if (!m_glInitialized) {
        InitGL();
    }

    glClearColor(0.2f, 0.2f, 0.2f, 1.0f); // Dark grey background for transparency
    glClear(GL_COLOR_BUFFER_BIT);

    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    glBindTexture(GL_TEXTURE_2D, m_texture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 24, 21, GL_RGBA, GL_UNSIGNED_BYTE, m_pixels);

    glBegin(GL_QUADS);
    glTexCoord2f(0, 0); glVertex2f(-1, 1);
    glTexCoord2f(1, 0); glVertex2f(1, 1);
    glTexCoord2f(1, 1); glVertex2f(1, -1);
    glTexCoord2f(0, 1); glVertex2f(-1, -1);
    glEnd();

    SwapBuffers();
}

PaneVICSprites::PaneVICSprites(wxWindow* parent, sim_session_t *sim)
    : SimPane(parent, sim) 
{
    wxFlexGridSizer* sizer = new wxFlexGridSizer(2, 4, 10, 10);
    
    for (int i = 0; i < 8; i++) {
        wxBoxSizer* colSizer = new wxBoxSizer(wxVERTICAL);
        colSizer->Add(new wxStaticText(this, wxID_ANY, wxString::Format("Sprite %d", i)), 0, wxALIGN_CENTER);
        m_canvases[i] = new SpriteCanvas(this, sim, i);
        colSizer->Add(m_canvases[i], 0, wxALIGN_CENTER);
        sizer->Add(colSizer, 0, wxALL, 5);
    }

    SetSizer(sizer);
}

void PaneVICSprites::RefreshPane(const SimSnapshot &snap) {
    (void)snap;
    if (IsShown()) {
        for (int i = 0; i < 8; i++) {
            m_canvases[i]->RefreshSprite();
        }
    }
}
