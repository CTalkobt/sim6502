#ifndef SIM_DIALOGS_H
#define SIM_DIALOGS_H

#include <wx/wx.h>
#include <wx/wizard.h>
#include <wx/listbox.h>
#include <wx/propgrid/propgrid.h>
#include "sim_api.h"
#include "project_manager.h"

// --- Load Binary Dialog ---
class LoadBinaryDialog : public wxDialog {
public:
    LoadBinaryDialog(wxWindow* parent, const wxString& path);
    uint16_t GetAddress() const;
    bool ShouldOverride() const;

private:
    wxTextCtrl* m_addrCtrl;
    wxCheckBox* m_overridePRG;
    bool m_isPRG;
};

// --- Save Binary Dialog ---
class SaveBinaryDialog : public wxDialog {
public:
    SaveBinaryDialog(wxWindow* parent);
    uint16_t GetStart() const;
    uint16_t GetCount() const;

private:
    wxTextCtrl* m_startCtrl;
    wxTextCtrl* m_countCtrl;
};

// --- New Project Wizard ---
class NewProjectWizard : public wxWizard {
public:
    NewProjectWizard(wxWindow* parent);
    
    struct Result {
        bool success;
        wxString name;
        wxString path;
        wxString templateId;
        std::map<std::string, std::string> vars;
    };
    
    Result GetResult() const { return m_result; }

private:
    void OnFinished(wxWizardEvent& event);

    Result m_result;
};

class TemplatePage : public wxWizardPageSimple {
public:
    TemplatePage(wxWizard* parent, std::vector<ProjectTemplate>& templates);
    ProjectTemplate* GetSelectedTemplate() const;

private:
    wxListBox* m_list;
    std::vector<ProjectTemplate>& m_templates;
};

class ConfigPage : public wxWizardPageSimple {
public:
    ConfigPage(wxWizard* parent, TemplatePage* prevPage);
    wxString GetProjectName() const { return m_nameCtrl->GetValue(); }
    wxString GetProjectPath() const { return m_pathCtrl->GetValue(); }
    std::map<std::string, std::string> GetVars() const;

private:
    void OnPageChanging(wxWizardEvent& event);
    void OnNameChanged(wxCommandEvent& event);

    wxTextCtrl* m_nameCtrl;
    wxTextCtrl* m_pathCtrl;
    wxPropertyGrid* m_varsGrid;
    TemplatePage* m_prevPage;
};

#endif
