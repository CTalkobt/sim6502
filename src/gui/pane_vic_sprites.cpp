#include "pane_vic_sprites.h"
#include <wx/sizer.h>

SpriteCanvas::SpriteCanvas(wxWindow* parent, sim_session_t* sim, int index)
    : wxGLCanvas(parent, wxID_ANY, NULL, wxDefaultPosition, wxSize(64, 64)),
      m_sim(sim), m_index(index), m_texture(0), m_glInitialized(false)
{
    m_context = new wxGLContext(this);
    m_pixels = new uint8_t[24 * 21 * 4](); // RGBA
    Bind(wxEVT_PAINT, &SpriteCanvas::OnPaint, this);
}

SpriteCanvas::~SpriteCanvas() {
    delete[] m_pixels;
    delete m_context;
}

void SpriteCanvas::InitGL() {
    SetCurrent(*m_context);
    glGenTextures(1, &m_texture);
    glBindTexture(GL_TEXTURE_2D, m_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    m_glInitialized = true;
}

void SpriteCanvas::RefreshSprite() {
    Refresh();
}

void SpriteCanvas::OnPaint(wxPaintEvent& WXUNUSED(event)) {
    wxPaintDC dc(this);
    SetCurrent(*m_context);

    if (!m_glInitialized) InitGL();

    sim_vic_render_sprite(m_sim, m_index, m_pixels);

    glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glViewport(0, 0, GetSize().x, GetSize().y);

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

PaneVICSprites::PaneVICSprites(wxWindow* parent, sim_session_t *sim)
    : SimPane(parent, sim) 
{
    wxFlexGridSizer* sizer = new wxFlexGridSizer(4, 2, 5, 5);
    for (int i = 0; i < 8; i++) {
        m_canvases[i] = new SpriteCanvas(this, m_sim, i);
        sizer->Add(m_canvases[i], 1, wxEXPAND);
    }
    SetSizer(sizer);
}

void PaneVICSprites::RefreshPane(const SimSnapshot &snap) {
    (void)snap;
    for (int i = 0; i < 8; i++) {
        m_canvases[i]->RefreshSprite();
    }
}
