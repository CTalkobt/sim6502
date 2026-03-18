#ifndef SIM_PANE_VIC_CHAR_EDITOR_H
#define SIM_PANE_VIC_CHAR_EDITOR_H

#include "pane_base.h"
#include <wx/glcanvas.h>
#include <wx/toolbar.h>
#include <wx/stattext.h>
#include <wx/clrpicker.h>
#include <wx/checkbox.h>
#include <wx/radiobut.h>
#include <wx/textctrl.h>
#include <wx/image.h>
#include <vector>

class PaneVICChars;

class CharSelector : public wxGLCanvas {
public:
    CharSelector(PaneVICChars* parent, sim_session_t* sim);
    virtual ~CharSelector();
    void RefreshSelector() { Refresh(); }

private:
    void OnPaint(wxPaintEvent& event);
    void OnMouseLeftDown(wxMouseEvent& event);
    void OnKeyDown(wxKeyEvent& event);
    void InitGL();

    PaneVICChars* m_pane;
    sim_session_t* m_sim;
    wxGLContext* m_context;
    uint8_t* m_pixels; // RGBA 128x128 (16x16 chars, each 8x8)
    unsigned int m_texture;
    bool m_glInitialized;
};

class CharBigEditor : public wxGLCanvas {
public:
    CharBigEditor(PaneVICChars* parent, sim_session_t* sim);
    virtual ~CharBigEditor();
    void SetCharIndex(int index) { m_index = index; Refresh(); }
    void RefreshEditor() { Refresh(); }
    void PushUndo();

private:
    struct UndoEntry {
        int charIndex;
        uint8_t bytes[8];
    };

    void OnPaint(wxPaintEvent& event);
    void OnMouseLeftDown(wxMouseEvent& event);
    void OnMouseMove(wxMouseEvent& event);
    void OnMouseLeave(wxMouseEvent& event);
    void OnKeyDown(wxKeyEvent& event);
    void InitGL();
    void DoEdit(int x, int y);
    void DoUndo();

    PaneVICChars* m_pane;
    sim_session_t* m_sim;
    int m_index;
    wxGLContext* m_context;
    uint8_t* m_pixels; // RGBA 8x8
    unsigned int m_texture;
    bool m_glInitialized;
    int m_last_px, m_last_py;
    int m_last_width, m_last_height;
    int m_hover_px, m_hover_py;
    std::vector<UndoEntry> m_undoStack;
};

class CharVirtualScreen : public wxGLCanvas {
public:
    CharVirtualScreen(PaneVICChars* parent, sim_session_t* sim);
    virtual ~CharVirtualScreen();
    void RefreshScreen() { Refresh(); }

private:
    void OnPaint(wxPaintEvent& event);
    void OnMouseLeftDown(wxMouseEvent& event);
    void OnMouseMove(wxMouseEvent& event);
    void InitGL();

    PaneVICChars* m_pane;
    sim_session_t* m_sim;
    wxGLContext* m_context;
    uint8_t* m_pixels; // RGBA 320x200
    unsigned int m_texture;
    bool m_glInitialized;
    int m_last_width, m_last_height;
};

class PaneVICChars : public SimPane {
    friend class CharBigEditor;
    friend class CharSelector;
    friend class CharVirtualScreen;
public:
    PaneVICChars(wxWindow* parent, sim_session_t *sim);
    void RefreshPane(const SimSnapshot &snap) override;
    void RefreshAll();
    wxString GetPaneTitle() const override { return "VIC-II Character Editor"; }
    wxString GetPaneName() const override { return "VICChars"; }

    bool IsLocked() const { return m_locked; }
    bool ShowMCMGrid() const { return m_showMCMGrid; }
    bool HasClipboard() const { return m_hasClipboard; }
    void SetSelectedChar(int index);
    int GetSelectedChar() const { return m_selectedChar; }
    void CopyChar();
    void PasteChar();

    uint16_t GetCharSetAddr();
    uint8_t GetScratchpadChar(int x, int y) { return m_scratchpad[y * 40 + x]; }
    void SetScratchpadChar(int x, int y, uint8_t c) { m_scratchpad[y * 40 + x] = c; m_virtualScreen->RefreshScreen(); }

    // Colors
    uint8_t GetBGColor();
    uint8_t GetMC1Color();
    uint8_t GetMC2Color();
    uint8_t GetCharColor();
    bool IsMCM();

private:
    void OnToggleLock(wxCommandEvent& event);
    void OnToggleMCM(wxCommandEvent& event);
    void OnColorChanged(wxCommandEvent& event);
    void OnClearScratchpad(wxCommandEvent& event);
    void OnLoadCharSet(wxCommandEvent& event);
    void OnSaveCharSet(wxCommandEvent& event);
    void OnAddrModeChanged(wxCommandEvent& event);
    void OnCharTransform(wxCommandEvent& event);
    void ApplyTransform(int op);
    void OnToggleMCMGrid(wxCommandEvent& event);
    void OnExportASM(wxCommandEvent& event);
    void OnExportPNG(wxCommandEvent& event);
    void OnCopyChar(wxCommandEvent& event);
    void OnPasteChar(wxCommandEvent& event);

    CharSelector* m_selector;
    CharBigEditor* m_bigEditor;
    CharVirtualScreen* m_virtualScreen;
    wxToolBar* m_toolBar;
    bool m_locked;
    int m_selectedChar;

    uint8_t m_scratchpad[1000];

    wxStaticText* m_lblEditChar;

    // Right-side controls
    wxCheckBox* m_chkMCM;
    wxRadioButton* m_rbAddrAuto;
    wxRadioButton* m_rbAddrManual;
    wxTextCtrl* m_txtAddrManual;
    wxColourPickerCtrl* m_cpBG;
    wxColourPickerCtrl* m_cpMC1;
    wxColourPickerCtrl* m_cpMC2;
    wxColourPickerCtrl* m_cpChar;
    wxStaticText* m_lblAddr;
    wxStaticText* m_lblHelp;
    bool m_showMCMGrid;
    bool m_hasClipboard;
    uint8_t m_clipboard[8];
};

#endif
