#include "pane_vic_screen.h"
#include <wx/sizer.h>
#include <wx/app.h>
#include <wx/frame.h>
#include <wx/aui/aui.h>
#include <algorithm>

enum {
    ID_VIC_ZOOM_1X         = 4100,
    ID_VIC_ZOOM_2X         = 4101,
    ID_VIC_ZOOM_3X         = 4102,
    ID_VIC_ZOOM_4X         = 4103,
    ID_VIC_ZOOM_FIT        = 4104,
    ID_VIC_ZOOM_FULLSCREEN = 4105,
    ID_VIC_DOCK            = 4106,
};

PaneVICScreen::PaneVICScreen(wxWindow* parent, sim_session_t *sim)
    : SimPane(parent, sim),
      m_canvas(nullptr),
      m_context(nullptr),
      m_toolbar(nullptr),
      m_pixels(nullptr),
      m_texture(0),
      m_glInitialized(false),
      m_zoom(VIC_ZOOM_FIT),
      m_fullscreenFrame(nullptr),
      m_pendingFullscreen(false)
{
    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);

    m_toolbar = new wxToolBar(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                              wxTB_HORIZONTAL | wxTB_FLAT | wxTB_TEXT | wxTB_NOICONS);
    m_toolbar->AddRadioTool(ID_VIC_ZOOM_1X,  "1x",  wxNullBitmap, wxNullBitmap, "1x zoom (384x272)");
    m_toolbar->AddRadioTool(ID_VIC_ZOOM_2X,  "2x",  wxNullBitmap, wxNullBitmap, "2x zoom (768x544)");
    m_toolbar->AddRadioTool(ID_VIC_ZOOM_3X,  "3x",  wxNullBitmap, wxNullBitmap, "3x zoom (1152x816)");
    m_toolbar->AddRadioTool(ID_VIC_ZOOM_4X,  "4x",  wxNullBitmap, wxNullBitmap, "4x zoom (1536x1088)");
    m_toolbar->AddRadioTool(ID_VIC_ZOOM_FIT, "Fit", wxNullBitmap, wxNullBitmap, "Scale to fit pane (aspect-correct)");
    m_toolbar->ToggleTool(ID_VIC_ZOOM_FIT, true);
    m_toolbar->AddSeparator();
    m_toolbar->AddCheckTool(ID_VIC_ZOOM_FULLSCREEN, "Full Screen", wxNullBitmap, wxNullBitmap,
                            "Undock pane and go full screen");
    m_toolbar->AddSeparator();
    m_toolbar->AddTool(ID_VIC_DOCK, "Dock", wxNullBitmap, "Dock pane back into the main window");
    m_toolbar->EnableTool(ID_VIC_DOCK, false);   // hidden until floating
    m_toolbar->Realize();
    sizer->Add(m_toolbar, 0, wxEXPAND);

    m_pixels = new uint8_t[384 * 272 * 3]();

    m_canvas = new wxGLCanvas(this, wxID_ANY, NULL, wxDefaultPosition, wxDefaultSize);
    m_context = new wxGLContext(m_canvas);
    sizer->Add(m_canvas, 1, wxEXPAND);
    SetSizer(sizer);

    m_canvas->Bind(wxEVT_PAINT, &PaneVICScreen::OnPaint,      this);
    m_canvas->Bind(wxEVT_SIZE,  &PaneVICScreen::OnSize,       this);
    m_toolbar->Bind(wxEVT_TOOL, &PaneVICScreen::OnZoom,       this, ID_VIC_ZOOM_1X, ID_VIC_ZOOM_FIT);
    m_toolbar->Bind(wxEVT_TOOL, &PaneVICScreen::OnFullScreen, this, ID_VIC_ZOOM_FULLSCREEN);
    m_toolbar->Bind(wxEVT_TOOL, [this](wxCommandEvent&) { ExitFullScreen(); DockPane(); }, ID_VIC_DOCK);
    Bind(wxEVT_SHOW, &PaneVICScreen::OnShow, this);
}

PaneVICScreen::~PaneVICScreen() {
    if (m_fullscreenFrame) {
        m_fullscreenFrame->ShowFullScreen(false);
        m_fullscreenFrame = nullptr;
    }
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
    // Keep toolbar Dock button in sync with actual float/dock state
    bool isFloating = (wxGetTopLevelParent(this) != wxTheApp->GetTopWindow());
    m_toolbar->EnableTool(ID_VIC_DOCK, isFloating);

    if (m_pixels) {
        sim_vic_render_framebuffer(m_sim, m_pixels);
        m_canvas->Refresh();
    }
}

void PaneVICScreen::ExitFullScreen() {
    if (!m_fullscreenFrame) return;
    m_fullscreenFrame->ShowFullScreen(false);
    m_fullscreenFrame = nullptr;
    m_toolbar->ToggleTool(ID_VIC_ZOOM_FULLSCREEN, false);
    UpdateDockButton();
}

