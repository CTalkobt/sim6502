#ifndef SIM_PANE_SNAP_DIFF_H
#define SIM_PANE_SNAP_DIFF_H

#include "pane_base.h"
#include <wx/listctrl.h>

#include <vector>

class VirtualDiffList : public wxListCtrl {
public:
    VirtualDiffList(wxWindow* parent, wxWindowID id, const wxPoint& pos, const wxSize& size, long style)
        : wxListCtrl(parent, id, pos, size, style) {}

    wxString OnGetItemText(long item, long column) const override;
    void SetDiffData(const std::vector<sim_diff_entry_t>& data) { m_data = data; SetItemCount((long)m_data.size()); }
    const std::vector<sim_diff_entry_t>& GetDiffData() const { return m_data; }

private:
    std::vector<sim_diff_entry_t> m_data;
};

class PaneSnapDiff : public SimPane {
public:
    PaneSnapDiff(wxWindow* parent, sim_session_t *sim);
    void RefreshPane(const SimSnapshot &snap) override;
    wxString GetPaneTitle() const override { return "Snapshot Diff"; }
    wxString GetPaneName() const override { return "SnapDiff"; }

private:
    void OnTakeSnapshot(wxCommandEvent& event);
    void OnClear(wxCommandEvent& event);
    void OnItemActivated(wxListEvent& event);

    VirtualDiffList* m_list;
    wxStaticText* m_status_text;
    uint64_t m_last_snap_cycles;
    uint32_t m_last_snap_timestamp;
    uint64_t m_last_cpu_cycles;
    int m_last_count;
};

#endif
