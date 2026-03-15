#include "pane_vic_screen.h"
#include <wx/sizer.h>
#include <GL/gl.h>

PaneVICScreen::PaneVICScreen(wxWindow* parent, sim_session_t *sim)
    : SimPane(parent, sim), m_glInitialized(false)
{
    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
    
    int args[] = { WX_GL_RGBA, WX_GL_DOUBLEBUFFER, WX_GL_DEPTH_SIZE, 16, 0 };
    m_canvas = new wxGLCanvas(this, wxID_ANY, args);
    m_context = new wxGLContext(m_canvas);
    
    sizer->Add(m_canvas, 1, wxEXPAND);
    SetSizer(sizer);

    m_pixels = (uint8_t*)malloc(384 * 272 * 3);
    memset(m_pixels, 0, 384 * 272 * 3);

    m_canvas->Bind(wxEVT_PAINT, &PaneVICScreen::OnPaint, this);
    m_canvas->Bind(wxEVT_SIZE, &PaneVICScreen::OnSize, this);
}

PaneVICScreen::~PaneVICScreen() {
    delete m_context;
    free(m_pixels);
    if (m_glInitialized) {
        glDeleteTextures(1, &m_texture);
    }
}

void PaneVICScreen::InitGL() {
    m_canvas->SetCurrent(*m_context);
    glGenTextures(1, &m_texture);
    glBindTexture(GL_TEXTURE_2D, m_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 384, 272, 0, GL_RGB, GL_UNSIGNED_BYTE, m_pixels);
    m_glInitialized = true;
}

void PaneVICScreen::RefreshPane(const SimSnapshot &snap) {
    (void)snap;
    if (IsShown()) {
        sim_vic_render_framebuffer(m_sim, m_pixels);
        m_canvas->Refresh();
    }
}

void PaneVICScreen::OnPaint(wxPaintEvent& WXUNUSED(event)) {
    wxPaintDC dc(m_canvas);
    m_canvas->SetCurrent(*m_context);

    if (!m_glInitialized) {
        InitGL();
    }

    glClear(GL_COLOR_BUFFER_BIT);

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, m_texture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 384, 272, GL_RGB, GL_UNSIGNED_BYTE, m_pixels);

    glBegin(GL_QUADS);
    glTexCoord2f(0, 0); glVertex2f(-1, 1);
    glTexCoord2f(1, 0); glVertex2f(1, 1);
    glTexCoord2f(1, 1); glVertex2f(1, -1);
    glTexCoord2f(0, 1); glVertex2f(-1, -1);
    glEnd();

    m_canvas->SwapBuffers();
}

void PaneVICScreen::OnSize(wxSizeEvent& event) {
    if (m_glInitialized) {
        m_canvas->SetCurrent(*m_context);
        wxSize sz = event.GetSize();
        glViewport(0, 0, sz.x, sz.y);
    }
    event.Skip();
}
