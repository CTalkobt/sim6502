#ifndef SIM_PANE_SOURCE_H
#define SIM_PANE_SOURCE_H

#include "pane_base.h"
#include <wx/stc/stc.h>
#include <wx/toolbar.h>
#include <wx/textctrl.h>
#include <wx/panel.h>
#include <wx/stattext.h>

class PaneSource : public SimPane {
public:
    PaneSource(wxWindow* parent, sim_session_t *sim);
    void RefreshPane(const SimSnapshot &snap) override;
    wxString GetPaneTitle() const override { return "Source View"; }
    wxString GetPaneName() const override { return "Source"; }

    bool LoadFile(const wxString& path);

private:
    void SetupStyles();
    void ShowFindBar(bool show);
    void DoFind(bool forward);

    void OnMarginClick(wxStyledTextEvent& event);
    void OnReload(wxCommandEvent& event);
    void OnKeyDown(wxKeyEvent& event);
    void OnFindKeyDown(wxKeyEvent& event);
    void OnFindNext(wxCommandEvent& event);
    void OnFindPrev(wxCommandEvent& event);
    void OnFindClose(wxCommandEvent& event);
    void OnContextMenu(wxContextMenuEvent& event);
    void OnNavDisasm(wxCommandEvent& event);
    void OnNavMemory(wxCommandEvent& event);

    wxStyledTextCtrl* m_stc;
    wxStaticText*     m_titleLabel;
    wxPanel*          m_findPanel;
    wxTextCtrl*       m_findCtrl;
    wxString          m_loadedPath;
    int               m_currentLine;
    int               m_contextMenuLine;
};

#endif
