#include "pane_test_runner.h"
#include <wx/toolbar.h>
#include <wx/artprov.h>
#include <wx/stattext.h>
#include <wx/msgdlg.h>

static int tr_parse_field(const char *s) {
    if (!s || !s[0] || s[0] == '-') return -1;
    return (int)strtol(s, NULL, 16) & 0xFF;
}

PaneTestRunner::PaneTestRunner(wxWindow* parent, sim_session_t *sim)
    : SimPane(parent, sim) 
{
    wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);
    
    wxToolBar* toolBar = new wxToolBar(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTB_HORIZONTAL | wxTB_FLAT);
    toolBar->AddControl(new wxStaticText(toolBar, wxID_ANY, " Routine: $"));
    m_routineAddr = new wxTextCtrl(toolBar, wxID_ANY, "0000", wxDefaultPosition, wxSize(60, -1));
    toolBar->AddControl(m_routineAddr);
    toolBar->AddControl(new wxStaticText(toolBar, wxID_ANY, " Scratch: $"));
    m_scratchAddr = new wxTextCtrl(toolBar, wxID_ANY, "0000", wxDefaultPosition, wxSize(60, -1));
    toolBar->AddControl(m_scratchAddr);
    toolBar->AddSeparator();
    toolBar->AddTool(701, "Add Row", wxArtProvider::GetBitmap(wxART_NEW), "Add a new test case to the runner");
    toolBar->AddTool(702, "Run All", wxArtProvider::GetBitmap(wxART_GO_FORWARD), "Execute all defined test cases");
    toolBar->Realize();
    mainSizer->Add(toolBar, 0, wxEXPAND);

    m_list = new wxListCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLC_REPORT | wxLC_SINGLE_SEL);
    m_list->InsertColumn(0, "Label", wxLIST_FORMAT_LEFT, 100);
    m_list->InsertColumn(1, "A in", wxLIST_FORMAT_LEFT, 40);
    m_list->InsertColumn(2, "X in", wxLIST_FORMAT_LEFT, 40);
    m_list->InsertColumn(3, "Y in", wxLIST_FORMAT_LEFT, 40);
    m_list->InsertColumn(4, "A exp", wxLIST_FORMAT_LEFT, 40);
    m_list->InsertColumn(5, "X exp", wxLIST_FORMAT_LEFT, 40);
    m_list->InsertColumn(6, "Y exp", wxLIST_FORMAT_LEFT, 40);
    m_list->InsertColumn(7, "Result", wxLIST_FORMAT_LEFT, 150);

    mainSizer->Add(m_list, 1, wxEXPAND);
    SetSizer(mainSizer);

    toolBar->Bind(wxEVT_TOOL, &PaneTestRunner::OnAddRow, this, 701);
    toolBar->Bind(wxEVT_TOOL, &PaneTestRunner::OnRunAll, this, 702);
}

void PaneTestRunner::RefreshPane(const SimSnapshot &snap) {
    (void)snap;
}

void PaneTestRunner::OnAddRow(wxCommandEvent& WXUNUSED(event)) {
    TestRow row;
    memset(&row, 0, sizeof(row));
    snprintf(row.label, sizeof(row.label), "Test %d", (int)m_rows.size() + 1);
    strcpy(row.in_a, "-"); strcpy(row.in_x, "-"); strcpy(row.in_y, "-");
    strcpy(row.ex_a, "00"); strcpy(row.ex_x, "00"); strcpy(row.ex_y, "00");
    row.result = -1;
    m_rows.push_back(row);
    
    long item = m_list->InsertItem((long)m_rows.size() - 1, row.label);
    m_list->SetItem(item, 1, row.in_a);
    m_list->SetItem(item, 2, row.in_x);
    m_list->SetItem(item, 3, row.in_y);
    m_list->SetItem(item, 4, row.ex_a);
    m_list->SetItem(item, 5, row.ex_x);
    m_list->SetItem(item, 6, row.ex_y);
}

void PaneTestRunner::OnRunAll(wxCommandEvent& WXUNUSED(event)) {
    uint16_t raddr = (uint16_t)strtol(m_routineAddr->GetValue().mb_str(), NULL, 16);
    uint16_t saddr = (uint16_t)strtol(m_scratchAddr->GetValue().mb_str(), NULL, 16);

    size_t count = m_rows.size();
    if (count == 0) return;

    sim_test_in_t*     ins = new sim_test_in_t[count]();
    sim_test_expect_t* exs = new sim_test_expect_t[count]();
    sim_test_result_t* res = new sim_test_result_t[count]();

    for (size_t i = 0; i < count; i++) {
        ins[i].a = ins[i].x = ins[i].y = ins[i].z = ins[i].b = ins[i].s = ins[i].p = -1;
        exs[i].a = exs[i].x = exs[i].y = exs[i].z = exs[i].b = exs[i].s = exs[i].p = -1;

        ins[i].a = tr_parse_field(m_rows[i].in_a);
        ins[i].x = tr_parse_field(m_rows[i].in_x);
        ins[i].y = tr_parse_field(m_rows[i].in_y);
        exs[i].a = tr_parse_field(m_rows[i].ex_a);
        exs[i].x = tr_parse_field(m_rows[i].ex_x);
        exs[i].y = tr_parse_field(m_rows[i].ex_y);
        strncpy(ins[i].label, m_rows[i].label, 47);
    }

    int totalPassed = sim_validate_routine(m_sim, raddr, saddr, 10000, ins, exs, res, (int)count);

    for (size_t i = 0; i < count; i++) {
        m_rows[i].result = res[i].passed;
        strncpy(m_rows[i].fail_msg, res[i].fail_msg, 127);
        
        m_list->SetItem((long)i, 7, res[i].passed ? "PASS" : wxString::Format("FAIL: %s", res[i].fail_msg));
        m_list->SetItemTextColour((long)i, res[i].passed ? *wxGREEN : *wxRED);
    }

    delete[] ins; delete[] exs; delete[] res;
    
    wxMessageBox(wxString::Format("%d / %zu passed", totalPassed, count), "Test Results");
}
