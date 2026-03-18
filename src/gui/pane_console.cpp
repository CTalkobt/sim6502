#include "pane_console.h"
#include <wx/utils.h>
#include "sim_api.h"

PaneConsole::PaneConsole(wxWindow* parent, sim_session_t *sim)
    : SimPane(parent, sim)
{
    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);

    m_output = new wxTextCtrl(this, wxID_ANY, "", wxDefaultPosition, wxDefaultSize,
                              wxTE_MULTILINE | wxTE_READONLY | wxTE_RICH2);

    m_filter = new wxSearchCtrl(this, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER);
    m_filter->SetDescriptiveText("Filter output...");
    m_filter->ShowSearchButton(true);
    m_filter->ShowCancelButton(true);

    // Input row: history label on the left, input field fills the rest
    wxBoxSizer* inputRow = new wxBoxSizer(wxHORIZONTAL);
    m_hist_label = new wxStaticText(this, wxID_ANY, "");
    m_input = new wxTextCtrl(this, wxID_ANY, "", wxDefaultPosition, wxDefaultSize,
                             wxTE_PROCESS_ENTER);
    m_input->SetHint("\x60 to focus  \u2502  \u2191\u2193 history  \u2502  Tab complete");
    inputRow->Add(m_hist_label, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 4);
    inputRow->Add(m_input,      1, wxEXPAND);

    sizer->Add(m_output,   1, wxEXPAND);
    sizer->Add(m_filter,   0, wxEXPAND);
    sizer->Add(inputRow,   0, wxEXPAND);
    SetSizer(sizer);

    m_input->Bind(wxEVT_TEXT_ENTER, &PaneConsole::OnSubmit, this);
    m_input->Bind(wxEVT_KEY_DOWN,   &PaneConsole::OnKeyDown, this);
    m_output->Bind(wxEVT_KEY_DOWN,  &PaneConsole::OnOutputKeyDown, this);

    m_filter->Bind(wxEVT_TEXT,        &PaneConsole::OnFilterChanged, this);
    m_filter->Bind(wxEVT_TEXT_ENTER,  &PaneConsole::OnFilterChanged, this);
    m_filter->Bind(wxEVT_SEARCHCTRL_CANCEL_BTN, &PaneConsole::OnFilterChanged, this);

    m_history_pos = -1;
    m_tab_idx     = -1;

    // Set up logging from sim core back to this pane
    sim_set_log_callback(m_sim, [](const char *text, void *userdata) {
        ((PaneConsole*)userdata)->Log(text);
    }, this);

    Log("6502 Simulator Console Ready.\n", *wxBLUE);
}

void PaneConsole::RefreshPane(const SimSnapshot &snap) {
    (void)snap;
}

void PaneConsole::Log(const wxString& text, const wxColour& col) {
    m_log_entries.push_back({text, col});

    // Only append to the live output when the entry passes the current filter.
    wxString filterText = m_filter->GetValue();
    if (filterText.IsEmpty() || text.Lower().Contains(filterText.Lower())) {
        m_output->SetDefaultStyle(wxTextAttr(col));
        m_output->AppendText(text);
        m_output->SetDefaultStyle(wxTextAttr(m_output->GetForegroundColour()));
    }
}

void PaneConsole::RebuildOutput() {
    m_output->Clear();
    wxString filterText = m_filter->GetValue().Lower();
    for (const auto& e : m_log_entries) {
        if (filterText.IsEmpty() || e.text.Lower().Contains(filterText)) {
            m_output->SetDefaultStyle(wxTextAttr(e.color));
            m_output->AppendText(e.text);
        }
    }
    m_output->SetDefaultStyle(wxTextAttr(m_output->GetForegroundColour()));
}

void PaneConsole::OnFilterChanged(wxCommandEvent& WXUNUSED(event)) {
    RebuildOutput();
}

void PaneConsole::UpdateHistoryLabel() {
    if (m_history_pos == -1 || m_history.empty()) {
        m_hist_label->SetLabel("");
    } else {
        int pos  = (int)m_history.size() - m_history_pos;   // 1-based from end
        int total = (int)m_history.size();
        m_hist_label->SetLabelText(wxString::Format("\u2191 %d/%d", pos, total));
    }
    Layout();
}

void PaneConsole::OnSubmit(wxCommandEvent& WXUNUSED(event)) {
    wxString cmd = m_input->GetValue();
    if (cmd.IsEmpty()) return;

    Log("> " + cmd + "\n", wxColour(0, 128, 0));
    m_history.push_back(cmd);
    m_history_pos = -1;
    UpdateHistoryLabel();
    m_input->Clear();

    if (cmd == "cls") {
        m_output->Clear();
        m_log_entries.clear();
    } else if (cmd == "quit" || cmd == "exit") {
        Log("Use File > Quit to exit.\n", *wxRED);
    } else {
        sim_exec_command(m_sim, cmd.ToUTF8());
    }
}

