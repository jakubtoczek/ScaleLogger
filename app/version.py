from __future__ import annotations

APP_NAME = "ScaleLogger"
APP_VERSION = "0.95"
APP_ID = "jakubtoczek.ScaleLogger"
APP_WINDOW_TITLE = f"{APP_NAME} {APP_VERSION}"
APP_REPOSITORY_URL = "github.com/jakubtoczek/ScaleLogger"
APP_LICENSE_NAME = "MIT"
APP_ACKNOWLEDGEMENT = "Development assisted by OpenAI ChatGPT and Codex"
APP_CONFIG_FILENAME = "ScaleLogger.config.json"
APP_PRESETS_DIRNAME = "presets"
APP_LOGS_DIRNAME = "logs"


def about_text() -> str:
    return "\n".join(
        [
            f"{APP_NAME} {APP_VERSION}",
            f"Repository: {APP_REPOSITORY_URL}",
            f"License: {APP_LICENSE_NAME}",
            APP_ACKNOWLEDGEMENT,
        ]
    )
