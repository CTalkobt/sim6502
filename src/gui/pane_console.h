#ifndef SIM_PANE_CONSOLE_H
#define SIM_PANE_CONSOLE_H

#include "pane_base.h"
#include <wx/textctrl.h>
#include <wx/srchctrl.h>
#include <wx/stattext.h>
#include <vector>
#include <string>

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
    void OnOutputKeyDown(wxKeyEvent& event);
    void OnFilterChanged(wxCommandEvent& event);

    // Applies one Tab-completion step to the current input.
    void DoTabComplete();

    // Rebuilds the output control from m_log_entries, applying the current filter.
    void RebuildOutput();

    // Updates m_hist_label to show position in history (e.g. "↑ 2/7"), or clears it.
    void UpdateHistoryLabel();

    wxTextCtrl*              m_output;
    wxSearchCtrl*            m_filter;
    wxStaticText*            m_hist_label;
    wxTextCtrl*              m_input;

    std::vector<wxString>    m_history;
    int                      m_history_pos;

    // All log entries, kept so the output can be rebuilt when the filter changes.
    struct LogEntry { wxString text; wxColour color; };
    std::vector<LogEntry>    m_log_entries;

    // Tab-completion state: populated on first Tab, cleared by OnKeyDown on any other key.
    std::vector<std::string> m_tab_matches;  // full input values for each completion candidate
    int                      m_tab_idx;      // next match to cycle to (-1 = idle)
};

#endif
