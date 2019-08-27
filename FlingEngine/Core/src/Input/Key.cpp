#include "pch.h"
#include "Input/Key.h"

namespace Fling
{
    /* The unknown key */
    std::string KeyNames::FL_KEY_UNKNOWN = "UNKNOWN";

    /* Printable keys */
    std::string KeyNames::FL_KEY_SPACE = "SPACE";             
    std::string KeyNames::FL_KEY_APOSTROPHE = "APOSTROPHE";          /* ' */
    std::string KeyNames::FL_KEY_COMMA = "COMMA";               /* , */
    std::string KeyNames::FL_KEY_MINUS = "MINUS";               /* - */
    std::string KeyNames::FL_KEY_PERIOD = "PERIOD";              /* . */
    std::string KeyNames::FL_KEY_SLASH = "SLASH";               /* / */
    std::string KeyNames::FL_KEY_0 = "0";                 
    std::string KeyNames::FL_KEY_1 = "1";                 
    std::string KeyNames::FL_KEY_2 = "2";                 
    std::string KeyNames::FL_KEY_3 = "3";
    std::string KeyNames::FL_KEY_4 = "4";
    std::string KeyNames::FL_KEY_5 = "5";
    std::string KeyNames::FL_KEY_6 = "6";
    std::string KeyNames::FL_KEY_7 = "7";
    std::string KeyNames::FL_KEY_8 = "8";
    std::string KeyNames::FL_KEY_9 = "9";
    std::string KeyNames::FL_KEY_SEMICOLON = "SEMICOLON";  /* ; */
    std::string KeyNames::FL_KEY_EQUAL = "EQUAL";  /* = */
    std::string KeyNames::FL_KEY_A = "A";
    std::string KeyNames::FL_KEY_B = "B";
    std::string KeyNames::FL_KEY_C = "C";
    std::string KeyNames::FL_KEY_D = "D";
    std::string KeyNames::FL_KEY_E = "E";
    std::string KeyNames::FL_KEY_F = "F";
    std::string KeyNames::FL_KEY_G = "G";
    std::string KeyNames::FL_KEY_H = "H";
    std::string KeyNames::FL_KEY_I = "I";
    std::string KeyNames::FL_KEY_J = "J";
    std::string KeyNames::FL_KEY_K = "K";
    std::string KeyNames::FL_KEY_L = "L";
    std::string KeyNames::FL_KEY_M = "M";
    std::string KeyNames::FL_KEY_N = "N";
    std::string KeyNames::FL_KEY_O = "O";
    std::string KeyNames::FL_KEY_P = "P";
    std::string KeyNames::FL_KEY_Q = "Q";
    std::string KeyNames::FL_KEY_R = "R";
    std::string KeyNames::FL_KEY_S = "S";
    std::string KeyNames::FL_KEY_T = "T";
    std::string KeyNames::FL_KEY_U = "U";
    std::string KeyNames::FL_KEY_V = "V";
    std::string KeyNames::FL_KEY_W = "W";
    std::string KeyNames::FL_KEY_X = "X";
    std::string KeyNames::FL_KEY_Y = "Y";
    std::string KeyNames::FL_KEY_Z = "Z";
    std::string KeyNames::FL_KEY_LEFT_BRACKET = "BRACKET";  /* [ */
    std::string KeyNames::FL_KEY_BACKSLASH = "BACKSLASH";  /* \ */
    std::string KeyNames::FL_KEY_RIGHT_BRACKET = "BRACKET";  /* ] */
    std::string KeyNames::FL_KEY_GRAVE_ACCENT = "ACCENT";  /* ` */
    std::string KeyNames::FL_KEY_WORLD_1 = "WORLD_1"; /* non-US #1 */
    std::string KeyNames::FL_KEY_WORLD_2 = "WORLD_1"; /* non-US #2 */

