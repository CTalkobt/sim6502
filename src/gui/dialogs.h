#ifndef SIM_DIALOGS_H
#define SIM_DIALOGS_H

#include <wx/wx.h>
#include <wx/wizard.h>
#include <wx/listbox.h>
#include <wx/listctrl.h>
#include <wx/propgrid/propgrid.h>
#include <wx/spinctrl.h>
#include <wx/treebook.h>
#include <wx/filepicker.h>
#include <vector>
#include "sim_api.h"
#include "project_manager.h"

// --- ROM entry (address-based layout, machine-agnostic) ---
struct RomEntry {
    wxString addr;  // e.g. "$A000" or "$20000"
    wxString type;  // "Kernal" | "Basic" | "Character" | "Expansion" | "Other"
    wxString file;  // absolute path to .rom or .bin
};

// Small dialog for adding/editing a single ROM entry
class RomEntryDialog : public wxDialog {
public:
    RomEntryDialog(wxWindow* parent, const RomEntry& entry = RomEntry());
    RomEntry GetEntry() const;
private:
    wxTextCtrl*       m_addrCtrl;
    wxComboBox*       m_typeCombo;
    wxFilePickerCtrl* m_filePicker;
};

// --- Settings Dialog ---
class SettingsDialog : public wxDialog {
public:
    // theme: 0=Dark, 1=Light, 2=Auto  (matches MainFrame::m_theme)
    SettingsDialog(wxWindow* parent, int fontSize, int theme, float uiScale,
                   float speedScale, const wxString& machine, const wxString& processor);

    int      GetFontSize()  const;
    int      GetTheme()     const;
    float    GetUIScale()   const;
    wxString GetVICEBin()   const;
    wxString GetVICEData()  const;
    wxString GetXemuBin()   const;
    wxString GetXemuData()  const;

    // ROM Mapping page — indices map to: 0=raw6502, 1=c64, 2=c128, 3=mega65, 4=x16
    static constexpr int kRomTargetCount = 5;
    const std::vector<RomEntry>& GetROMEntries(int targetIdx) const;

    // Speed page
    float    GetSpeedScale()       const;
    wxString GetDefaultMachine()   const;
    wxString GetDefaultProcessor() const;

    // Limits page
    int           GetHistDepth()       const;
    int           GetTraceDepth()      const;
    int           GetMaxBreakpoints()  const;
    int           GetMaxWatches()      const;
    int           GetMaxSnapDiff()     const;
    unsigned long GetCycleLimit()      const; // 0 = unlimited

private:
    void RefreshRomList();

    // General page
    wxSpinCtrl*       m_fontSizeCtrl;
    wxRadioButton*    m_themeAuto;
    wxRadioButton*    m_themeDark;
    wxRadioButton*    m_themeLight;
    wxComboBox*       m_scaleCombo;
    // Emulators page
    wxDirPickerCtrl*  m_viceBinPicker;
    wxDirPickerCtrl*  m_viceDataPicker;
    wxDirPickerCtrl*  m_xemuBinPicker;
    wxDirPickerCtrl*  m_xemuDataPicker;
    // ROM Mapping page
    std::vector<RomEntry> m_romEntries[5];
    wxListCtrl*           m_romList;
    wxComboBox*           m_romTargetCombo;
    int                   m_romCurrentTarget;
    // Speed page
    wxComboBox*       m_speedCombo;
    wxComboBox*       m_machineCombo;
    wxComboBox*       m_processorCombo;
    // Limits page
    wxSpinCtrl*       m_histDepthCtrl;
    wxSpinCtrl*       m_traceDepthCtrl;
    wxSpinCtrl*       m_maxBpCtrl;
    wxSpinCtrl*       m_maxWatchCtrl;
    wxSpinCtrl*       m_maxSnapCtrl;
    wxTextCtrl*       m_cycLimitCtrl;
    wxCheckBox*       m_cycUnlimitedCheck;
};

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
