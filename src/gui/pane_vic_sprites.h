#ifndef SIM_PANE_VIC_SPRITES_H
#define SIM_PANE_VIC_SPRITES_H

#include "pane_base.h"
#include <wx/glcanvas.h>

class SpriteCanvas : public wxGLCanvas {
public:
    SpriteCanvas(wxWindow* parent, sim_session_t* sim, int index);
    virtual ~SpriteCanvas();
    void RefreshSprite();

private:
    void OnPaint(wxPaintEvent& event);
    void InitGL();

    sim_session_t* m_sim;
    int m_index;
    wxGLContext* m_context;
    uint8_t* m_pixels; // RGBA
    unsigned int m_texture;
    bool m_glInitialized;
};

class PaneVICSprites : public SimPane {
public:
    PaneVICSprites(wxWindow* parent, sim_session_t *sim);
    void RefreshPane(const SimSnapshot &snap) override;
    wxString GetPaneTitle() const override { return "VIC-II Sprites"; }
    wxString GetPaneName() const override { return "VICSprites"; }

private:
    SpriteCanvas* m_canvases[8];
};

#endif