    /* Function keys */
    std::string KeyNames::FL_KEY_ESCAPE = "ESCAPE";
    std::string KeyNames::FL_KEY_ENTER = "ENTER";
    std::string KeyNames::FL_KEY_TAB = "TAB";
    std::string KeyNames::FL_KEY_BACKSPACE = "BACKSPACE";
    std::string KeyNames::FL_KEY_INSERT = "INSERT";
    std::string KeyNames::FL_KEY_DELETE = "DELETE";
    std::string KeyNames::FL_KEY_RIGHT = "RIGHT";
    std::string KeyNames::FL_KEY_LEFT = "LEFT";
    std::string KeyNames::FL_KEY_DOWN = "DOWN";
    std::string KeyNames::FL_KEY_UP = "UP";
    std::string KeyNames::FL_KEY_PAGE_UP = "PAGE_UP";
    std::string KeyNames::FL_KEY_PAGE_DOWN = "PAGE_DOWN";
    std::string KeyNames::FL_KEY_HOME = "HOME";
    std::string KeyNames::FL_KEY_END = "END";
    std::string KeyNames::FL_KEY_CAPS_LOCK = "CAPS_LOCK";
    std::string KeyNames::FL_KEY_SCROLL_LOCK = "SCROLL_LOCK";
    std::string KeyNames::FL_KEY_NUM_LOCK = "NUM_LOCK";
    std::string KeyNames::FL_KEY_PRINT_SCREEN = "PRINT_SCREEN";
    std::string KeyNames::FL_KEY_PAUSE = "PAUSE";
    std::string KeyNames::FL_KEY_F1 = "F1";
    std::string KeyNames::FL_KEY_F2 = "F2";
    std::string KeyNames::FL_KEY_F3 = "F3";
    std::string KeyNames::FL_KEY_F4 = "F4";
    std::string KeyNames::FL_KEY_F5 = "F5";
    std::string KeyNames::FL_KEY_F6 = "F6";
    std::string KeyNames::FL_KEY_F7 = "F7";
    std::string KeyNames::FL_KEY_F8 = "F8";
    std::string KeyNames::FL_KEY_F9 = "F9";
    std::string KeyNames::FL_KEY_F10 = "F10";
    std::string KeyNames::FL_KEY_F11 = "F11";
    std::string KeyNames::FL_KEY_F12 = "F12";
    std::string KeyNames::FL_KEY_KP_DECIMAL = "";
    std::string KeyNames::FL_KEY_KP_DIVIDE = "";
    std::string KeyNames::FL_KEY_KP_MULTIPLY = "";
    std::string KeyNames::FL_KEY_KP_SUBTRACT = "";
    std::string KeyNames::FL_KEY_KP_ADD = "";
    std::string KeyNames::FL_KEY_KP_ENTER = "";
    std::string KeyNames::FL_KEY_KP_EQUAL = "";
    std::string KeyNames::FL_KEY_LEFT_SHIFT = "LEFT_SHIFT";
    std::string KeyNames::FL_KEY_LEFT_CONTROL = "LEFT_CONTROL";
    std::string KeyNames::FL_KEY_LEFT_ALT = "LEFT_ALT";
    std::string KeyNames::FL_KEY_LEFT_SUPER = "LEFT_SUPER";
    std::string KeyNames::FL_KEY_RIGHT_SHIFT = "RIGHT_SHIFT";
    std::string KeyNames::FL_KEY_RIGHT_CONTROL = "RIGHT_CONTROL";
    std::string KeyNames::FL_KEY_RIGHT_ALT = "RIGHT_ALT";
    std::string KeyNames::FL_KEY_RIGHT_SUPER = "RIGHT_SUPER";
    std::string KeyNames::FL_KEY_MENU = "MENU";

    std::string KeyNames::FL_MOUSE_BUTTON_1 = "MOUSE_1";
    std::string KeyNames::FL_MOUSE_BUTTON_2 = "MOUSE_2";
    std::string KeyNames::FL_MOUSE_BUTTON_3 = "MOUSE_3";
    std::string KeyNames::FL_MOUSE_BUTTON_4 = "MOUSE_4";
    std::string KeyNames::FL_MOUSE_BUTTON_5 = "MOUSE_5";
    std::string KeyNames::FL_MOUSE_BUTTON_6 = "MOUSE_6";
    std::string KeyNames::FL_MOUSE_BUTTON_7 = "MOUSE_7";
    std::string KeyNames::FL_MOUSE_BUTTON_8 = "MOUSE_8";
}