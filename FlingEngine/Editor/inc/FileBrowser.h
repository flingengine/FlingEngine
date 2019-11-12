#pragma once

#include <string>
#include <imgui.h>

namespace Fling
{
    class FileBrowser
    {
    public: 
        FileBrowser(std::string t_Title = "Open File...");
        ~FileBrowser() = default;

        /** Display the actual ImGUI info here. */
		void Display();

        /** Set the text in the title bar of this file dialoag */
		void SetTitle(std::string t_Title);

        /** Opens this file dialoag next time Display is called */
        void Open();

        bool HasSelected() const { return m_HasSelected; }
        std::string GetSelected() const { m_SelectedFile; }
        void ClearSelected();

        bool IsOpen() const { return m_IsOpen; }

	private:
		std::string m_Title = "Select File...";

        /** The selected file in this dialog */
        std::string m_SelectedFile = "NONE";

        bool m_IsOpen = false;

        bool m_HasSelected = false;
    };
}   // namespace Fling