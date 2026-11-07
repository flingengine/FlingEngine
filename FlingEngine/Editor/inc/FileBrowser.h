#pragma once

#include "Strings/String.h"


#if WITH_IMGUI
#include <imgui.h>
#endif
namespace Fling
{
    class FileBrowser
    {
    public: 
        FileBrowser(String t_Title = "Open File...");
        ~FileBrowser() = default;

        /** Display the actual ImGUI info here. */
		void Display();

        /** Set the text in the title bar of this file dialoag */
		void SetTitle(String t_Title);

        /** Opens this file dialoag next time Display is called */
        void Open();

        bool HasSelected() const { return m_HasSelected; }
        String GetSelected() const { return m_SelectedFile; }
        void ClearSelected();

        bool IsOpen() const { return m_IsOpen; }

	private:
		String m_Title = "Select File...";

        /** The selected file in this dialog */
        String m_SelectedFile = String();

		String m_CurrentWorkingDir = String();

        bool m_IsOpen = false;

        bool m_HasSelected = false;
    };
}   // namespace Fling