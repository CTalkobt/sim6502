#include "dialogs.h"
#include "debug_types.h"
#include "breakpoints.h"
#include <wx/stattext.h>
#include <wx/sizer.h>
#include <wx/checkbox.h>
#include <wx/filepicker.h>
#include <wx/propgrid/propgrid.h>
#include <wx/spinctrl.h>
#include <wx/treebook.h>
#include <wx/config.h>
#include <unistd.h>

// --- Emulator path auto-detection ---

// Returns the first directory from the list that contains the named executable.
static wxString FindDirWithExe(const wxString dirs[], size_t n, const wxString& exe)
{
    for (size_t i = 0; i < n; i++) {
        if (wxFileExists(dirs[i] + wxFILE_SEP_PATH + exe))
            return dirs[i];
    }
    return "";
}

// Returns the first directory from the list that contains any of the named executables.
static wxString FindDirWithAnyExe(const wxString dirs[], size_t nd,
                                   const wxString exes[], size_t ne)
{
    for (size_t i = 0; i < nd; i++) {
        for (size_t j = 0; j < ne; j++) {
            if (wxFileExists(dirs[i] + wxFILE_SEP_PATH + exes[j]))
                return dirs[i];
        }
    }
    return "";
}

static void DetectEmulatorPaths(wxString& viceBin, wxString& viceData,
                                wxString& xemuBin, wxString& xemuData)
{
    auto dirExists = [](const wxString& p) { return wxDirExists(p); };

#if defined(__WXMSW__)
    // --- VICE (Windows) — user-local first, then system ---
    {
        const wxString dirs[] = {
            wxString(wxGetenv("LOCALAPPDATA")) + "\\VICE",
            "C:\\Program Files\\VICE",
            "C:\\Program Files (x86)\\VICE",
        };
        viceBin = FindDirWithExe(dirs, WXSIZEOF(dirs), "x64sc.exe");
    }
    {
        const wxString dirs[] = {
            wxString(wxGetenv("LOCALAPPDATA")) + "\\VICE",
            "C:\\Program Files\\VICE",
            "C:\\Program Files (x86)\\VICE",
        };
        for (auto& d : dirs) { if (dirExists(d)) { viceData = d; break; } }
    }

    // --- Xemu (Windows) ---
    {
        const wxString dirs[] = {
            wxString(wxGetenv("LOCALAPPDATA")) + "\\xemu",
            "C:\\Program Files\\xemu",
            "C:\\Program Files (x86)\\xemu",
        };
        const wxString exes[] = { "xemu-xmega65.exe", "xemu-xc65.exe", "xmega65.exe" };
        xemuBin = FindDirWithAnyExe(dirs, WXSIZEOF(dirs), exes, WXSIZEOF(exes));
    }
    {
        const wxString dirs[] = {
            wxString(wxGetenv("APPDATA")) + "\\xemu",
            wxString(wxGetenv("LOCALAPPDATA")) + "\\xemu",
            "C:\\Program Files\\xemu",
        };
        for (auto& d : dirs) { if (dirExists(d)) { xemuData = d; break; } }
    }

#elif defined(__WXOSX__)
    // --- VICE (macOS) — ~/Applications and Homebrew before /Applications ---
    {
        const wxString dirs[] = {
            wxGetHomeDir() + "/Applications/VICE.app/Contents/MacOS",
            "/opt/homebrew/bin",
            "/usr/local/bin",
            "/Applications/VICE.app/Contents/MacOS",
        };
        viceBin = FindDirWithExe(dirs, WXSIZEOF(dirs), "x64sc");
    }
    {
        const wxString dirs[] = {
            wxGetHomeDir() + "/Applications/VICE.app/Contents/Resources",
            "/opt/homebrew/share/vice",
            "/usr/local/share/vice",
            "/Applications/VICE.app/Contents/Resources",
        };
        for (auto& d : dirs) { if (dirExists(d)) { viceData = d; break; } }
    }

    // --- Xemu (macOS) ---
    {
        const wxString dirs[] = {
            wxGetHomeDir() + "/Applications/Xemu.app/Contents/MacOS",
            "/opt/homebrew/bin",
            "/usr/local/bin",
            "/Applications/Xemu.app/Contents/MacOS",
        };
        const wxString exes[] = { "xemu-xmega65", "xemu-xc65", "xemu-xplus4", "xmega65" };
        xemuBin = FindDirWithAnyExe(dirs, WXSIZEOF(dirs), exes, WXSIZEOF(exes));
    }
    {
        const wxString dirs[] = {
            wxGetHomeDir() + "/Library/Application Support/xemu",
            "/opt/homebrew/share/xemu",
            "/usr/local/share/xemu",
        };
        for (auto& d : dirs) { if (dirExists(d)) { xemuData = d; break; } }
    }

#else
    // --- VICE (Linux) — user-local first, then system ---
    {
        const wxString dirs[] = {
            wxGetHomeDir() + "/bin",
            wxGetHomeDir() + "/.local/bin",
            "/snap/bin",
            "/usr/local/bin",
            "/usr/games",
            "/usr/bin",
        };
        viceBin = FindDirWithExe(dirs, WXSIZEOF(dirs), "x64sc");
    }
    {
        const wxString dirs[] = {
            wxGetHomeDir() + "/.local/share/vice",
            "/usr/local/share/vice",
            "/usr/share/vice",
            "/usr/lib/vice",
        };
        for (auto& d : dirs) { if (dirExists(d)) { viceData = d; break; } }
    }

    // --- Xemu (Linux) — look for xemu-* binaries, user-local first ---
    {
        const wxString dirs[] = {
            wxGetHomeDir() + "/bin",
            wxGetHomeDir() + "/.local/bin",
            "/usr/local/bin",
            "/opt/xemu",
            "/usr/bin",
        };
        const wxString exes[] = {
            "xemu-xmega65", "xemu-xc65", "xemu-xplus4", "xemu-xdos", "xmega65"
        };
        xemuBin = FindDirWithAnyExe(dirs, WXSIZEOF(dirs), exes, WXSIZEOF(exes));
    }
    {
        const wxString dirs[] = {
            wxGetHomeDir() + "/.xemu-binaries",
            wxGetHomeDir() + "/.local/share/xemu",
            "/usr/local/share/xemu",
            "/usr/share/xemu",
        };
        for (auto& d : dirs) { if (dirExists(d)) { xemuData = d; break; } }
    }
#endif
}

