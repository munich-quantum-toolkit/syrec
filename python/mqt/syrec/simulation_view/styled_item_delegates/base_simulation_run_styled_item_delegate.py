# Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
# Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

from __future__ import annotations

from typing import TYPE_CHECKING, Final

from PyQt6 import QtCore, QtGui

if TYPE_CHECKING:
    from PyQt6 import QtWidgets

    from mqt.syrec import NBitValuesContainer

DEFAULT_QREG_NAME_COLUMN_HEADER: Final[str] = "Quantum register"
DEFAULT_QREG_LAYOUT_TEXT_FORMAT: Final[str] = "(First qubit: {first_qubit:d} - Num. qubits: {n_qubits:d})"
DEFAULT_SIMULATION_RUN_CARD_HEADER_FORMAT: Final[str] = "Simulation run #{simulation_run_number:d}"
DEFAULT_INPUT_STATE_QREG_CONTENT_HEADER: Final[str] = "INPUT"
DEFAULT_OUTPUT_STATE_QREG_CONTENT_HEADER: Final[str] = "OUTPUT"
DEFAULT_EXPECTED_OUTPUT_STATE_QREG_CONTENT_PREFIX: Final[str] = "Expected:"
DEFAULT_ACTUAL_OUTPUT_STATE_QREG_CONTENT_PREFIX: Final[str] = "Actual:"
DEFAULT_UNKNOWN_QREG_CONTENT_PLACEHOLDER_TEXT: Final[str] = "<UNKNOWN>"
QREG_CONTENTS_HELP_TEXT: Final[str] = (
    "Qubit values of quantum registers have to be read from left to right with the leftmost character (0 or 1) being equal to the value of the first qubit of the quantum register while the rightmost character displays the value of the last qubit of the quantum register. Unknown quantum register qubit values are replaced with a placeholder text."
)

CARD_TITLE_FONT_SIZE: Final[int] = 14
CARD_CONTENT_FONT_SIZE: Final[int] = 10
QREG_LAYOUT_INFO_FONT_SIZE: Final[int] = 8
CARD_TITLE_BOTTOM_Y_MARGIN: Final[int] = 8
QREG_CONTENT_Y_SPACING: Final[int] = 4
QREG_CONTENT_X_SPACING: Final[int] = 6
CARD_CONTENT_PADDING: Final[int] = 20
QREG_CONTENTS_HELP_TEXT_FONT_SIZE: Final[int] = 8


