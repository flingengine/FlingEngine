#pragma once

#include "KeyState.h"
#include "FlingTypes.h"
#include <string>

namespace Fling
{
    class Key
    {
    private:
        /** Name of this key binding */
        std::string m_Name;
        
        /** The current state of this key. Updated when input is polled */
        KeyState m_State;

        /** The key code that is mapped to this key to the current platform */
        uint32 m_KeyCode;

    public:

        explicit Key(const std::string& t_Name, uint32 t_KeyCode)
            : m_Name(t_Name)
			, m_State(KeyState::UP)
			, m_KeyCode(t_KeyCode)
        {}

        ~Key() = default;

        /** Set the state of this key */
        void SetState(KeyState t_NewState){ m_State = t_NewState; }
        inline KeyState GetState () const { return m_State; }
        inline uint32 GetCode () const { return m_KeyCode; }

        inline const std::string& GetName() const { return m_Name; }

        inline bool IsPressed() const { return m_State == KeyState::DOWN; }
        inline bool IsUp() const { return m_State == KeyState::UP; }

    };


    struct KeyNames
    {
        /* The unknown key */
        static std::string FL_KEY_UNKNOWN;

        /* Printable keys */
        static std::string FL_KEY_SPACE;             
        static std::string FL_KEY_APOSTROPHE;          /* ' */
        static std::string FL_KEY_COMMA;               /* , */
        static std::string FL_KEY_MINUS;               /* - */
        static std::string FL_KEY_PERIOD;              /* . */
        static std::string FL_KEY_SLASH;               /* / */
        static std::string FL_KEY_0;                 
        static std::string FL_KEY_1;                 
        static std::string FL_KEY_2;                 
        static std::string FL_KEY_3;
        static std::string FL_KEY_4;
        static std::string FL_KEY_5;
        static std::string FL_KEY_6;
        static std::string FL_KEY_7;
        static std::string FL_KEY_8;
        static std::string FL_KEY_9;
        static std::string FL_KEY_SEMICOLON;  /* ; */
        static std::string FL_KEY_EQUAL;  /* = */
        static std::string FL_KEY_A;
        static std::string FL_KEY_B;
        static std::string FL_KEY_C;
        static std::string FL_KEY_D;
        static std::string FL_KEY_E;
        static std::string FL_KEY_F;
        static std::string FL_KEY_G;
        static std::string FL_KEY_H;
        static std::string FL_KEY_I;
        static std::string FL_KEY_J;
        static std::string FL_KEY_K;
        static std::string FL_KEY_L;
        static std::string FL_KEY_M;
        static std::string FL_KEY_N;
        static std::string FL_KEY_O;
        static std::string FL_KEY_P;
        static std::string FL_KEY_Q;
        static std::string FL_KEY_R;
        static std::string FL_KEY_S;
        static std::string FL_KEY_T;
        static std::string FL_KEY_U;
        static std::string FL_KEY_V;
        static std::string FL_KEY_W;
        static std::string FL_KEY_X;
        static std::string FL_KEY_Y;
        static std::string FL_KEY_Z;
        static std::string FL_KEY_LEFT_BRACKET;  /* [ */
        static std::string FL_KEY_BACKSLASH;  /* \ */
        static std::string FL_KEY_RIGHT_BRACKET;  /* ] */
        static std::string FL_KEY_GRAVE_ACCENT;  /* ` */
        static std::string FL_KEY_WORLD_1; /* non-US #1 */
        static std::string FL_KEY_WORLD_2; /* non-US #2 */

        /* Function keys */
        static std::string FL_KEY_ESCAPE;
        static std::string FL_KEY_ENTER;
        static std::string FL_KEY_TAB;
        static std::string FL_KEY_BACKSPACE;
        static std::string FL_KEY_INSERT;
        static std::string FL_KEY_DELETE;
        static std::string FL_KEY_RIGHT;
        static std::string FL_KEY_LEFT;
        static std::string FL_KEY_DOWN;
        static std::string FL_KEY_UP;
        static std::string FL_KEY_PAGE_UP;
        static std::string FL_KEY_PAGE_DOWN;
        static std::string FL_KEY_HOME;
        static std::string FL_KEY_END;
        static std::string FL_KEY_CAPS_LOCK;
        static std::string FL_KEY_SCROLL_LOCK;
        static std::string FL_KEY_NUM_LOCK;
        static std::string FL_KEY_PRINT_SCREEN;
        static std::string FL_KEY_PAUSE;
        static std::string FL_KEY_F1;
        static std::string FL_KEY_F2;
        static std::string FL_KEY_F3;
        static std::string FL_KEY_F4;
        static std::string FL_KEY_F5;
        static std::string FL_KEY_F6;
        static std::string FL_KEY_F7;
        static std::string FL_KEY_F8;
        static std::string FL_KEY_F9;
        static std::string FL_KEY_F10;
        static std::string FL_KEY_F11;
        static std::string FL_KEY_F12;
        static std::string FL_KEY_F13;
        static std::string FL_KEY_F14;
        static std::string FL_KEY_F15;
        static std::string FL_KEY_F16;
        static std::string FL_KEY_F17;
        static std::string FL_KEY_F18;
        static std::string FL_KEY_F19;
        static std::string FL_KEY_F20;
        static std::string FL_KEY_F21;
        static std::string FL_KEY_F22;
        static std::string FL_KEY_F23;
        static std::string FL_KEY_F24;
        static std::string FL_KEY_F25;
        static std::string FL_KEY_KP_0;
        static std::string FL_KEY_KP_1;
        static std::string FL_KEY_KP_2;
        static std::string FL_KEY_KP_3;
        static std::string FL_KEY_KP_4;
        static std::string FL_KEY_KP_5;
        static std::string FL_KEY_KP_6;
        static std::string FL_KEY_KP_7;
        static std::string FL_KEY_KP_8;
        static std::string FL_KEY_KP_9;
        static std::string FL_KEY_KP_DECIMAL;
        static std::string FL_KEY_KP_DIVIDE;
        static std::string FL_KEY_KP_MULTIPLY;
        static std::string FL_KEY_KP_SUBTRACT;
        static std::string FL_KEY_KP_ADD;
        static std::string FL_KEY_KP_ENTER;
        static std::string FL_KEY_KP_EQUAL;
        static std::string FL_KEY_LEFT_SHIFT;
        static std::string FL_KEY_LEFT_CONTROL;
        static std::string FL_KEY_LEFT_ALT;
        static std::string FL_KEY_LEFT_SUPER;
        static std::string FL_KEY_RIGHT_SHIFT;
        static std::string FL_KEY_RIGHT_CONTROL;
        static std::string FL_KEY_RIGHT_ALT;
        static std::string FL_KEY_RIGHT_SUPER;
        static std::string FL_KEY_MENU;

        static std::string FL_MOUSE_BUTTON_1;
        static std::string FL_MOUSE_BUTTON_2;
        static std::string FL_MOUSE_BUTTON_3;
        static std::string FL_MOUSE_BUTTON_4;
        static std::string FL_MOUSE_BUTTON_5;
        static std::string FL_MOUSE_BUTTON_6;
        static std::string FL_MOUSE_BUTTON_7;
        static std::string FL_MOUSE_BUTTON_8;
    };
    

}   // namespace Fling