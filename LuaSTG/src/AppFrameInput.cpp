#include "AppFrame.h"

#define NOMINMAX
#include <Windows.h>
#include "Mouse.h"
#include "platform/Keyboard.hpp"

namespace LuaSTGPlus
{
    static platform::Keyboard g_Keyboard;
    static platform::Keyboard::State g_KeyboardState;
}

static struct InputEventListener : public Core::Graphics::IWindowEventListener
{
    NativeWindowMessageResult onNativeWindowMessage(void* window, uint32_t message, uintptr_t arg1, intptr_t arg2)
    {
        switch (message)
        {
        case WM_ACTIVATE:
        case WM_ACTIVATEAPP:
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
        case WM_KEYUP:
        case WM_SYSKEYUP:
            LuaSTGPlus::g_Keyboard.ProcessMessage((HWND)window, message, arg1, arg2);
            break;
        }

        switch (message)
        {
        case WM_ACTIVATE:
        case WM_ACTIVATEAPP:
        case WM_INPUT:
        case WM_MOUSEMOVE:
        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP:
        case WM_RBUTTONDOWN:
        case WM_RBUTTONUP:
        case WM_MBUTTONDOWN:
        case WM_MBUTTONUP:
        case WM_MOUSEWHEEL:
        case WM_XBUTTONDOWN:
        case WM_XBUTTONUP:
        case WM_MOUSEHOVER:
            DirectX::Mouse::ProcessMessage(message, arg1, arg2);
            break;
        }

        return {};
    }
} g_InputEventListener;

namespace LuaSTGPlus
{
    static std::unique_ptr<DirectX::Mouse> Mouse;
    static DirectX::Mouse::State MouseState;

    void AppFrame::OpenInput()
    {
        g_Keyboard.Reset();
        Mouse = std::make_unique<DirectX::Mouse>();
        ZeroMemory(&MouseState, sizeof(MouseState));
        m_pAppModel->getWindow()->addEventListener(&g_InputEventListener);
        Mouse->SetWindow((HWND)m_pAppModel->getWindow()->getNativeHandle());
    }
    void AppFrame::CloseInput()
    {
        m_pAppModel->getWindow()->removeEventListener(&g_InputEventListener);
        Mouse = nullptr;
    }
    void AppFrame::UpdateInput()
    {
        g_Keyboard.GetState(g_KeyboardState, true);
        if (Mouse)
        {
            MouseState = Mouse->GetState();
            Mouse->ResetScrollWheelValue();
        }
        else
        {
            ZeroMemory(&MouseState, sizeof(MouseState));
        }
    }

    bool AppFrame::WantSwitchFullScreenMode()
    {
        return
            (g_KeyboardState.IsKeyDown(platform::Keyboard::Key::LeftControl) || g_KeyboardState.IsKeyDown(platform::Keyboard::Key::RightControl))
            && g_KeyboardState.IsKeyDown(platform::Keyboard::Key::Enter, true)
            ;
    }

    fBool AppFrame::GetKeyState(int VKCode)LNOEXCEPT
    {
        return g_KeyboardState.IsKeyDown((platform::Keyboard::Key)VKCode);
    }
    
    int AppFrame::GetLastKey()LNOEXCEPT
    {
        return (int)g_KeyboardState.LastKeyDown;
    }
    
    fBool AppFrame::GetMouseState_legacy(int button)LNOEXCEPT
    {
        switch (button)
        {
        case 0:
            return MouseState.leftButton;
        case 1:
            return MouseState.middleButton;
        case 2:
            return MouseState.rightButton;
        case 3:
            return MouseState.xButton1;
        case 4:
            return MouseState.xButton2;
        default:
            return false;
        }
    }
    fBool AppFrame::GetMouseState(int button)LNOEXCEPT
    {
        switch (button)
        {
        case VK_LBUTTON:
            return MouseState.leftButton;
        case VK_MBUTTON:
            return MouseState.middleButton;
        case VK_RBUTTON:
            return MouseState.rightButton;
        case VK_XBUTTON1:
            return MouseState.xButton1;
        case VK_XBUTTON2:
            return MouseState.xButton2;
        default:
            return false;
        }
    }
    fcyVec2 AppFrame::GetMousePosition(bool no_flip)LNOEXCEPT
    {
        if (no_flip)
        {
            return fcyVec2((fFloat)MouseState.x, (fFloat)MouseState.y);
        }
        else
        {
            return fcyVec2((fFloat)MouseState.x, m_OptionResolution.y - (fFloat)MouseState.y);
        }
    }
    fInt AppFrame::GetMouseWheelDelta()LNOEXCEPT
    {
        return MouseState.scrollWheelValue;
    }
};
