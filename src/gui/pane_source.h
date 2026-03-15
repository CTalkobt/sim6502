#ifndef SIM_PANE_SOURCE_H
#define SIM_PANE_SOURCE_H

#include "pane_base.h"
#include <wx/stc/stc.h>

class PaneSource : public SimPane {
public:
    PaneSource(wxWindow* parent, sim_session_t *sim);
    void RefreshPane(const SimSnapshot &snap) override;
    wxString GetPaneTitle() const override { return "Source View"; }
    wxString GetPaneName() const override { return "Source"; }

    bool LoadFile(const wxString& path);

private:
    void SetupStyles();
    void OnMarginClick(wxStyledTextEvent& event);

    wxStyledTextCtrl* m_stc;
    wxString          m_loadedPath;
    int               m_currentLine;
};

#endif
