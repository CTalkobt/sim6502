#ifndef SIM_PANE_VIC_SPRITES_H
#define SIM_PANE_VIC_SPRITES_H

#include "pane_base.h"
#include <wx/glcanvas.h>
#include <wx/toolbar.h>
#include <wx/stattext.h>
#include <wx/clrpicker.h>
#include <vector>

struct SpriteEdit {
    int sprite_index;
    uint16_t addr;
    uint8_t old_val;
    uint8_t new_val;
};

class PaneVICSprites;

class SpriteThumbnail : public wxGLCanvas {
public:
    SpriteThumbnail(PaneVICSprites* parent, sim_session_t* sim, int index);
    virtual ~SpriteThumbnail();
    void RefreshSprite();
    void SetSelected(bool sel) { m_selected = sel; Refresh(); }

private:
    void OnPaint(wxPaintEvent& event);
    void OnMouseLeftDown(wxMouseEvent& event);
    void InitGL();

    PaneVICSprites* m_pane;
    sim_session_t* m_sim;
    int m_index;
    wxGLContext* m_context;
    uint8_t* m_pixels; // RGBA
    unsigned int m_texture;
    bool m_glInitialized;
    bool m_selected;
};

class SpriteBigEditor : public wxGLCanvas {
public:
    SpriteBigEditor(PaneVICSprites* parent, sim_session_t* sim);
    virtual ~SpriteBigEditor();
    void SetSpriteIndex(int index) { m_index = index; Refresh(); }
    void RefreshEditor() { Refresh(); }

private:
    void OnPaint(wxPaintEvent& event);
    void OnMouseLeftDown(wxMouseEvent& event);
    void OnMouseMove(wxMouseEvent& event);
    void InitGL();
    void DoEdit(int x, int y);

    PaneVICSprites* m_pane;
    sim_session_t* m_sim;
    int m_index;
    wxGLContext* m_context;
    uint8_t* m_pixels; // RGBA
    unsigned int m_texture;
    bool m_glInitialized;
    int m_last_px, m_last_py;
    int m_last_width, m_last_height;
};

class PaneVICSprites : public SimPane {
    friend class SpriteBigEditor;
public:
    PaneVICSprites(wxWindow* parent, sim_session_t *sim);
    void RefreshPane(const SimSnapshot &snap) override;
    void RefreshAll();
    wxString GetPaneTitle() const override { return "VIC-II Sprites"; }
    wxString GetPaneName() const override { return "VICSprites"; }

    bool IsLocked() const { return m_locked; }
    void PushEdit(const SpriteEdit& edit);
    void Undo();
    void SetSelectedSprite(int index);
    int GetSelectedSprite() const { return m_selectedSprite; }

private:
    void OnToggleLock(wxCommandEvent& event);
    void OnUndo(wxCommandEvent& event);
    void OnRotate90(wxCommandEvent& event);
    void OnRotate180(wxCommandEvent& event);
    void OnInvert(wxCommandEvent& event);
    void OnSaveBlob(wxCommandEvent& event);
    void OnLoadBlob(wxCommandEvent& event);
    void OnToggleMCM(wxCommandEvent& event);
    void OnColorChanged(wxCommandEvent& event);

    void UpdateControls();
    uint32_t GetSpriteDataBase(int sn);

    SpriteThumbnail* m_thumbnails[8];
    SpriteBigEditor* m_bigEditor;
    wxToolBar* m_toolBar;
    bool m_locked;
    int m_selectedSprite;
    std::vector<SpriteEdit> m_undoStack;

    // Right-side controls
    wxCheckBox* m_chkMCM;
    wxColourPickerCtrl* m_cpSprite;
    wxColourPickerCtrl* m_cpMC0;
    wxColourPickerCtrl* m_cpMC1;
    wxStaticText* m_txtAddr;
};

#endif