class BaseSimulationRunStyledItemDelegate:
    @staticmethod
    def _get_pixel_width_for_longest_sim_run_header(
        largest_sim_run_number: int, font_used_to_draw_text: QtGui.QFont, expected_font_size: int
    ) -> int:
        return int(
            QtGui.QFontMetrics(
                QtGui.QFont(font_used_to_draw_text.family(), expected_font_size, font_used_to_draw_text.weight())
            ).horizontalAdvance(
                DEFAULT_SIMULATION_RUN_CARD_HEADER_FORMAT.format(simulation_run_number=largest_sim_run_number)
            )
        )

    @staticmethod
    def _get_pixel_width_of_text(text: str, font_used_to_draw_text: QtGui.QFont, expected_font_size: int) -> int:
        return int(
            QtGui.QFontMetrics(
                QtGui.QFont(font_used_to_draw_text.family(), expected_font_size, font_used_to_draw_text.weight())
            ).horizontalAdvance(text)
        )

    @staticmethod
    def _get_pixel_height_of_text(font_used_to_draw_text: QtGui.QFont, expected_font_size: int) -> int:
        return int(
            QtGui.QFontMetrics(
                QtGui.QFont(font_used_to_draw_text.family(), expected_font_size, font_used_to_draw_text.weight())
            ).height()
        )

    @staticmethod
    def _stringify_some_qubits_of_n_bit_values_container(
        n_bit_values_container: NBitValuesContainer, first_qubit: int, n_qubits: int
    ) -> str:
        last_qubit_of_qreg: Final[int] = first_qubit + (n_qubits - 1)

        if first_qubit <= last_qubit_of_qreg < n_bit_values_container.size():
            return "".join([
                "1" if n_bit_values_container.test(i) else "0" for i in range(first_qubit, last_qubit_of_qreg + 1)
            ])
        return DEFAULT_UNKNOWN_QREG_CONTENT_PLACEHOLDER_TEXT

    @staticmethod
    def _get_estimated_quantum_register_contents_column_width(
        option: QtWidgets.QStyleOptionViewItem,
        largest_quantum_register_size_in_qubits: int,
        font_size: int,
        *,
        does_content_include_prefix: bool = False,
        prefix_and_content_x_spacing: int = QREG_CONTENT_X_SPACING,
    ) -> int:
        prefix_text_width: Final[int] = max(
            BaseSimulationRunStyledItemDelegate._get_pixel_width_of_text(
                DEFAULT_EXPECTED_OUTPUT_STATE_QREG_CONTENT_PREFIX, option.font, font_size
            ),
            BaseSimulationRunStyledItemDelegate._get_pixel_width_of_text(
                DEFAULT_ACTUAL_OUTPUT_STATE_QREG_CONTENT_PREFIX, option.font, font_size
            ),
        )

        text_width_for_largest_qreg: Final[int] = BaseSimulationRunStyledItemDelegate._get_pixel_width_of_text(
            "".join(["0" for i in range(largest_quantum_register_size_in_qubits)]), option.font, font_size
        )
        text_width_for_unknown_qreg_content: Final[int] = BaseSimulationRunStyledItemDelegate._get_pixel_width_of_text(
            "<UNKNOWN>", option.font, font_size
        )
        # We can ignore the text width of the headers of the INPUT and OUTPUT columns since the placeholder text for unknown quantum register contents is larger than both header texts
        return (prefix_text_width + prefix_and_content_x_spacing if does_content_include_prefix else 0) + max(
            text_width_for_largest_qreg, text_width_for_unknown_qreg_content
        )

    @staticmethod
    def _get_column_width_scaled_by_ratio_to_total_available_width(
        required_column_width: int, total_required_width: int, total_available_width: int
    ) -> int:
        return (
            int(float(required_column_width / total_required_width) * total_available_width)
            if total_required_width > 0
            else 0
        )

    @staticmethod
    def _scale_column_widths_based_on_ratio_to_total_available_width(
        required_column_widths: list[int], total_available_width: int
    ) -> list[int]:
        total_required_column_widths: int = sum(required_column_widths)
        return [
            BaseSimulationRunStyledItemDelegate._get_column_width_scaled_by_ratio_to_total_available_width(
                r_col_width, total_required_column_widths, total_available_width
            )
            for r_col_width in required_column_widths
        ]

    @staticmethod
    def _draw_elided_text(
        painter: QtGui.QPainter,
        text: str,
        text_rect: QtCore.QRect,
        font_size: int,
        *,
        draw_bold_text: bool = False,
        text_alignment: QtCore.Qt.AlignmentFlag = QtCore.Qt.AlignmentFlag.AlignTop
        | QtCore.Qt.AlignmentFlag.AlignCenter,
        text_color: QtCore.Qt.GlobalColor = QtCore.Qt.GlobalColor.black,
    ) -> None:
        painter.save()
        bold_font = QtGui.QFont(painter.font().family(), font_size)
        bold_font.setBold(draw_bold_text)
        painter.setFont(bold_font)
        painter.setPen(text_color)

        font_metrics: QtGui.QFontMetrics = painter.fontMetrics()
        available_column_width: int = text_rect.width()
        elided_text: str = font_metrics.elidedText(text, QtCore.Qt.TextElideMode.ElideRight, available_column_width)

        painter.drawText(
            text_rect,
            text_alignment,
            elided_text,
        )
        painter.restore()

    @staticmethod
    def _paint_rect_edge_points(
        painter: QtGui.QPainter,
        rect: QtCore.QRect,
        font_size: int,
        color: QtGui.QColor | QtCore.Qt.GlobalColor,
        simulation_run_number: int,
    ) -> None:
        painter.save()
        custom_pen = QtGui.QPen(color)
        custom_pen.setWidth(font_size)
        painter.setPen(custom_pen)

        painter.drawPoint(QtCore.QPoint(rect.topLeft()))
        painter.drawText(rect.topLeft().x(), rect.topLeft().y(), str(simulation_run_number) + "-TL")
        painter.drawPoint(QtCore.QPoint(rect.topRight()))
        painter.drawText(rect.topRight().x(), rect.topRight().y(), str(simulation_run_number) + "-TR")
        painter.drawPoint(QtCore.QPoint(rect.bottomLeft()))
        painter.drawText(rect.bottomLeft().x(), rect.bottomLeft().y(), str(simulation_run_number) + "-BL")
        painter.drawPoint(QtCore.QPoint(rect.bottomRight()))
        painter.drawText(rect.bottomRight().x(), rect.bottomRight().y(), str(simulation_run_number) + "-BR")
        painter.restore()
