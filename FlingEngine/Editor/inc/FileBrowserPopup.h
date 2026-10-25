#pragma once

#include <string>
#include <vector>

namespace Fling
{
    /**
     * A modal ImGui window that selects one file. It shows the folders and the files of
     * one folder, with a filter on the file name end (for example ".jsonl"). You can go
     * up a folder, open a folder, or type a path.
     *
     * Call Open once, then call Draw every frame. Draw returns true on the frame that a
     * file is selected.
     */
    class FileBrowserPopup
    {
    public:
        /**
         * Open the window on the next Draw.
         *
         * @param t_StartPath   A folder, or a file whose folder to show (the file is selected).
         *                      If it does not exist, the nearest parent folder that exists is shown
         * @param t_Extension   Show only files whose name ends with this. Empty shows every file
         */
        void Open(const std::string& t_StartPath, const std::string& t_Extension);

        /**
         * Draw the window if it is open.
         *
         * @param t_Title       The window title. It is also the ImGui popup ID, so keep it the same
         * @param t_OutPath     Receives the absolute path of the selected file
         * @return True on the frame that a file is selected
         */
        bool Draw(const char* t_Title, std::string& t_OutPath);

        bool IsOpen() const { return m_IsOpen; }

    private:
        /** Read the folders and files of m_CurrentDir. */
        void Refresh();

        std::string m_CurrentDir;

        std::string m_Extension;

        /** The file name that is selected in the list, or typed. */
        std::string m_Selected;

        std::vector<std::string> m_Folders;

        std::vector<std::string> m_Files;

        std::string m_Error;

        bool m_IsOpen = false;

        bool m_OpenRequested = false;
    };
} // namespace Fling
