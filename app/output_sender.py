from __future__ import annotations

import ctypes
from dataclasses import dataclass
import platform

from .key_sequences import KEY_TOKEN_TO_VK, normalize_key_token

INPUT_KEYBOARD = 1
KEYEVENTF_KEYUP = 0x0002
KEYEVENTF_UNICODE = 0x0004
WORD = ctypes.c_uint16
DWORD = ctypes.c_uint32
LONG = ctypes.c_int32
UINT = ctypes.c_uint
ULONG_PTR = ctypes.c_size_t


class MOUSEINPUT(ctypes.Structure):
    _fields_ = [
        ("dx", LONG),
        ("dy", LONG),
        ("mouseData", DWORD),
        ("dwFlags", DWORD),
        ("time", DWORD),
        ("dwExtraInfo", ULONG_PTR),
    ]


class KEYBDINPUT(ctypes.Structure):
    _fields_ = [
        ("wVk", WORD),
        ("wScan", WORD),
        ("dwFlags", DWORD),
        ("time", DWORD),
        ("dwExtraInfo", ULONG_PTR),
    ]


class HARDWAREINPUT(ctypes.Structure):
    _fields_ = [
        ("uMsg", DWORD),
        ("wParamL", WORD),
        ("wParamH", WORD),
    ]


class INPUTUNION(ctypes.Union):
    _fields_ = [
        ("mi", MOUSEINPUT),
        ("ki", KEYBDINPUT),
        ("hi", HARDWAREINPUT),
    ]


class INPUT(ctypes.Structure):
    _anonymous_ = ("union",)
    _fields_ = [("type", DWORD), ("union", INPUTUNION)]


LPINPUT = ctypes.POINTER(INPUT)


@dataclass(slots=True)
class SendResult:
    ok: bool
    stage: str = ""
    message: str = ""


class WindowsInputSender:
    def __init__(self) -> None:
        self._enabled = platform.system().lower() == "windows"
        self._send_input = None
        if self._enabled:
            user32 = ctypes.WinDLL("user32", use_last_error=True)
            self._send_input = user32.SendInput
            self._send_input.argtypes = (UINT, LPINPUT, ctypes.c_int)
            self._send_input.restype = UINT

    def send_text_and_action(self, text: str, post_action: str, custom_sequence: list[str] | None = None) -> SendResult:
        if not self._enabled or self._send_input is None:
            return SendResult(False, "text", "Keyboard injection is only available on Windows.")
        if not text:
            return SendResult(False, "text", "Cannot send empty text.")

        try:
            text_inputs = self.build_text_inputs(text)
            if not text_inputs or not self._send_inputs(text_inputs):
                return SendResult(False, "text", f"Failed to inject text {text!r}. {self._last_error_message()}")

            post_action_inputs = self.build_post_action_inputs(post_action, custom_sequence or [])
            if post_action_inputs is None:
                return SendResult(False, "post_action", f"Unsupported post-action key: {post_action}.")
            if post_action_inputs and not self._send_inputs(post_action_inputs):
                return SendResult(False, "post_action", f"Failed to inject after-send action. {self._last_error_message()}")
        except Exception as exc:  # pragma: no cover - defensive around Win32 APIs
            return SendResult(False, "text", f"Keyboard injection failed: {exc}")

        return SendResult(True)

    def build_text_inputs(self, text: str) -> list[INPUT]:
        inputs: list[INPUT] = []
        for code_unit in self._utf16_units(text):
            inputs.append(self._unicode_input(code_unit, key_up=False))
            inputs.append(self._unicode_input(code_unit, key_up=True))
        return inputs

    def build_post_action_inputs(self, post_action: str, custom_sequence: list[str]) -> list[INPUT] | None:
        if post_action == "none":
            return []
        if post_action == "custom_sequence":
            if not custom_sequence:
                return None
            return self.build_custom_sequence_inputs(custom_sequence)
        token = normalize_key_token(post_action)
        if token is None:
            return None
        return self._token_inputs(token)

    def build_custom_sequence_inputs(self, custom_sequence: list[str]) -> list[INPUT] | None:
        inputs: list[INPUT] = []
        for token in custom_sequence:
            normalized = normalize_key_token(token)
            if normalized is None:
                return None
            inputs.extend(self._token_inputs(normalized))
        return inputs

    def _token_inputs(self, token: str) -> list[INPUT]:
        vk = KEY_TOKEN_TO_VK[token]
        return [self._virtual_key_input(vk, False), self._virtual_key_input(vk, True)]

    def _utf16_units(self, text: str) -> list[int]:
        encoded = text.encode("utf-16-le")
        return [int.from_bytes(encoded[index : index + 2], "little") for index in range(0, len(encoded), 2)]

    def _unicode_input(self, code_point: int, key_up: bool) -> INPUT:
        flags = KEYEVENTF_UNICODE | (KEYEVENTF_KEYUP if key_up else 0)
        return INPUT(type=INPUT_KEYBOARD, ki=KEYBDINPUT(0, code_point, flags, 0, 0))

    def _virtual_key_input(self, vk: int, key_up: bool) -> INPUT:
        flags = KEYEVENTF_KEYUP if key_up else 0
        return INPUT(type=INPUT_KEYBOARD, ki=KEYBDINPUT(vk, 0, flags, 0, 0))

    def _send_inputs(self, inputs: list[INPUT]) -> bool:
        if self._send_input is None or not inputs:
            return False
        array_type = INPUT * len(inputs)
        data = array_type(*inputs)
        ctypes.set_last_error(0)
        sent = self._send_input(len(inputs), data, ctypes.sizeof(INPUT))
        return sent == len(inputs)

    def _last_error_message(self) -> str:
        error_code = ctypes.get_last_error()
        if error_code == 0:
            return "Windows did not report an error. Check that the target window is focused and running at the same privilege level."
        return f"Windows error {error_code}: {ctypes.FormatError(error_code).strip()}"
