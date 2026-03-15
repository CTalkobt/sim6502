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

    Log("6502 Simulator Console Ready.", *wxBLUE);
}

void PaneConsole::RefreshPane(const SimSnapshot &snap) {
    (void)snap;
}

void PaneConsole::Log(const wxString& text, const wxColour& col) {
    m_output->SetDefaultStyle(wxTextAttr(col));
    m_output->AppendText(text);
    if (!text.EndsWith("\n")) m_output->AppendText("\n");
    m_output->SetDefaultStyle(wxTextAttr(m_output->GetForegroundColour()));
}

void PaneConsole::OnSubmit(wxCommandEvent& WXUNUSED(event)) {
    wxString cmd = m_input->GetValue();
    if (cmd.IsEmpty()) return;

    Log("> " + cmd, wxColour(0, 128, 0));
    m_history.push_back(cmd);
    m_history_pos = -1;
    m_input->Clear();

    if (cmd == "help") {
        Log("Available commands: step, reset, cls");
    } else if (cmd == "step") {
        sim_step(m_sim, 1);
        Log("Stepped 1 instruction.");
    } else if (cmd == "reset") {
        sim_reset(m_sim);
        Log("Simulator reset.");
    } else if (cmd == "cls") {
        m_output->Clear();
    } else {
        Log("Unknown command: " + cmd, *wxRED);
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
