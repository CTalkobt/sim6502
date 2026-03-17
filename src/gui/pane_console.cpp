#include "pane_console.h"

PaneConsole::PaneConsole(wxWindow* parent, sim_session_t *sim)
    : SimPane(parent, sim) 
{
    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
    
    m_output = new wxTextCtrl(this, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE | wxTE_READONLY | wxTE_RICH2);
    m_input = new wxTextCtrl(this, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER);

    sizer->Add(m_output, 1, wxEXPAND);
    sizer->Add(m_input, 0, wxEXPAND);
    SetSizer(sizer);

    m_input->Bind(wxEVT_TEXT_ENTER, &PaneConsole::OnSubmit, this);
    m_input->Bind(wxEVT_KEY_DOWN, &PaneConsole::OnKeyDown, this);
    
    m_history_pos = -1;

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
    m_output->SetDefaultStyle(wxTextAttr(col));
    m_output->AppendText(text);
    m_output->SetDefaultStyle(wxTextAttr(m_output->GetForegroundColour()));
}

void PaneConsole::OnSubmit(wxCommandEvent& WXUNUSED(event)) {
    wxString cmd = m_input->GetValue();
    if (cmd.IsEmpty()) return;

    Log("> " + cmd + "\n", wxColour(0, 128, 0));
    m_history.push_back(cmd);
    m_history_pos = -1;
    m_input->Clear();

    if (cmd == "cls") {
        m_output->Clear();
    } else if (cmd == "quit" || cmd == "exit") {
        Log("Use File > Quit to exit.\n", *wxRED);
    } else {
        sim_exec_command(m_sim, cmd.ToUTF8());
    }
}

void PaneConsole::OnKeyDown(wxKeyEvent& event) {
    if (event.GetKeyCode() == WXK_UP) {
        if (!m_history.empty()) {
            if (m_history_pos == -1) m_history_pos = m_history.size() - 1;
            else if (m_history_pos > 0) m_history_pos--;
            m_input->SetValue(m_history[m_history_pos]);
            m_input->SetInsertionPointEnd();
        }
    } else if (event.GetKeyCode() == WXK_DOWN) {
        if (m_history_pos != -1) {
            if (m_history_pos < (int)m_history.size() - 1) {
                m_history_pos++;
                m_input->SetValue(m_history[m_history_pos]);
            } else {
                m_history_pos = -1;
                m_input->Clear();
            }
            m_input->SetInsertionPointEnd();
        }
    } else {
        event.Skip();
    }
}
