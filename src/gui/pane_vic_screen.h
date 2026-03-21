#ifndef SIM_PANE_VIC_SCREEN_H
#define SIM_PANE_VIC_SCREEN_H

#include "pane_base.h"
#include <wx/glcanvas.h>
#include <wx/toolbar.h>

enum VICScreenZoom {
    VIC_ZOOM_1X  = 1,
    VIC_ZOOM_2X  = 2,
    VIC_ZOOM_3X  = 3,
    VIC_ZOOM_4X  = 4,
    VIC_ZOOM_FIT = 5,
};

class PaneVICScreen : public SimPane {
public:
    PaneVICScreen(wxWindow* parent, sim_session_t *sim);
    virtual ~PaneVICScreen();
    void RefreshPane(const SimSnapshot &snap) override;
    wxString GetPaneTitle() const override { return "VIC-II Screen"; }
    wxString GetPaneName() const override { return "VICScreen"; }

private:
    void OnPaint(wxPaintEvent& event);
    void OnSize(wxSizeEvent& event);
    void OnZoom(wxCommandEvent& event);
    void OnFullScreen(wxCommandEvent& event);
    void OnShow(wxShowEvent& event);
    void InitGL();
    void ExitFullScreen();
    void ApplyFullScreen();
    void ComputeQuad(int cw, int ch, float& left, float& right, float& top, float& bottom);

    wxGLCanvas*  m_canvas;
    wxGLContext* m_context;
    wxToolBar*   m_toolbar;
    uint8_t*     m_pixels;
    unsigned int m_texture;
    bool         m_glInitialized;
    int          m_zoom;
    wxFrame*     m_fullscreenFrame;   // non-null while the pane's floating frame is fullscreen
    bool         m_pendingFullscreen; // set when we've floated the pane and are waiting for OnShow
};

#endif
