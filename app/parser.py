from __future__ import annotations

from dataclasses import dataclass
import re

from .config import ParsingSettings

NUMERIC_PATTERN = re.compile(r"^[+-]?(?:\d+(?:\.\d*)?|\.\d+)$")
SPACED_SIGN_PATTERN = re.compile(r"^([+-])\s+(.+)$")
AUTO_SUFFIX_PATTERN = re.compile(r"^([+-]?(?:\d+(?:\.\d*)?|\.\d+))\s*([A-Za-zµμ]+)$")
SINGLE_PLUS_VALUE_PATTERN = re.compile(r"^\+(?:\d|\.)")
SINGLE_MINUS_VALUE_PATTERN = re.compile(r"^-(?:\d|\.)")
UNICODE_MINUS_TRANSLATION = str.maketrans(
    {
        "\u2212": "-",
        "\u2010": "-",
        "\u2011": "-",
        "\u2012": "-",
        "\u2013": "-",
        "\u2014": "-",
        "\ufe63": "-",
        "\uff0d": "-",
    }
)
AUTO_SUFFIX_UNITS = {"g", "mg", "kg", "ug", "µg", "μg", "lb", "lbs", "oz", "ozt", "ct", "gn", "dwt"}
AUTO_SUFFIX_UNITS_CASEFOLD = {unit.casefold() for unit in AUTO_SUFFIX_UNITS}


@dataclass(slots=True)
class ParseResult:
    ok: bool
    raw_text: str
    processed_text: str
    mode: str
    message: str = ""


class ScaleLineParser:
    def process(self, raw_line: str, settings: ParsingSettings) -> ParseResult:
        processed = raw_line
        if settings.trim_whitespace:
            processed = processed.strip()

        if settings.normalize_sign:
            processed = self._normalize_sign(processed, settings.drop_plus_sign, settings.drop_minus_sign)

        if settings.strip_suffix:
            processed = self._strip_suffix(processed, settings.suffix)

        if settings.mode == "raw":
            if processed == "":
                return ParseResult(False, raw_line, processed, settings.mode, "Processed raw line is empty.")
            return ParseResult(True, raw_line, processed, settings.mode)

        if processed == "":
            return ParseResult(False, raw_line, processed, settings.mode, "Parsed value is empty.")

        if not settings.numeric_validation:
            return ParseResult(True, raw_line, processed, settings.mode)

        if not NUMERIC_PATTERN.fullmatch(processed):
            return ParseResult(
                False,
                raw_line,
                processed,
                settings.mode,
                f"Malformed numeric input in parsed mode: {raw_line!r}",
            )

        return ParseResult(True, raw_line, processed, settings.mode)

    def _strip_suffix(self, text: str, configured_suffix: str) -> str:
        candidate = text
        exact_suffix = configured_suffix.strip()
        if exact_suffix:
            if candidate.endswith(exact_suffix):
                candidate = candidate[: -len(exact_suffix)]
                return candidate.rstrip()
            return candidate

        match = AUTO_SUFFIX_PATTERN.fullmatch(candidate)
        if match and match.group(2).casefold() in AUTO_SUFFIX_UNITS_CASEFOLD:
            return match.group(1)
        return candidate

    def _normalize_sign(self, text: str, drop_plus_sign: bool, drop_minus_sign: bool) -> str:
        candidate = text.translate(UNICODE_MINUS_TRANSLATION)
        match = SPACED_SIGN_PATTERN.match(candidate)
        if match:
            candidate = f"{match.group(1)}{match.group(2).lstrip()}"
        if drop_plus_sign and SINGLE_PLUS_VALUE_PATTERN.match(candidate):
            candidate = candidate[1:]
        if drop_minus_sign and SINGLE_MINUS_VALUE_PATTERN.match(candidate):
            candidate = candidate[1:]
        return candidate