// --- Settings Dialog ---
SettingsDialog::SettingsDialog(wxWindow* parent, int fontSize, int theme, float uiScale,
                               float speedScale, const wxString& machine,
                               const wxString& processor)
    : wxDialog(parent, wxID_ANY, "Settings", wxDefaultPosition, wxSize(750, 480))
{
    wxBoxSizer* outerSizer = new wxBoxSizer(wxVERTICAL);

    wxTreebook* book = new wxTreebook(this, wxID_ANY);

    // --- General page ---
    wxPanel* genPage = new wxPanel(book);
    wxBoxSizer* genSizer = new wxBoxSizer(wxVERTICAL);

    // Theme
    wxStaticBoxSizer* themeSizer = new wxStaticBoxSizer(wxVERTICAL, genPage, "Theme");
    m_themeAuto  = new wxRadioButton(genPage, wxID_ANY, "Auto (follow OS)",
                                     wxDefaultPosition, wxDefaultSize, wxRB_GROUP);
    m_themeDark  = new wxRadioButton(genPage, wxID_ANY, "Dark");
    m_themeLight = new wxRadioButton(genPage, wxID_ANY, "Light");
    themeSizer->Add(m_themeAuto,  0, wxALL, 6);
    themeSizer->Add(m_themeDark,  0, wxALL, 6);
    themeSizer->Add(m_themeLight, 0, wxALL, 6);
    genSizer->Add(themeSizer, 0, wxEXPAND | wxALL, 12);

    // Font Size
    wxStaticBoxSizer* fontSizer = new wxStaticBoxSizer(wxHORIZONTAL, genPage, "Font Size");
    fontSizer->Add(new wxStaticText(genPage, wxID_ANY, "Points:"),
                   0, wxALIGN_CENTER_VERTICAL | wxALL, 8);
    m_fontSizeCtrl = new wxSpinCtrl(genPage, wxID_ANY, wxEmptyString,
                                    wxDefaultPosition, wxSize(70, -1),
                                    wxSP_ARROW_KEYS, 8, 32, fontSize);
    fontSizer->Add(m_fontSizeCtrl, 0, wxALL, 8);
    genSizer->Add(fontSizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 12);

    // Interface Scaling
    wxStaticBoxSizer* scaleSizer = new wxStaticBoxSizer(wxHORIZONTAL, genPage, "Interface Scaling");
    scaleSizer->Add(new wxStaticText(genPage, wxID_ANY, "Scale:"),
                    0, wxALIGN_CENTER_VERTICAL | wxALL, 8);
    wxArrayString scaleChoices;
    scaleChoices.Add("0.50x"); scaleChoices.Add("0.75x"); scaleChoices.Add("1.00x");
    scaleChoices.Add("1.25x"); scaleChoices.Add("1.50x"); scaleChoices.Add("1.75x");
    scaleChoices.Add("2.00x");
    m_scaleCombo = new wxComboBox(genPage, wxID_ANY, "", wxDefaultPosition,
                                  wxSize(90, -1), scaleChoices, wxCB_READONLY);
    scaleSizer->Add(m_scaleCombo, 0, wxALL, 8);
    genSizer->Add(scaleSizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 12);

    genPage->SetSizer(genSizer);
    book->AddPage(genPage, "General");

    // --- Emulators page ---
    wxPanel* emuPage = new wxPanel(book);
    wxBoxSizer* emuSizer = new wxBoxSizer(wxVERTICAL);

    // Read saved paths from config
    wxString viceBin, viceData, xemuBin, xemuData;
    {
        wxConfigBase* cfg = wxConfigBase::Get();
        if (cfg) {
            cfg->Read("Emulators/VICEBinDir",  &viceBin,  "");
            cfg->Read("Emulators/VICEData",    &viceData, "");
            cfg->Read("Emulators/XemuBinDir",  &xemuBin,  "");
            cfg->Read("Emulators/XemuData",    &xemuData, "");
        }
    }

    // VICE group
    wxStaticBoxSizer* viceSizer = new wxStaticBoxSizer(wxVERTICAL, emuPage, "VICE");
    wxFlexGridSizer* viceGrid = new wxFlexGridSizer(2, 2, 8, 8);
    viceGrid->AddGrowableCol(1);
    viceGrid->Add(new wxStaticText(emuPage, wxID_ANY, "Binary Dir:"), 0, wxALIGN_CENTER_VERTICAL);
    m_viceBinPicker  = new wxDirPickerCtrl(emuPage, wxID_ANY, viceBin,
                           "Select VICE binaries directory",
                           wxDefaultPosition, wxDefaultSize, wxDIRP_USE_TEXTCTRL);
    viceGrid->Add(m_viceBinPicker, 1, wxEXPAND);
    viceGrid->Add(new wxStaticText(emuPage, wxID_ANY, "Data Dir:"), 0, wxALIGN_CENTER_VERTICAL);
    m_viceDataPicker = new wxDirPickerCtrl(emuPage, wxID_ANY, viceData,
                           "Select VICE data directory",
                           wxDefaultPosition, wxDefaultSize, wxDIRP_USE_TEXTCTRL);
    viceGrid->Add(m_viceDataPicker, 1, wxEXPAND);
    viceSizer->Add(viceGrid, 0, wxEXPAND | wxALL, 8);
    emuSizer->Add(viceSizer, 0, wxEXPAND | wxALL, 12);

    // Xemu group
    wxStaticBoxSizer* xemuSizer = new wxStaticBoxSizer(wxVERTICAL, emuPage, "Xemu");
    wxFlexGridSizer* xemuGrid = new wxFlexGridSizer(2, 2, 8, 8);
    xemuGrid->AddGrowableCol(1);
    xemuGrid->Add(new wxStaticText(emuPage, wxID_ANY, "Binary Dir:"), 0, wxALIGN_CENTER_VERTICAL);
    m_xemuBinPicker  = new wxDirPickerCtrl(emuPage, wxID_ANY, xemuBin,
                           "Select Xemu binaries directory",
                           wxDefaultPosition, wxDefaultSize, wxDIRP_USE_TEXTCTRL);
    xemuGrid->Add(m_xemuBinPicker, 1, wxEXPAND);
    xemuGrid->Add(new wxStaticText(emuPage, wxID_ANY, "Data Dir:"), 0, wxALIGN_CENTER_VERTICAL);
    m_xemuDataPicker = new wxDirPickerCtrl(emuPage, wxID_ANY, xemuData,
                           "Select Xemu data directory",
                           wxDefaultPosition, wxDefaultSize, wxDIRP_USE_TEXTCTRL);
    xemuGrid->Add(m_xemuDataPicker, 1, wxEXPAND);
    xemuSizer->Add(xemuGrid, 0, wxEXPAND | wxALL, 8);
    emuSizer->Add(xemuSizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 12);

    // Detect button
    wxButton* detectBtn = new wxButton(emuPage, wxID_ANY, "Detect Paths");
    emuSizer->Add(detectBtn, 0, wxALIGN_RIGHT | wxLEFT | wxRIGHT | wxBOTTOM, 12);
    detectBtn->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
        wxString vb = m_viceBinPicker->GetPath(),  vd = m_viceDataPicker->GetPath();
        wxString xb = m_xemuBinPicker->GetPath(),  xd = m_xemuDataPicker->GetPath();
        DetectEmulatorPaths(vb, vd, xb, xd);
        m_viceBinPicker->SetPath(vb);
        m_viceDataPicker->SetPath(vd);
        m_xemuBinPicker->SetPath(xb);
        m_xemuDataPicker->SetPath(xd);
        if (vb.IsEmpty() && vd.IsEmpty() && xb.IsEmpty() && xd.IsEmpty())
            wxMessageBox("No VICE or Xemu installations were found.", "Detection Results",
                         wxOK | wxICON_INFORMATION, this);
    });

    emuPage->SetSizer(emuSizer);
    book->AddPage(emuPage, "Emulators");

    // --- ROM Mapping page ---
    static const char* const kRomTargetIds[]    = { "raw6502", "c64", "c128", "mega65", "x16" };
    static const char* const kRomTargetLabels[] = { "Raw 6502", "C64", "C128", "MEGA65", "Commander X16" };

    // Load saved ROM entries from config
    m_romCurrentTarget = 1; // default to C64
    {
        wxConfigBase* cfg = wxConfigBase::Get();
        for (int t = 0; t < kRomTargetCount; t++) {
            wxString base = wxString::Format("ROMs/%s/", kRomTargetIds[t]);
            int count = 0;
            if (cfg) cfg->Read(base + "Count", &count, 0);
            for (int i = 0; i < count; i++) {
                wxString eb = wxString::Format("%s%d/", base, i);
                RomEntry e;
                if (cfg) {
                    cfg->Read(eb + "Addr", &e.addr, "");
                    cfg->Read(eb + "Type", &e.type, "Other");
                    cfg->Read(eb + "File", &e.file, "");
                }
                if (!e.file.IsEmpty())
                    m_romEntries[t].push_back(e);
            }
        }
    }

    wxPanel* romPage = new wxPanel(book);
    wxBoxSizer* romSizer = new wxBoxSizer(wxVERTICAL);

    // Target selector + action buttons
    wxBoxSizer* romTopRow = new wxBoxSizer(wxHORIZONTAL);
    romTopRow->Add(new wxStaticText(romPage, wxID_ANY, "Machine:"),
                   0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 6);
    wxArrayString romTargetNames;
    for (const char* lbl : kRomTargetLabels) romTargetNames.Add(lbl);
    m_romTargetCombo = new wxComboBox(romPage, wxID_ANY, kRomTargetLabels[m_romCurrentTarget],
                                      wxDefaultPosition, wxSize(160, -1),
                                      romTargetNames, wxCB_READONLY);
    romTopRow->Add(m_romTargetCombo, 0, wxRIGHT, 16);

    wxButton* romAddBtn    = new wxButton(romPage, wxID_ANY, "Add",    wxDefaultPosition, wxSize(65, -1));
    wxButton* romEditBtn   = new wxButton(romPage, wxID_ANY, "Edit",   wxDefaultPosition, wxSize(65, -1));
    wxButton* romRemoveBtn = new wxButton(romPage, wxID_ANY, "Remove", wxDefaultPosition, wxSize(70, -1));
    wxButton* romScanBtn   = new wxButton(romPage, wxID_ANY, "Scan",   wxDefaultPosition, wxSize(65, -1));
    romTopRow->Add(romAddBtn,    0, wxRIGHT, 4);
    romTopRow->Add(romEditBtn,   0, wxRIGHT, 4);
    romTopRow->Add(romRemoveBtn, 0, wxRIGHT, 12);
    romTopRow->Add(romScanBtn,   0);
    romSizer->Add(romTopRow, 0, wxEXPAND | wxALL, 10);

    // Entry list: Address | Type | File
    m_romList = new wxListCtrl(romPage, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                wxLC_REPORT | wxLC_SINGLE_SEL | wxBORDER_SUNKEN);
    m_romList->InsertColumn(0, "Address", wxLIST_FORMAT_LEFT,  90);
    m_romList->InsertColumn(1, "Type",    wxLIST_FORMAT_LEFT, 110);
    m_romList->InsertColumn(2, "File",    wxLIST_FORMAT_LEFT, 470);
    romSizer->Add(m_romList, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);

    romPage->SetSizer(romSizer);
    book->AddPage(romPage, "ROM Mapping");

    RefreshRomList();

    // Target switch
    m_romTargetCombo->Bind(wxEVT_COMBOBOX, [this](wxCommandEvent&) {
        m_romCurrentTarget = m_romTargetCombo->GetSelection();
        RefreshRomList();
    });

    // Add
    romAddBtn->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
        RomEntryDialog dlg(this);
        if (dlg.ShowModal() == wxID_OK) {
            RomEntry e = dlg.GetEntry();
            if (!e.file.IsEmpty()) {
                m_romEntries[m_romCurrentTarget].push_back(e);
                RefreshRomList();
            }
        }
    });

    // Edit
    romEditBtn->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
        long sel = m_romList->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
        if (sel < 0 || sel >= (long)m_romEntries[m_romCurrentTarget].size()) return;
        RomEntryDialog dlg(this, m_romEntries[m_romCurrentTarget][(size_t)sel]);
        if (dlg.ShowModal() == wxID_OK)  {
            m_romEntries[m_romCurrentTarget][(size_t)sel] = dlg.GetEntry();
            RefreshRomList();
        }
    });

    // Double-click to edit
    m_romList->Bind(wxEVT_LIST_ITEM_ACTIVATED, [this](wxListEvent& ev) {
        long sel = ev.GetIndex();
        if (sel < 0 || sel >= (long)m_romEntries[m_romCurrentTarget].size()) return;
        RomEntryDialog dlg(this, m_romEntries[m_romCurrentTarget][(size_t)sel]);
        if (dlg.ShowModal() == wxID_OK) {
            m_romEntries[m_romCurrentTarget][(size_t)sel] = dlg.GetEntry();
            RefreshRomList();
        }
    });

    // Remove
    romRemoveBtn->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
        long sel = m_romList->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
        if (sel < 0 || sel >= (long)m_romEntries[m_romCurrentTarget].size()) return;
        m_romEntries[m_romCurrentTarget].erase(
            m_romEntries[m_romCurrentTarget].begin() + sel);
        RefreshRomList();
    });

    // Scan
    romScanBtn->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
        wxConfigBase* cfg = wxConfigBase::Get();
        wxString viceData, xemuData;
        if (cfg) {
            cfg->Read("Emulators/VICEData", &viceData, "");
            cfg->Read("Emulators/XemuData", &xemuData, "");
        }

        auto tryAdd = [this](int t, const wxString& addr,
                             const wxString& type, const wxString& path) {
            if (!wxFileExists(path)) return;
            for (const auto& e : m_romEntries[t])
                if (e.file == path) return;
            m_romEntries[t].push_back({addr, type, path});
        };
        auto findFirst = [](const wxString& dir, const wxString& glob) -> wxString {
            return wxFindFirstFile(dir + wxFILE_SEP_PATH + glob, wxFILE);
        };

        if (!viceData.IsEmpty()) {
            wxString sep = wxFILE_SEP_PATH;
            wxString c64  = viceData + sep + "C64"  + sep;
            wxString c128 = viceData + sep + "C128" + sep;
            // C64
            wxString f;
            f = wxFileExists(c64 + "kernal")  ? c64 + "kernal"  : findFirst(c64, "kernal-*.bin");
            tryAdd(1, "$E000", "Kernal",    f);
            f = wxFileExists(c64 + "basic")   ? c64 + "basic"   : findFirst(c64, "basic-*.bin");
            tryAdd(1, "$A000", "Basic",     f);
            f = wxFileExists(c64 + "chargen") ? c64 + "chargen" : findFirst(c64, "chargen-*.bin");
            tryAdd(1, "$D000", "Character", f);
            // C128
            f = wxFileExists(c128 + "kernal64") ? c128 + "kernal64" : findFirst(c128, "kernal-*.bin");
            tryAdd(2, "$E000", "Kernal",    f);
            f = wxFileExists(c128 + "basiclo")  ? c128 + "basiclo"  : findFirst(c128, "basiclo-*.bin");
            tryAdd(2, "$4000", "Basic",     f);
            f = wxFileExists(c128 + "chargen")  ? c128 + "chargen"  : findFirst(c128, "chargen-*.bin");
            tryAdd(2, "$D000", "Character", f);
        }

        if (!xemuData.IsEmpty()) {
            wxString sep = wxFILE_SEP_PATH;
            for (const char* name : { "MEGA65.ROM", "mega65.rom", "mega65.rom.working" }) {
                wxString path = xemuData + sep + name;
                if (wxFileExists(path)) { tryAdd(3, "$20000", "Kernal", path); break; }
            }
        }

        RefreshRomList();
        wxMessageBox("Scan complete. New ROM entries were added where files were found.",
                     "ROM Scan", wxOK | wxICON_INFORMATION, this);
    });

    // --- Speed page ---
    wxPanel* speedPage = new wxPanel(book);
    wxBoxSizer* speedSizer = new wxBoxSizer(wxVERTICAL);

    // Execution speed
    wxStaticBoxSizer* speedBox = new wxStaticBoxSizer(wxHORIZONTAL, speedPage, "Execution Speed");
    speedBox->Add(new wxStaticText(speedPage, wxID_ANY, "Speed:"),
                  0, wxALIGN_CENTER_VERTICAL | wxALL, 8);
    wxArrayString speedChoices;
    speedChoices.Add("Unlimited");
    speedChoices.Add("0.5x  (492 KHz)");
    speedChoices.Add("1x    (985 KHz PAL)");
    speedChoices.Add("2x    (1.97 MHz)");
    speedChoices.Add("4x    (3.94 MHz)");
    speedChoices.Add("8x    (7.88 MHz)");
    speedChoices.Add("16x   (15.8 MHz)");
    m_speedCombo = new wxComboBox(speedPage, wxID_ANY, "", wxDefaultPosition,
                                  wxSize(180, -1), speedChoices, wxCB_READONLY);
    speedBox->Add(m_speedCombo, 0, wxALL, 8);
    speedBox->Add(new wxStaticText(speedPage, wxID_ANY, "(applies immediately)"),
                  0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
    speedSizer->Add(speedBox, 0, wxEXPAND | wxALL, 12);

    // Default machine
    wxStaticBoxSizer* machBox = new wxStaticBoxSizer(wxHORIZONTAL, speedPage, "Default Machine");
    machBox->Add(new wxStaticText(speedPage, wxID_ANY, "Machine:"),
                 0, wxALIGN_CENTER_VERTICAL | wxALL, 8);
    wxArrayString machChoices;
    machChoices.Add("raw6502"); machChoices.Add("c64"); machChoices.Add("c128");
    machChoices.Add("mega65");  machChoices.Add("x16");
    m_machineCombo = new wxComboBox(speedPage, wxID_ANY, "", wxDefaultPosition,
                                    wxSize(140, -1), machChoices, wxCB_READONLY);
    machBox->Add(m_machineCombo, 0, wxALL, 8);
    speedSizer->Add(machBox, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 12);

    // Default processor
    wxStaticBoxSizer* procBox = new wxStaticBoxSizer(wxHORIZONTAL, speedPage, "Default Processor");
    procBox->Add(new wxStaticText(speedPage, wxID_ANY, "Processor:"),
                 0, wxALIGN_CENTER_VERTICAL | wxALL, 8);
    wxArrayString procChoices;
    procChoices.Add("6502"); procChoices.Add("6502-undoc");
    procChoices.Add("65c02"); procChoices.Add("65ce02"); procChoices.Add("45gs02");
    m_processorCombo = new wxComboBox(speedPage, wxID_ANY, "", wxDefaultPosition,
                                       wxSize(140, -1), procChoices, wxCB_READONLY);
    procBox->Add(m_processorCombo, 0, wxALL, 8);
    speedSizer->Add(procBox, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 12);

    speedPage->SetSizer(speedSizer);
    book->AddPage(speedPage, "Speed");

    // --- Limits page ---
    // Load persisted values, falling back to compile-time defaults
    int cfgHistDepth    = SIM_HIST_DEFAULT_DEPTH;
    int cfgTraceDepth   = SIM_TRACE_DEPTH;
    int cfgMaxBp        = MAX_BREAKPOINTS;
    int cfgMaxWatch     = 16;
    int cfgMaxSnap      = 4096;
    long cfgCycleLimit  = 0; // 0 = unlimited
    {
        wxConfigBase* cfg = wxConfigBase::Get();
        if (cfg) {
            cfg->Read("Limits/HistDepth",      &cfgHistDepth,   cfgHistDepth);
            cfg->Read("Limits/TraceDepth",     &cfgTraceDepth,  cfgTraceDepth);
            cfg->Read("Limits/MaxBreakpoints", &cfgMaxBp,       cfgMaxBp);
            cfg->Read("Limits/MaxWatches",     &cfgMaxWatch,    cfgMaxWatch);
            cfg->Read("Limits/MaxSnapDiff",    &cfgMaxSnap,     cfgMaxSnap);
            cfg->Read("Limits/CycleLimit",     &cfgCycleLimit,  0L);
        }
    }

    wxPanel* limPage = new wxPanel(book);
    wxBoxSizer* limSizer = new wxBoxSizer(wxVERTICAL);

    wxStaticBoxSizer* limBox = new wxStaticBoxSizer(wxVERTICAL, limPage, "Buffer & Table Limits");
    wxFlexGridSizer* limGrid = new wxFlexGridSizer(6, 3, 8, 12);
    limGrid->AddGrowableCol(2);

    auto addLimitRow = [&](const wxString& label, wxSpinCtrl*& ctrl,
                           int val, int lo, int hi, const wxString& note)
    {
        limGrid->Add(new wxStaticText(limPage, wxID_ANY, label),
                     0, wxALIGN_CENTER_VERTICAL);
        ctrl = new wxSpinCtrl(limPage, wxID_ANY, wxEmptyString,
                              wxDefaultPosition, wxSize(110, -1),
                              wxSP_ARROW_KEYS, lo, hi, val);
        limGrid->Add(ctrl, 0, wxALIGN_CENTER_VERTICAL);
        limGrid->Add(new wxStaticText(limPage, wxID_ANY, note),
                     0, wxALIGN_CENTER_VERTICAL);
    };

    addLimitRow("Execution History Depth:",
                m_histDepthCtrl,  cfgHistDepth,  1024,   2097152,
                wxString::Format("(default %d)", SIM_HIST_DEFAULT_DEPTH));
    addLimitRow("Trace Buffer Depth:",
                m_traceDepthCtrl, cfgTraceDepth, 1000,   1000000,
                wxString::Format("(default %d)", SIM_TRACE_DEPTH));
    addLimitRow("Max Breakpoints:",
                m_maxBpCtrl,      cfgMaxBp,      4,      256,
                wxString::Format("(default %d)", MAX_BREAKPOINTS));
    addLimitRow("Max Watches:",
                m_maxWatchCtrl,   cfgMaxWatch,   4,      256,
                "(default 16)");
    addLimitRow("Max Snap-Diff Entries:",
                m_maxSnapCtrl,    cfgMaxSnap,    100,    65536,
                "(default 4096)");

    // Cycle limit row (special: has Unlimited checkbox instead of a note)
    limGrid->Add(new wxStaticText(limPage, wxID_ANY, "Max Execution Cycles:"),
                 0, wxALIGN_CENTER_VERTICAL);
    bool cycUnlimited = (cfgCycleLimit == 0);
    m_cycLimitCtrl = new wxTextCtrl(limPage, wxID_ANY,
                         cfgCycleLimit > 0 ? wxString::Format("%ld", cfgCycleLimit) : "1000000",
                         wxDefaultPosition, wxSize(110, -1));
    m_cycLimitCtrl->Enable(!cycUnlimited);
    limGrid->Add(m_cycLimitCtrl, 0, wxALIGN_CENTER_VERTICAL);
    m_cycUnlimitedCheck = new wxCheckBox(limPage, wxID_ANY, "Unlimited");
    m_cycUnlimitedCheck->SetValue(cycUnlimited);
    limGrid->Add(m_cycUnlimitedCheck, 0, wxALIGN_CENTER_VERTICAL);
    m_cycUnlimitedCheck->Bind(wxEVT_CHECKBOX, [this](wxCommandEvent&) {
        m_cycLimitCtrl->Enable(!m_cycUnlimitedCheck->IsChecked());
    });

    limBox->Add(limGrid, 0, wxEXPAND | wxALL, 10);
    limSizer->Add(limBox, 0, wxEXPAND | wxALL, 12);
    limSizer->Add(new wxStaticText(limPage, wxID_ANY,
                  "Changes take effect after the next session Reset."),
                  0, wxLEFT | wxBOTTOM, 14);

    limPage->SetSizer(limSizer);
    book->AddPage(limPage, "Limits");

    outerSizer->Add(book, 1, wxEXPAND | wxALL, 12);
    outerSizer->Add(CreateButtonSizer(wxOK | wxCANCEL), 0, wxALIGN_CENTER | wxALL, 12);
    SetSizerAndFit(outerSizer);

    // Apply initial values
    switch (theme) {
        case 0: m_themeDark->SetValue(true);  break;
        case 1: m_themeLight->SetValue(true); break;
        default: m_themeAuto->SetValue(true); break;
    }

    wxString scaleStr = wxString::Format("%.2fx", (double)uiScale);
    int scaleIdx = m_scaleCombo->FindString(scaleStr);
    m_scaleCombo->SetSelection(scaleIdx != wxNOT_FOUND ? scaleIdx
                                                       : m_scaleCombo->FindString("1.00x"));

    // Speed page initial values
    if (speedScale <= 0.0f)       m_speedCombo->SetSelection(0); // Unlimited
    else if (speedScale <= 0.75f) m_speedCombo->SetSelection(1); // 0.5x
    else if (speedScale <= 1.5f)  m_speedCombo->SetSelection(2); // 1x
    else if (speedScale <= 3.0f)  m_speedCombo->SetSelection(3); // 2x
    else if (speedScale <= 6.0f)  m_speedCombo->SetSelection(4); // 4x
    else if (speedScale <= 12.0f) m_speedCombo->SetSelection(5); // 8x
    else                          m_speedCombo->SetSelection(6); // 16x

    int machIdx = m_machineCombo->FindString(machine);
    m_machineCombo->SetSelection(machIdx != wxNOT_FOUND ? machIdx : 0);

    int procIdx = m_processorCombo->FindString(processor);
    m_processorCombo->SetSelection(procIdx != wxNOT_FOUND ? procIdx : 0);
}

