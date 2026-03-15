#include "pane_vic_screen.h"
#include <wx/sizer.h>

PaneVICScreen::PaneVICScreen(wxWindow* parent, sim_session_t *sim)
    : SimPane(parent, sim),
      m_canvas(nullptr),
      m_context(nullptr),
      m_pixels(nullptr),
      m_texture(0),
      m_glInitialized(false)
{
    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
    
    m_pixels = new uint8_t[384 * 272 * 3]();
    
    m_canvas = new wxGLCanvas(this, wxID_ANY, NULL, wxDefaultPosition, wxSize(384, 272));
    m_context = new wxGLContext(m_canvas);
    
    sizer->Add(m_canvas, 1, wxEXPAND);
    SetSizer(sizer);

    m_canvas->Bind(wxEVT_PAINT, &PaneVICScreen::OnPaint, this);
}

PaneVICScreen::~PaneVICScreen() {
    delete[] m_pixels;
    delete m_context;
}

void PaneVICScreen::InitGL() {
    m_canvas->SetCurrent(*m_context);
    glGenTextures(1, &m_texture);
    glBindTexture(GL_TEXTURE_2D, m_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    m_glInitialized = true;
}

void PaneVICScreen::RefreshPane(const SimSnapshot &snap) {
    (void)snap;
    if (m_pixels) {
        sim_vic_render_framebuffer(m_sim, m_pixels);
        m_canvas->Refresh();
    }
}

void PaneVICScreen::OnPaint(wxPaintEvent& WXUNUSED(event)) {
    wxPaintDC dc(m_canvas);
    m_canvas->SetCurrent(*m_context);

    if (!m_glInitialized) InitGL();

    glClear(GL_COLOR_BUFFER_BIT);
    glViewport(0, 0, m_canvas->GetSize().x, m_canvas->GetSize().y);

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, m_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 384, 272, 0, GL_RGB, GL_UNSIGNED_BYTE, m_pixels);

    glBegin(GL_QUADS);
    glTexCoord2f(0, 1); glVertex2f(-1, -1);
    glTexCoord2f(1, 1); glVertex2f(1, -1);
    glTexCoord2f(1, 0); glVertex2f(1, 1);
    glTexCoord2f(0, 0); glVertex2f(-1, 1);
    glEnd();

    m_canvas->SwapBuffers();
}