void PaneConsole::OnOutputKeyDown(wxKeyEvent& event) {
    const int key = event.GetKeyCode();
    if (event.ControlDown()) {
        if (key == 'A') {
            m_output->SetSelection(-1, -1);
            return;
        }
        if (key == 'C') {
            m_output->Copy();
            return;
        }
    }
    event.Skip();
}

void PaneConsole::DoTabComplete() {
    // OnKeyDown clears m_tab_idx / m_tab_matches on every non-Tab key, so
    // when we arrive here the state is either fresh (idx == -1) or mid-cycle.

    if (m_tab_idx >= 0) {
        // Subsequent Tab: cycle through the existing match list.
        m_input->SetValue(wxString(m_tab_matches[m_tab_idx].c_str(), wxConvUTF8));
        m_input->SetInsertionPointEnd();
        m_tab_idx = (m_tab_idx + 1) % (int)m_tab_matches.size();
        return;
    }

    // First Tab press for this editing session: gather completions.
    wxString current = m_input->GetValue();

    // Decide what to complete.
    // No space yet → completing the command name (first word).
    // Space present → completing a symbol name (argument).
    const bool completing_arg   = current.Contains(' ');
    const wxString fixedPart    = completing_arg ? current.BeforeLast(' ') + " " : "";
    const wxString completionStem = completing_arg ? current.AfterLast(' ') : current;

    std::vector<std::string> raw;
    if (completing_arg)
        raw = sim_get_symbol_completions(m_sim, completionStem.ToUTF8().data());
    else
        raw = sim_get_completions(completionStem.ToUTF8().data());

    if (raw.empty()) {
        wxBell();
        return;
    }

    // Build m_tab_matches as full input values (fixedPart + completion word).
    m_tab_matches.clear();
    for (const auto& r : raw)
        m_tab_matches.push_back((fixedPart + wxString(r.c_str(), wxConvUTF8)).ToStdString());

    if (m_tab_matches.size() == 1) {
        // Unambiguous: complete immediately and append a space.
        m_input->SetValue(wxString(m_tab_matches[0].c_str(), wxConvUTF8) + " ");
        m_input->SetInsertionPointEnd();
        m_tab_matches.clear();
        return;
    }

    // Multiple matches: extend the input to the longest common prefix of the raw names.
    std::string common = raw[0];
    for (size_t i = 1; i < raw.size(); i++) {
        size_t j = 0;
        while (j < common.size() && j < raw[i].size() && common[j] == raw[i][j]) j++;
        common = common.substr(0, j);
    }
    wxString fullCommon = fixedPart + wxString(common.c_str(), wxConvUTF8);
    if (fullCommon.length() > current.length()) {
        m_input->SetValue(fullCommon);
        m_input->SetInsertionPointEnd();
    }

    // Print the raw names (without fixedPart) so the list is readable.
    wxString matchLine;
    for (const auto& r : raw)
        matchLine += wxString(r.c_str(), wxConvUTF8) + "  ";
    Log("\n" + matchLine + "\n", wxColour(100, 180, 255));

    // Prime cycling so the next Tab advances through options.
    m_tab_idx = 0;
}

void PaneConsole::OnKeyDown(wxKeyEvent& event) {
    const int key = event.GetKeyCode();

    if (key == WXK_TAB) {
        DoTabComplete();
        // Do not Skip() — suppress the default focus-change behaviour.
        return;
    }

    // Any key other than Tab resets completion cycling.
    if (key != WXK_UP && key != WXK_DOWN) {
        m_tab_matches.clear();
        m_tab_idx = -1;
    }

    if (key == WXK_UP) {
        if (!m_history.empty()) {
            if (m_history_pos == -1) m_history_pos = m_history.size() - 1;
            else if (m_history_pos > 0) m_history_pos--;
            m_input->SetValue(m_history[m_history_pos]);
            m_input->SetInsertionPointEnd();
            UpdateHistoryLabel();
        }
    } else if (key == WXK_DOWN) {
        if (m_history_pos != -1) {
            if (m_history_pos < (int)m_history.size() - 1) {
                m_history_pos++;
                m_input->SetValue(m_history[m_history_pos]);
            } else {
                m_history_pos = -1;
                m_input->Clear();
            }
            m_input->SetInsertionPointEnd();
            UpdateHistoryLabel();
        }
    } else {
        event.Skip();
    }
}
