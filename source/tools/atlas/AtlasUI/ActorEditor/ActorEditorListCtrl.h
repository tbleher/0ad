#include "DraggableListCtrl.h"

#include "IAtlasExporter.h"

class ActorEditor;

class ActorEditorListCtrl : public DraggableListCtrl
{
	friend class ActorEditor;

public:
	ActorEditorListCtrl(wxWindow* parent);

	void OnUpdate(wxCommandEvent& event);

	wxListItemAttr* OnGetItemAttr(long item) const;

private:
	void DoImport(AtObj& in);
	AtObj DoExport();

	wxListItemAttr m_ListItemAttr_Model[2];
	wxListItemAttr m_ListItemAttr_Texture[2];
	wxListItemAttr m_ListItemAttr_Anim[2];
	wxListItemAttr m_ListItemAttr_Prop[2];
};
