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

	void FileBrowser::Display(bool* t_OutHasSelectedFile)
	{
		ImGui::BeginChild(m_Title.c_str());
		ImGui::SetNextWindowSize(ImVec2(200, 200), ImGuiCond_FirstUseEver);

		ImGui::LabelText("Test", "Hey");

		if (ImGui::Button("Test"))
		{
			if (t_OutHasSelectedFile)
			{
				*t_OutHasSelectedFile = true;
			}
		}

		ImGui::EndChild();
	}
}