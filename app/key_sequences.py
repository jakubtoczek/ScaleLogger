from __future__ import annotations

from PySide6.QtCore import Qt

CUSTOM_SEQUENCE_ACTION = "custom_sequence"

KEY_TOKEN_TO_VK = {
    "down": 0x28,
    "right": 0x27,
    "left": 0x25,
    "up": 0x26,
    "enter": 0x0D,
    "tab": 0x09,
    "esc": 0x1B,
    "space": 0x20,
    "backspace": 0x08,
    "delete": 0x2E,
    "home": 0x24,
    "end": 0x23,
    "pageup": 0x21,
    "pagedown": 0x22,
}
KEY_TOKEN_TO_VK.update({chr(code).lower(): code for code in range(ord('0'), ord('9') + 1)})
KEY_TOKEN_TO_VK.update({chr(code).lower(): code for code in range(ord('A'), ord('Z') + 1)})
KEY_TOKEN_TO_VK.update({f"f{index}": 0x6F + index for index in range(1, 13)})

KEY_TOKEN_LABELS = {
    "down": "Down",
    "right": "Right",
    "left": "Left",
    "up": "Up",
    "enter": "Enter",
    "tab": "Tab",
    "esc": "Esc",
    "space": "Space",
    "backspace": "Backspace",
    "delete": "Delete",
    "home": "Home",
    "end": "End",
    "pageup": "Page Up",
    "pagedown": "Page Down",
}
KEY_TOKEN_LABELS.update({str(index): str(index) for index in range(10)})
KEY_TOKEN_LABELS.update({chr(code).lower(): chr(code) for code in range(ord('A'), ord('Z') + 1)})
KEY_TOKEN_LABELS.update({f"f{index}": f"F{index}" for index in range(1, 13)})

QT_KEY_TO_TOKEN = {
    Qt.Key.Key_Down: "down",
    Qt.Key.Key_Right: "right",
    Qt.Key.Key_Left: "left",
    Qt.Key.Key_Up: "up",
    Qt.Key.Key_Return: "enter",
    Qt.Key.Key_Enter: "enter",
    Qt.Key.Key_Tab: "tab",
    Qt.Key.Key_Backtab: "tab",
    Qt.Key.Key_Escape: "esc",
    Qt.Key.Key_Space: "space",
    Qt.Key.Key_Backspace: "backspace",
    Qt.Key.Key_Delete: "delete",
    Qt.Key.Key_Home: "home",
    Qt.Key.Key_End: "end",
    Qt.Key.Key_PageUp: "pageup",
    Qt.Key.Key_PageDown: "pagedown",
}
QT_KEY_TO_TOKEN.update({getattr(Qt.Key, f"Key_F{index}"): f"f{index}" for index in range(1, 13)})


def normalize_key_token(value: str) -> str | None:
    candidate = (value or "").strip().lower().replace(" ", "")
    aliases = {
        "escape": "esc",
        "pgup": "pageup",
        "pgdn": "pagedown",
        "del": "delete",
        "return": "enter",
    }
    candidate = aliases.get(candidate, candidate)
    if candidate in KEY_TOKEN_TO_VK:
        return candidate
    return None


def key_token_from_qt_event(key: int, text: str) -> str | None:
    token = QT_KEY_TO_TOKEN.get(Qt.Key(key))
    if token is not None:
        return token
    clean_text = (text or "").strip()
    if len(clean_text) == 1 and clean_text.isascii() and clean_text.isalnum():
        return clean_text.lower()
    return None


def format_key_sequence(tokens: list[str]) -> str:
    if not tokens:
        return "No keys added"
    return " -> ".join(KEY_TOKEN_LABELS.get(token, token) for token in tokens)
