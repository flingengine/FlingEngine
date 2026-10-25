#include "FileBrowserPopup.h"

#include <imgui.h>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <system_error>

namespace Fling
{
    namespace
    {
        namespace fs = std::filesystem;

        std::string ToLower(std::string t_Text)
        {
            std::transform(t_Text.begin(),
                           t_Text.end(),
                           t_Text.begin(),
                           [](unsigned char t_Char) { return static_cast<char>(std::tolower(t_Char)); });
            return t_Text;
        }

        bool EndsWith(const std::string& t_Text, const std::string& t_Ending)
        {
            return t_Text.size() >= t_Ending.size() &&
                   ToLower(t_Text.substr(t_Text.size() - t_Ending.size())) == ToLower(t_Ending);
        }

        bool CompareNames(const std::string& t_A, const std::string& t_B)
        {
            return ToLower(t_A) < ToLower(t_B);
        }
    } // namespace

    void FileBrowserPopup::Open(const std::string& t_StartPath, const std::string& t_Extension)
    {
        m_Extension = t_Extension;
        m_Selected.clear();
        m_Error.clear();

        std::error_code Ec;
        fs::path Path = fs::absolute(fs::path(t_StartPath), Ec);
        if (Ec || t_StartPath.empty())
        {
            Path = fs::current_path(Ec);
        }
        Path = Path.lexically_normal();

        fs::path Dir;
        if (fs::is_regular_file(Path, Ec))
        {
            Dir = Path.parent_path();
            m_Selected = Path.filename().string();
        }
        else
        {
            // Walk up to the nearest folder that exists
            Dir = Path;
            while (!Dir.empty() && !fs::is_directory(Dir, Ec))
            {
                const fs::path Parent = Dir.parent_path();
                if (Parent == Dir)
                {
                    Dir.clear();
                    break;
                }
                Dir = Parent;
            }
        }

        if (Dir.empty())
        {
            Dir = fs::current_path(Ec);
        }
        m_CurrentDir = Dir.string();
        m_OpenRequested = true;
        m_IsOpen = true;
        Refresh();
    }

    void FileBrowserPopup::Refresh()
    {
        m_Folders.clear();
        m_Files.clear();
        m_Error.clear();

        std::error_code Ec;
        fs::directory_iterator It(fs::path(m_CurrentDir), fs::directory_options::skip_permission_denied, Ec);
        if (Ec)
        {
            m_Error = "Cannot read folder: " + Ec.message();
            return;
        }

        const fs::directory_iterator End;
        while (It != End)
        {
            std::error_code EntryEc;
            const std::string Name = It->path().filename().string();
            if (It->is_directory(EntryEc))
            {
                m_Folders.push_back(Name);
            }
            else if (It->is_regular_file(EntryEc) && (m_Extension.empty() || EndsWith(Name, m_Extension)))
            {
                m_Files.push_back(Name);
            }

            It.increment(Ec);
            if (Ec)
            {
                m_Error = "Cannot read folder: " + Ec.message();
                break;
            }
        }

        std::sort(m_Folders.begin(), m_Folders.end(), CompareNames);
        std::sort(m_Files.begin(), m_Files.end(), CompareNames);
    }

    bool FileBrowserPopup::Draw(const char* t_Title, std::string& t_OutPath)
    {
        if (!m_IsOpen)
        {
            return false;
        }

        if (m_OpenRequested)
        {
            ImGui::OpenPopup(t_Title);
            m_OpenRequested = false;
        }

        ImGui::SetNextWindowSize(ImVec2(560.0f, 420.0f), ImGuiCond_Appearing);

        bool Chosen = false;
        bool KeepOpen = true;
        if (ImGui::BeginPopupModal(t_Title, &KeepOpen))
        {
            ImGui::TextWrapped("%s", m_CurrentDir.c_str());

            if (ImGui::Button("Up"))
            {
                const fs::path Current(m_CurrentDir);
                const fs::path Parent = Current.parent_path();
                if (!Parent.empty() && Parent != Current)
                {
                    m_CurrentDir = Parent.string();
                    m_Selected.clear();
                    Refresh();
                }
            }

            if (!m_Error.empty())
            {
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", m_Error.c_str());
            }

            const float FooterHeight = ImGui::GetFrameHeightWithSpacing() * 2.0f + ImGui::GetStyle().ItemSpacing.y;
            if (ImGui::BeginChild("FileBrowserList", ImVec2(0.0f, -FooterHeight), ImGuiChildFlags_Borders))
            {
                std::string OpenFolder;
                for (const std::string& Folder : m_Folders)
                {
                    const std::string Label = "[dir] " + Folder + "/";
                    if (ImGui::Selectable(Label.c_str(), false))
                    {
                        OpenFolder = Folder;
                    }
                }

                for (const std::string& File : m_Files)
                {
                    if (ImGui::Selectable(File.c_str(), File == m_Selected, ImGuiSelectableFlags_AllowDoubleClick))
                    {
                        m_Selected = File;
                        if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                        {
                            Chosen = true;
                        }
                    }
                }

                if (!OpenFolder.empty())
                {
                    m_CurrentDir = (fs::path(m_CurrentDir) / OpenFolder).string();
                    m_Selected.clear();
                    Refresh();
                }
            }
            ImGui::EndChild();

            ImGui::SetNextItemWidth(-1.0f);
            char Buffer[1024] = {};
            m_Selected.copy(Buffer, sizeof(Buffer) - 1);
            if (ImGui::InputText("##FileBrowserName", Buffer, sizeof(Buffer)))
            {
                m_Selected = Buffer;
            }

            ImGui::BeginDisabled(m_Selected.empty());
            if (ImGui::Button("Select"))
            {
                Chosen = true;
            }
            ImGui::EndDisabled();
            ImGui::SameLine();
            const bool Cancelled = ImGui::Button("Cancel");

            if (Chosen && !m_Selected.empty())
            {
                std::error_code Ec;
                fs::path Result = fs::path(m_Selected);
                if (Result.is_relative())
                {
                    Result = fs::path(m_CurrentDir) / Result;
                }
                Result = fs::absolute(Result, Ec).lexically_normal();
                t_OutPath = Result.string();
                m_IsOpen = false;
                ImGui::CloseCurrentPopup();
            }
            else if (Cancelled)
            {
                Chosen = false;
                m_IsOpen = false;
                ImGui::CloseCurrentPopup();
            }
            else
            {
                Chosen = false;
            }

            ImGui::EndPopup();
        }

        if (!KeepOpen)
        {
            m_IsOpen = false;
        }
        return Chosen;
    }
} // namespace Fling
