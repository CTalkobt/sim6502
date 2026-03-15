#include "dialogs.h"
#include <wx/stattext.h>
#include <wx/sizer.h>
#include <wx/checkbox.h>
#include <wx/filepicker.h>
#include <wx/propgrid/propgrid.h>
#include <unistd.h>

// --- Load Binary Dialog ---
LoadBinaryDialog::LoadBinaryDialog(wxWindow* parent, const wxString& path)
    : wxDialog(parent, wxID_ANY, "Load Binary", wxDefaultPosition, wxDefaultSize)
{
    m_isPRG = path.Lower().EndsWith(".prg");
    uint16_t headerAddr = 0;
    if (m_isPRG) {
        FILE *f = fopen(path.mb_str(), "rb");
        if (f) {
            int lo = fgetc(f), hi = fgetc(f);
            fclose(f);
            if (lo != EOF && hi != EOF)
                headerAddr = (uint16_t)((unsigned)lo | ((unsigned)hi << 8));
        }
    }

    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
    sizer->Add(new wxStaticText(this, wxID_ANY, "File: " + path), 0, wxALL, 10);

    wxBoxSizer* addrSizer = new wxBoxSizer(wxHORIZONTAL);
    addrSizer->Add(new wxStaticText(this, wxID_ANY, "Load Address (hex): $"), 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 10);
    m_addrCtrl = new wxTextCtrl(this, wxID_ANY, m_isPRG ? wxString::Format("%04X", headerAddr) : "0200");
    addrSizer->Add(m_addrCtrl, 1, wxEXPAND | wxALL, 5);
    sizer->Add(addrSizer, 0, wxEXPAND);

    if (m_isPRG) {
        m_overridePRG = new wxCheckBox(this, wxID_ANY, "Override PRG header address");
        sizer->Add(m_overridePRG, 0, wxALL, 10);
    } else {
        m_overridePRG = NULL;
    }

    sizer->Add(CreateButtonSizer(wxOK | wxCANCEL), 0, wxALIGN_CENTER | wxALL, 10);
    SetSizerAndFit(sizer);
}

uint16_t LoadBinaryDialog::GetAddress() const {
    unsigned long addr;
    if (m_addrCtrl->GetValue().ToULong(&addr, 16)) return (uint16_t)addr;
    return 0;
}

bool LoadBinaryDialog::ShouldOverride() const {
    return m_overridePRG ? m_overridePRG->IsChecked() : true;
}

// --- Save Binary Dialog ---
SaveBinaryDialog::SaveBinaryDialog(wxWindow* parent)
    : wxDialog(parent, wxID_ANY, "Save Binary", wxDefaultPosition, wxDefaultSize)
{
    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);

    wxFlexGridSizer* grid = new wxFlexGridSizer(2, 2, 5, 5);
    grid->Add(new wxStaticText(this, wxID_ANY, "Start Address ($):"), 0, wxALIGN_CENTER_VERTICAL);
    m_startCtrl = new wxTextCtrl(this, wxID_ANY, "0200");
    grid->Add(m_startCtrl, 1, wxEXPAND);

    grid->Add(new wxStaticText(this, wxID_ANY, "Byte Count ($):"), 0, wxALIGN_CENTER_VERTICAL);
    m_countCtrl = new wxTextCtrl(this, wxID_ANY, "0100");
    grid->Add(m_countCtrl, 1, wxEXPAND);

    sizer->Add(grid, 1, wxEXPAND | wxALL, 10);
    sizer->Add(CreateButtonSizer(wxOK | wxCANCEL), 0, wxALIGN_CENTER | wxALL, 10);
    SetSizerAndFit(sizer);
}

uint16_t SaveBinaryDialog::GetStart() const {
    unsigned long val;
    if (m_startCtrl->GetValue().ToULong(&val, 16)) return (uint16_t)val;
    return 0;
}

uint16_t SaveBinaryDialog::GetCount() const {
    unsigned long val;
    if (m_countCtrl->GetValue().ToULong(&val, 16)) return (uint16_t)val;
    return 0;
}

// --- New Project Wizard ---
NewProjectWizard::NewProjectWizard(wxWindow* parent)
    : wxWizard(parent, wxID_ANY, "New Project Wizard")
{
    std::vector<ProjectTemplate> templates = ProjectManager::list_templates();
    
    TemplatePage* page1 = new TemplatePage(this, templates);
    ConfigPage* page2 = new ConfigPage(this, page1);
    
    wxWizardPageSimple::Chain(page1, page2);
    
    GetPageAreaSizer()->Add(page1);
    
    m_result.success = false;
    
    Bind(wxEVT_WIZARD_FINISHED, &NewProjectWizard::OnFinished, this);
}