void PaneVICScreen::ApplyFullScreen() {
    wxFrame* floatFrame = wxDynamicCast(wxGetTopLevelParent(this), wxFrame);
    if (floatFrame) {
        floatFrame->ShowFullScreen(true);
        m_fullscreenFrame = floatFrame;
        m_toolbar->ToggleTool(ID_VIC_ZOOM_FULLSCREEN, true);
    }
}

void PaneVICScreen::OnZoom(wxCommandEvent& event) {
    switch (event.GetId()) {
        case ID_VIC_ZOOM_1X:  m_zoom = VIC_ZOOM_1X;  break;
        case ID_VIC_ZOOM_2X:  m_zoom = VIC_ZOOM_2X;  break;
        case ID_VIC_ZOOM_3X:  m_zoom = VIC_ZOOM_3X;  break;
        case ID_VIC_ZOOM_4X:  m_zoom = VIC_ZOOM_4X;  break;
        case ID_VIC_ZOOM_FIT: m_zoom = VIC_ZOOM_FIT; break;
    }
    m_pendingFullscreen = false;
    ExitFullScreen();
    m_canvas->Refresh();
}

void PaneVICScreen::OnFullScreen(wxCommandEvent& WXUNUSED(event)) {
    if (m_fullscreenFrame) {
        ExitFullScreen();
        return;
    }

    wxFrame* mainFrame = wxDynamicCast(wxTheApp->GetTopWindow(), wxFrame);
    wxAuiManager* mgr = mainFrame ? wxAuiManager::GetManager(mainFrame) : nullptr;

    if (mgr) {
        wxAuiPaneInfo& info = mgr->GetPane(this);
        if (info.IsOk() && info.IsDocked()) {
            m_pendingFullscreen = true;
            info.Float().FloatingSize(384 * 2, 272 * 2 + 40);
            mgr->Update();
            return;
        }
    }

    ApplyFullScreen();
}

void PaneVICScreen::OnShow(wxShowEvent& event) {
    if (!event.IsShown()) {
        m_pendingFullscreen = false;
        ExitFullScreen();
        UpdateDockButton();
    } else if (m_pendingFullscreen) {
        m_pendingFullscreen = false;
        ApplyFullScreen();
    } else {
        bool isFloating = (wxGetTopLevelParent(this) != wxTheApp->GetTopWindow());
        m_toolbar->EnableTool(ID_VIC_DOCK, isFloating);
        UpdateDockButton();
    }
    event.Skip();
}

void PaneVICScreen::ComputeQuad(int cw, int ch,
                                float& left, float& right,
                                float& top,  float& bottom)
{
    float iw, ih;
    if (m_zoom == VIC_ZOOM_FIT) {
        float scale = std::min((float)cw / 384.0f, (float)ch / 272.0f);
        if (scale <= 0.0f) scale = 1.0f;
        iw = 384.0f * scale;
        ih = 272.0f * scale;
    } else {
        iw = 384.0f * (float)m_zoom;
        ih = 272.0f * (float)m_zoom;
    }

    float ox = ((float)cw - iw) * 0.5f;
    float oy = ((float)ch - ih) * 0.5f;
    left   =  2.0f * ox / (float)cw - 1.0f;
    right  =  2.0f * (ox + iw) / (float)cw - 1.0f;
    top    =  1.0f - 2.0f * oy / (float)ch;
    bottom =  1.0f - 2.0f * (oy + ih) / (float)ch;
}

void PaneVICScreen::OnPaint(wxPaintEvent& WXUNUSED(event)) {
    wxPaintDC dc(m_canvas);
    m_canvas->SetCurrent(*m_context);

    if (!m_glInitialized) InitGL();

    wxSize sz = m_canvas->GetClientSize();
    if (sz.x <= 0 || sz.y <= 0) {
        m_canvas->SwapBuffers();
        return;
    }

    double dpr = m_canvas->GetContentScaleFactor();
    if (dpr <= 0.0) dpr = 1.0;
    glViewport(0, 0, (int)(sz.x * dpr), (int)(sz.y * dpr));

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, m_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 384, 272, 0, GL_RGB, GL_UNSIGNED_BYTE, m_pixels);

    float left, right, top, bottom;
    ComputeQuad(sz.x, sz.y, left, right, top, bottom);

    glBegin(GL_QUADS);
    glTexCoord2f(0, 0); glVertex2f(left,  top);
    glTexCoord2f(1, 0); glVertex2f(right, top);
    glTexCoord2f(1, 1); glVertex2f(right, bottom);
    glTexCoord2f(0, 1); glVertex2f(left,  bottom);
    glEnd();

    m_canvas->SwapBuffers();
}

void PaneVICScreen::OnSize(wxSizeEvent& event) {
    m_canvas->Refresh();
    event.Skip();
}
