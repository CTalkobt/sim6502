#ifndef SIM_PANE_GENERIC_H
#define SIM_PANE_GENERIC_H

#include "pane_base.h"

class GenericSimPane : public SimPane {
public:
    GenericSimPane(wxWindow* parent, sim_session_t *sim, const wxString& title, const wxString& name)
        : SimPane(parent, sim), m_title(title), m_name(name) 
    {
        wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
        sizer->Add(new wxStaticText(this, wxID_ANY, title), 0, wxALL | wxALIGN_CENTER, 10);
        SetSizer(sizer);
    }

    void RefreshPane(const SimSnapshot &snap) override { (void)snap; }
    wxString GetPaneTitle() const override { return m_title; }
    wxString GetPaneName() const override { return m_name; }

private:
    wxString m_title;
    wxString m_name;
};

#endif // SIM_PANE_GENERIC_H
