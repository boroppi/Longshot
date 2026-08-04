// Safety-critical: the only synthetic input this tool ever injects is mouse
// wheel notches and the four enum Key values below (never a click, never
// text). See docs/architecture.md D4 -- a click into a foreign browser
// window could follow a link or submit a form, so this file's surface area
// must not grow beyond what the fsic::Key enum already allows.
#include "win32_backend.h"

#include <windows.h>

namespace fsic::win32 {

Status get_cursor_position(Point* out_px) {
    POINT p{};
    if (!GetCursorPos(&p)) return Status::Error("win32: GetCursorPos failed");
    *out_px = Point{p.x, p.y};
    return Status::Ok();
}

Status move_cursor(Point screen_px) {
    if (!SetCursorPos(screen_px.x, screen_px.y)) return Status::Error("win32: SetCursorPos failed");
    return Status::Ok();
}

Status inject_scroll(Point screen_px, ScrollDir dir, int notches) {
    if (!SetCursorPos(screen_px.x, screen_px.y)) return Status::Error("win32: SetCursorPos failed");
    Sleep(15);

    INPUT input{};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = MOUSEEVENTF_WHEEL;
    // Windows convention: positive = away from user (toward top of content),
    // negative = toward user (toward bottom).
    const int delta = (dir == ScrollDir::Down ? -1 : 1) * notches * WHEEL_DELTA;
    input.mi.mouseData = static_cast<DWORD>(delta);

    if (SendInput(1, &input, sizeof(INPUT)) != 1) {
        return Status::Error("win32: SendInput (wheel) failed");
    }
    return Status::Ok();
}

Status inject_key(uint64_t window_id, Key key) {
    // SendInput targets whatever window currently has focus, not a specific
    // HWND -- the caller (session.cpp) is responsible for having called
    // activate_window first. window_id is accepted for interface symmetry
    // with the other backends but unused here.
    (void)window_id;

    WORD vk;
    switch (key) {
        case Key::PageUp:
            vk = VK_PRIOR;
            break;
        case Key::PageDown:
            vk = VK_NEXT;
            break;
        case Key::Home:
            vk = VK_HOME;
            break;
        case Key::End:
            vk = VK_END;
            break;
        default:
            return Status::Error("win32: inject_key: unknown key");
    }

    INPUT inputs[2]{};
    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wVk = vk;
    inputs[1].type = INPUT_KEYBOARD;
    inputs[1].ki.wVk = vk;
    inputs[1].ki.dwFlags = KEYEVENTF_KEYUP;

    if (SendInput(2, inputs, sizeof(INPUT)) != 2) {
        return Status::Error("win32: SendInput (key) failed");
    }
    return Status::Ok();
}

} // namespace fsic::win32
