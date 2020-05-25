#if WITH_EDITOR

#include "pch.h"
#include "FileBrowser.h"

namespace Fling
{
    FileBrowser::FileBrowser(std::string t_Title)
		: m_Title(t_Title)
    {
		m_CurrentWorkingDir = FlingPaths::EngineAssetsDir();
    }

	void FileBrowser::SetTitle(std::string t_Title)
	{
		m_Title = t_Title;
	}

	void FileBrowser::Open()
	{
		ImGui::OpenPopup(m_Title.c_str());
	}

	void FileBrowser::ClearSelected()
	{
		m_HasSelected = false;
		m_SelectedFile = std::string();
	}

	void FileBrowser::Display()
	{
		bool open = true;
		ImGui::SetNextWindowSize(ImVec2(700, 450), ImGuiCond_FirstUseEver);

		if (ImGui::BeginPopupModal(m_Title.c_str(), &open))
		{
			namespace fs = std::filesystem;

			// For each file in 
			for (const auto& entry : fs::directory_iterator(m_CurrentWorkingDir))
			{
				std::string FilePath = entry.path().generic_string();
				const bool IsSelected = m_SelectedFile == FilePath;
				std::string DisplayName = (entry.is_directory() ? "[D] " : "[F] ") + FilePath;
				if (ImGui::Selectable(DisplayName.c_str(), IsSelected, ImGuiSelectableFlags_DontClosePopups))
				{
					if (entry.is_directory())
					{
						// Do deeper into this dir
						m_CurrentWorkingDir = FilePath;
						break;
					}
					else if (entry.is_regular_file())
					{
						m_SelectedFile = std::string(FilePath);
					}
				}
			}

			if (ImGui::Selectable("..", false, ImGuiSelectableFlags_DontClosePopups))
			{
				// Go back a directory
				
			}

			ImGui::Text("------");

			ImGui::TextUnformatted("Selected file %s", m_SelectedFile.c_str());

			ImGui::SameLine();

			if (ImGui::Button("Open Selected"))
			{
				m_HasSelected = true;
				ImGui::CloseCurrentPopup();
			}

			ImGui::SameLine();

			if (ImGui::Button("Close"))
			{
				m_HasSelected = false;
				ImGui::CloseCurrentPopup();
			}

			/*if (!typeFilters_.empty())
			{
				SameLine();
				PushItemWidth(8 * GetFontSize());
				Combo("##Filters", &typeFilterIndex_, typeFilters_.data(), int(typeFilters_.size()));
				PopItemWidth();
			}*/

			ImGui::EndPopup();
		}
	}
}

#endif