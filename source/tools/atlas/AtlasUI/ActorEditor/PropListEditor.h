#include "AtlasDialog.h"

#include "DraggableListCtrl.h"
#include "IAtlasExporter.h"

class PropListEditorListCtrl;

//////////////////////////////////////////////////////////////////////////

class PropListEditor : public AtlasDialog
{
	DECLARE_DYNAMIC_CLASS(PropListEditor);

public:
	PropListEditor();

protected:
	void Import(AtObj& in);
	AtObj Export();

private:
	PropListEditorListCtrl* m_MainListBox;
};

//////////////////////////////////////////////////////////////////////////

class PropListEditorListCtrl : public DraggableListCtrl
{
	friend class PropListEditor;

public:
	PropListEditorListCtrl(wxWindow* parent);

	void OnUpdate(wxCommandEvent& event);

	void DoImport(AtObj& in);
	AtObj DoExport();
};
