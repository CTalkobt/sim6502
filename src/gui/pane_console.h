#ifndef SIM_PANE_CONSOLE_H
#define SIM_PANE_CONSOLE_H

#include "pane_base.h"
#include <wx/textctrl.h>
#include <vector>

class PaneConsole : public SimPane {
public:
    PaneConsole(wxWindow* parent, sim_session_t *sim);
    void RefreshPane(const SimSnapshot &snap) override;
    wxString GetPaneTitle() const override { return "Console"; }
    wxString GetPaneName() const override { return "Console"; }

    void Log(const wxString& text, const wxColour& col = *wxBLACK);

private:
    void OnSubmit(wxCommandEvent& event);
    void OnKeyDown(wxKeyEvent& event);

    wxTextCtrl*           m_output;
    wxTextCtrl*           m_input;
    std::vector<wxString> m_history;
    int                   m_history_pos;
};

#endif
