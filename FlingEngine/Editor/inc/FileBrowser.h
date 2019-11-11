#pragma once

#include <string>
#include <imgui.h>

namespace Fling
{
    class FileBrowser
    {
    public: 
        FileBrowser(std::string t_Title);
        ~FileBrowser() = default;

		void Display(bool* t_OutHasSelectedFile);

		void SetTitle(std::string t_Title);

	private:
		std::string m_Title = "Select File...";
    };
}   // namespace Fling