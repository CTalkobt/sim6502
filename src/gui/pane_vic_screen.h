#ifndef SIM_PANE_VIC_SCREEN_H
#define SIM_PANE_VIC_SCREEN_H

#include "pane_base.h"
#include <wx/glcanvas.h>

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
    void InitGL();

    wxGLCanvas* m_canvas;
    wxGLContext* m_context;
    uint8_t* m_pixels;
    unsigned int m_texture;
    bool m_glInitialized;
};

#endif
