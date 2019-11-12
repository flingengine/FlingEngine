#include "FileBrowser.h"

namespace Fling
{
    FileBrowser::FileBrowser(std::string t_Title)
		: m_Title(t_Title)
    {
    }

	void FileBrowser::SetTitle(std::string t_Title)
	{
		m_Title = t_Title;
	}

	void FileBrowser::Open()
	{
		m_IsOpen = true;
	}

	void FileBrowser::ClearSelected()
	{
		m_HasSelected = false;
		m_SelectedFile = "NONE";
	}

	void FileBrowser::Display()
	{
		if(!m_IsOpen)
		{
			return;
		}
		
		ImGui::OpenPopup(m_Title.c_str());
		ImGui::SetNextWindowSize(ImVec2(200, 200), ImGuiCond_FirstUseEver);

		ImGui::LabelText("Test", "Hey");

		if (ImGui::Button("Close"))
		{
			m_IsOpen = false;
		}

		ImGui::EndPopup();
	}
}