void NewProjectWizard::OnFinished(wxWizardEvent& WXUNUSED(event)) {
    ConfigPage* configPage = (ConfigPage*)GetCurrentPage();
    TemplatePage* templatePage = (TemplatePage*)configPage->GetPrev();
    
    ProjectTemplate* tmpl = templatePage->GetSelectedTemplate();
    if (tmpl) {
        m_result.success = true;
        m_result.name = configPage->GetProjectName();
        m_result.path = configPage->GetProjectPath();
        m_result.templateId = tmpl->id;
        m_result.vars = configPage->GetVars();
    }
}

TemplatePage::TemplatePage(wxWizard* parent, std::vector<ProjectTemplate>& templates)
    : wxWizardPageSimple(parent), m_templates(templates)
{
    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
    sizer->Add(new wxStaticText(this, wxID_ANY, "Step 1: Select a Project Template"), 0, wxALL, 10);
    
    m_list = new wxListBox(this, wxID_ANY);
    for (const auto& t : m_templates) {
        m_list->Append(t.name);
    }
    if (!m_templates.empty()) m_list->SetSelection(0);
    
    sizer->Add(m_list, 1, wxEXPAND | wxALL, 10);
    SetSizer(sizer);
}

ProjectTemplate* TemplatePage::GetSelectedTemplate() const {
    int sel = m_list->GetSelection();
    if (sel != wxNOT_FOUND) return &m_templates[sel];
    return NULL;
}

ConfigPage::ConfigPage(wxWizard* parent, TemplatePage* prevPage)
    : wxWizardPageSimple(parent, NULL, NULL), m_prevPage(prevPage)
{
    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
    sizer->Add(new wxStaticText(this, wxID_ANY, "Step 2: Configure Project Details"), 0, wxALL, 10);
    
    wxFlexGridSizer* grid = new wxFlexGridSizer(2, 2, 5, 5);
    grid->Add(new wxStaticText(this, wxID_ANY, "Project Name:"), 0, wxALIGN_CENTER_VERTICAL);
    m_nameCtrl = new wxTextCtrl(this, wxID_ANY, "Untitled");
    grid->Add(m_nameCtrl, 1, wxEXPAND);
    
    grid->Add(new wxStaticText(this, wxID_ANY, "Target Directory:"), 0, wxALIGN_CENTER_VERTICAL);
    m_pathCtrl = new wxTextCtrl(this, wxID_ANY, "");
    grid->Add(m_pathCtrl, 1, wxEXPAND);
    
    sizer->Add(grid, 0, wxEXPAND | wxALL, 10);
    
    sizer->Add(new wxStaticText(this, wxID_ANY, "Template Variables:"), 0, wxLEFT | wxRIGHT, 10);
    m_varsGrid = new wxPropertyGrid(this, wxID_ANY, wxDefaultPosition, wxSize(-1, 150));
    sizer->Add(m_varsGrid, 1, wxEXPAND | wxALL, 10);
    
    SetSizer(sizer);
    
    m_nameCtrl->Bind(wxEVT_TEXT, &ConfigPage::OnNameChanged, this);
    Bind(wxEVT_WIZARD_PAGE_CHANGING, &ConfigPage::OnPageChanging, this);
}

void ConfigPage::OnPageChanging(wxWizardEvent& event) {
    if (event.GetDirection()) { // Forward
        ProjectTemplate* tmpl = m_prevPage->GetSelectedTemplate();
        if (tmpl) {
            m_varsGrid->Clear();
            for (auto const& [key, val] : tmpl->variables) {
                if (key == "PROJECT_NAME") continue;
                m_varsGrid->Append(new wxStringProperty(key, key, val));
            }
            
            if (m_pathCtrl->GetValue().IsEmpty()) {
                char cwd[512];
                if (getcwd(cwd, sizeof(cwd))) {
                    m_pathCtrl->SetValue(wxString::Format("%s/%s", cwd, m_nameCtrl->GetValue()));
                }
            }
        }
    }
}

void ConfigPage::OnNameChanged(wxCommandEvent& WXUNUSED(event)) {
    char cwd[512];
    if (getcwd(cwd, sizeof(cwd))) {
        m_pathCtrl->SetValue(wxString::Format("%s/%s", cwd, m_nameCtrl->GetValue()));
    }
}

std::map<std::string, std::string> ConfigPage::GetVars() const {
    std::map<std::string, std::string> vars;
    wxPropertyGridIterator it = m_varsGrid->GetIterator();
    for ( ; !it.AtEnd(); it++ ) {
        wxPGProperty* p = *it;
        vars[p->GetName().ToStdString()] = p->GetValueAsString().ToStdString();
    }
    return vars;
}