int SettingsDialog::GetFontSize() const {
    return m_fontSizeCtrl->GetValue();
}

int SettingsDialog::GetTheme() const {
    if (m_themeDark->GetValue())  return 0;
    if (m_themeLight->GetValue()) return 1;
    return 2; // Auto
}

float SettingsDialog::GetUIScale() const {
    wxString val = m_scaleCombo->GetValue();
    val.Replace("x", "");
    double d = 1.0;
    val.ToDouble(&d);
    return (float)d;
}

wxString SettingsDialog::GetVICEBin()  const { return m_viceBinPicker->GetPath();  }
wxString SettingsDialog::GetVICEData() const { return m_viceDataPicker->GetPath(); }
wxString SettingsDialog::GetXemuBin()  const { return m_xemuBinPicker->GetPath();  }
wxString SettingsDialog::GetXemuData() const { return m_xemuDataPicker->GetPath(); }

const std::vector<RomEntry>& SettingsDialog::GetROMEntries(int idx) const {
    return m_romEntries[idx];
}

int SettingsDialog::GetHistDepth()      const { return m_histDepthCtrl->GetValue();  }
int SettingsDialog::GetTraceDepth()     const { return m_traceDepthCtrl->GetValue(); }
int SettingsDialog::GetMaxBreakpoints() const { return m_maxBpCtrl->GetValue();      }
int SettingsDialog::GetMaxWatches()     const { return m_maxWatchCtrl->GetValue();   }
int SettingsDialog::GetMaxSnapDiff()    const { return m_maxSnapCtrl->GetValue();    }

