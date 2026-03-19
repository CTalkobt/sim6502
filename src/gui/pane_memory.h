#ifndef SIM_PANE_MEMORY_H
#define SIM_PANE_MEMORY_H

#include "pane_base.h"
#include <wx/listctrl.h>
#include <wx/textctrl.h>

#include <set>

class MemoryListCtrl : public wxListCtrl {
public:
    MemoryListCtrl(wxWindow* parent, sim_session_t* sim);
    wxString OnGetItemText(long item, long column) const override;
    wxListItemAttr* OnGetItemAttr(long item) const override;

    void SetCharset(int charset) { m_charset = charset; }
    void SetChangedAddresses(const std::set<uint16_t>& addrs) { m_changedAddrs = addrs; }

private:
    sim_session_t* m_sim;
    int            m_charset; // 0: ASCII, 1: PETSCII
    std::set<uint16_t> m_changedAddrs;
    mutable wxListItemAttr m_attrChanged;
};

class PaneMemory : public SimPane {
public:
    PaneMemory(wxWindow* parent, sim_session_t *sim, int index);
    void RefreshPane(const SimSnapshot &snap) override;
    wxString GetPaneTitle() const override { return m_title; }
    wxString GetPaneName() const override;

    void ScrollTo(uint16_t addr);
    
    void LoadState(wxConfigBase* cfg) override;
    void SaveState(wxConfigBase* cfg) override;

private:
    void OnGoToAddress(wxCommandEvent& event);
    void OnPrevPage(wxCommandEvent& event);
    void OnNextPage(wxCommandEvent& event);
    void OnFollowMode(wxCommandEvent& event);
    void OnCharsetMode(wxCommandEvent& event);
    void OnRename(wxCommandEvent& event);
    void OnItemActivated(wxListEvent& event);

    int             m_index;
    wxString        m_title;
    MemoryListCtrl* m_list;
    wxTextCtrl*     m_addrSearch;
    wxChoice*       m_followChoice;
    wxChoice*       m_charsetChoice;
    uint16_t        m_followAddr; // For "Specified Word Pointer" mode
};

#endif
