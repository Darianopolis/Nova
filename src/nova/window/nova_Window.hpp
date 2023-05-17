#pragma once

#include <nova/core/nova_Core.hpp>

namespace nova
{
    enum class Key : u32
    {
        A = GLFW_KEY_A,
        B = GLFW_KEY_B,
        C = GLFW_KEY_C,
        D = GLFW_KEY_D,
        E = GLFW_KEY_E,
        F = GLFW_KEY_F,
        G = GLFW_KEY_G,
        H = GLFW_KEY_H,
        I = GLFW_KEY_I,
        J = GLFW_KEY_J,
        K = GLFW_KEY_K,
        L = GLFW_KEY_L,
        M = GLFW_KEY_M,
        N = GLFW_KEY_N,
        O = GLFW_KEY_O,
        P = GLFW_KEY_P,
        Q = GLFW_KEY_Q,
        R = GLFW_KEY_R,
        S = GLFW_KEY_S,
        T = GLFW_KEY_T,
        U = GLFW_KEY_U,
        V = GLFW_KEY_V,
        W = GLFW_KEY_W,
        X = GLFW_KEY_X,
        Y = GLFW_KEY_Y,
        Z = GLFW_KEY_Z,

        _0 = GLFW_KEY_0,
        _1 = GLFW_KEY_1,
        _2 = GLFW_KEY_2,
        _3 = GLFW_KEY_3,
        _4 = GLFW_KEY_4,
        _5 = GLFW_KEY_5,
        _6 = GLFW_KEY_6,
        _7 = GLFW_KEY_7,
        _8 = GLFW_KEY_8,
        _9 = GLFW_KEY_9,

        Tab = GLFW_KEY_TAB,
        Space = GLFW_KEY_SPACE,
        Backspace = GLFW_KEY_BACKSPACE,
        Enter = GLFW_KEY_ENTER,

        Left = GLFW_KEY_LEFT,
        Up = GLFW_KEY_UP,
        Right = GLFW_KEY_RIGHT,
        Down = GLFW_KEY_DOWN,

        LeftShift = GLFW_KEY_LEFT_SHIFT,
        RightShift = GLFW_KEY_RIGHT_SHIFT,

        LeftControl = GLFW_KEY_LEFT_CONTROL,
        RightControl = GLFW_KEY_RIGHT_CONTROL,

        LeftAlt = GLFW_KEY_LEFT_ALT,
        RightAlt = GLFW_KEY_RIGHT_ALT,

        LeftSuper = GLFW_KEY_LEFT_SUPER,
        RightSuper = GLFW_KEY_RIGHT_SUPER,
    };

    struct Window
    {
        GLFWwindow* window = {};
    public:
        static Window* Create();
        static void Destroy(Window* window);

        bool PollEvents();
        bool WaitEvents();

        Vec2 GetCursorPos();
        Vec2U GetClientSize();
    };
}