unsigned long SettingsDialog::GetCycleLimit() const {
    if (m_cycUnlimitedCheck->IsChecked()) return 0;
    unsigned long val;
    if (m_cycLimitCtrl->GetValue().ToULong(&val) && val > 0) return val;
    return 0;
}

void SettingsDialog::RefreshRomList() {
    m_romList->Freeze();
    m_romList->DeleteAllItems();
    const auto& entries = m_romEntries[m_romCurrentTarget];
    for (size_t i = 0; i < entries.size(); i++) {
        long row = m_romList->InsertItem((long)i, entries[i].addr);
        m_romList->SetItem(row, 1, entries[i].type);
        m_romList->SetItem(row, 2, entries[i].file);
    }
    m_romList->Thaw();
}

// --- RomEntryDialog ---
RomEntryDialog::RomEntryDialog(wxWindow* parent, const RomEntry& entry)
    : wxDialog(parent, wxID_ANY, entry.file.IsEmpty() ? "Add ROM Entry" : "Edit ROM Entry",
               wxDefaultPosition, wxSize(560, -1))
{
    static const wxString romWildcard =
        "ROM files (*.rom;*.bin)|*.rom;*.bin|All files (*.*)|*.*";

    wxFlexGridSizer* grid = new wxFlexGridSizer(3, 2, 8, 8);
    grid->AddGrowableCol(1);

    grid->Add(new wxStaticText(this, wxID_ANY, "Load Address:"), 0, wxALIGN_CENTER_VERTICAL);
    m_addrCtrl = new wxTextCtrl(this, wxID_ANY, entry.addr.IsEmpty() ? "$" : entry.addr,
                                wxDefaultPosition, wxSize(120, -1));
    grid->Add(m_addrCtrl, 0);

    grid->Add(new wxStaticText(this, wxID_ANY, "Type:"), 0, wxALIGN_CENTER_VERTICAL);
    wxArrayString types;
    types.Add("Kernal"); types.Add("Basic"); types.Add("Character");
    types.Add("Expansion"); types.Add("Other");
    m_typeCombo = new wxComboBox(this, wxID_ANY,
                                 entry.type.IsEmpty() ? "Other" : entry.type,
                                 wxDefaultPosition, wxSize(140, -1), types, wxCB_READONLY);
    grid->Add(m_typeCombo, 0);

    grid->Add(new wxStaticText(this, wxID_ANY, "File:"), 0, wxALIGN_CENTER_VERTICAL);
    m_filePicker = new wxFilePickerCtrl(this, wxID_ANY, entry.file,
                       "Select ROM file", romWildcard, wxDefaultPosition, wxDefaultSize,
                       wxFLP_USE_TEXTCTRL | wxFLP_OPEN);
    grid->Add(m_filePicker, 1, wxEXPAND);

    wxBoxSizer* outer = new wxBoxSizer(wxVERTICAL);
    outer->Add(grid, 0, wxEXPAND | wxALL, 14);
    outer->Add(CreateButtonSizer(wxOK | wxCANCEL), 0, wxALIGN_CENTER | wxBOTTOM, 10);
    SetSizerAndFit(outer);
}

RomEntry RomEntryDialog::GetEntry() const {
    RomEntry e;
    e.addr = m_addrCtrl->GetValue().Trim();
    e.type = m_typeCombo->GetValue();
    e.file = m_filePicker->GetPath();
    return e;
}

float SettingsDialog::GetSpeedScale() const {
    int sel = m_speedCombo->GetSelection();
    switch (sel) {
        case 1: return 0.5f;
        case 2: return 1.0f;
        case 3: return 2.0f;
        case 4: return 4.0f;
        case 5: return 8.0f;
        case 6: return 16.0f;
        default: return 0.0f; // Unlimited
    }
}

wxString SettingsDialog::GetDefaultMachine()   const { return m_machineCombo->GetValue();   }
wxString SettingsDialog::GetDefaultProcessor() const { return m_processorCombo->GetValue(); }

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
