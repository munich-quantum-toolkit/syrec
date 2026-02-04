# Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
# Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

from __future__ import annotations

import sys
from typing import TYPE_CHECKING, Final

if sys.version_info >= (3, 12):
    from typing import override
else:
    from typing_extensions import override

from PyQt6 import QtCore, QtGui, QtWidgets

from ..simulation_run_model import (
    LARGEST_FIRST_QUBIT_OF_QUANTUM_REGISTER_QT_ROLE,
    LARGEST_QUANTUM_REGISTER_SIZE_QT_ROLE,
    LARGEST_SIM_RUN_NUMBER_QT_ROLE,
    LONGEST_QUANTUM_REGISTER_NAME_QT_ROLE,
    QUANTUM_REGISTER_LAYOUT_QT_ROLE,
    SIMULATION_RUN_IO_STATE_QT_ROLE,
)
from .base_simulation_run_styled_item_delegate import (
    CARD_CONTENT_FONT_SIZE,
    CARD_CONTENT_PADDING,
    CARD_TITLE_BOTTOM_Y_MARGIN,
    CARD_TITLE_FONT_SIZE,
    DEFAULT_ACTUAL_OUTPUT_STATE_QREG_CONTENT_PREFIX,
    DEFAULT_EXPECTED_OUTPUT_STATE_QREG_CONTENT_PREFIX,
    DEFAULT_INPUT_STATE_QREG_CONTENT_HEADER,
    DEFAULT_OUTPUT_STATE_QREG_CONTENT_HEADER,
    DEFAULT_QREG_LAYOUT_TEXT_FORMAT,
    DEFAULT_QREG_NAME_COLUMN_HEADER,
    DEFAULT_SIMULATION_RUN_CARD_HEADER_FORMAT,
    DEFAULT_UNKNOWN_QREG_CONTENT_PLACEHOLDER_TEXT,
    QREG_CONTENT_X_SPACING,
    QREG_CONTENT_Y_SPACING,
    QREG_CONTENTS_HELP_TEXT,
    QREG_CONTENTS_HELP_TEXT_FONT_SIZE,
    QREG_LAYOUT_INFO_FONT_SIZE,
    BaseSimulationRunStyledItemDelegate,
)

if TYPE_CHECKING:
    from mqt.syrec import NBitValuesContainer

    from ..simulation_run_model import (
        SimulationRunModel,
    )


# Progress bar delegate C++ example: https://doc.qt.io/qt-6/qtnetwork-torrent-example.html
class SimulationRunOverviewStyledItemDelegate(BaseSimulationRunStyledItemDelegate, QtWidgets.QStyledItemDelegate):  # type: ignore[misc]
    def __init__(self, parent: QtWidgets.QWidget = None) -> None:
        super().__init__(parent)

    @staticmethod
    def _get_required_qreg_name_and_layout_column_width(
        option: QtWidgets.QStyleOptionViewItem, index: QtCore.QModelIndex, font_size: int
    ) -> int:
        if not index.isValid():
            return 0

        largest_quantum_register_size: int = index.data(LARGEST_QUANTUM_REGISTER_SIZE_QT_ROLE)
        largest_first_qubit_of_quantum_registers: int = index.data(LARGEST_FIRST_QUBIT_OF_QUANTUM_REGISTER_QT_ROLE)

        return (2 * QREG_CONTENT_X_SPACING) + max(
            SimulationRunOverviewStyledItemDelegate._get_pixel_width_of_text(
                DEFAULT_QREG_NAME_COLUMN_HEADER, option.font, font_size
            ),
            SimulationRunOverviewStyledItemDelegate._get_pixel_width_of_text(
                index.data(LONGEST_QUANTUM_REGISTER_NAME_QT_ROLE), option.font, font_size
            ),
            SimulationRunOverviewStyledItemDelegate._get_pixel_width_of_text(
                DEFAULT_QREG_LAYOUT_TEXT_FORMAT.format(
                    first_qubit=largest_first_qubit_of_quantum_registers, n_qubits=largest_quantum_register_size
                ),
                option.font,
                font_size,
            ),
        )

    @staticmethod
    def _get_required_size_for_content(
        option: QtWidgets.QStyleOptionViewItem, index: QtCore.QModelIndex
    ) -> QtCore.QSize:
        if not index.isValid():
            return QtCore.QSize(0, 0)

        n_qregs: Final[int] = len(index.data(QUANTUM_REGISTER_LAYOUT_QT_ROLE))
        # Quantum register contents are displayed as two rows containing the following information:
        # R0: <QREG_NAME> <STRINGIFIED_INPUT_QUBIT_VALUES>  <STRINGIFIED_OUTPUT_QUBIT_VALUES>
        # R1: <QREG_LAYOUT_INFO>
        group_box_title_height: Final[int] = SimulationRunOverviewStyledItemDelegate._get_pixel_height_of_text(
            option.font, CARD_TITLE_FONT_SIZE
        )
        group_box_title_width: Final[int] = (
            SimulationRunOverviewStyledItemDelegate._get_pixel_width_for_longest_sim_run_header(
                index.data(LARGEST_SIM_RUN_NUMBER_QT_ROLE), option.font, CARD_TITLE_FONT_SIZE
            )
        )

        qreg_contents_text_height: Final[int] = (
            QREG_CONTENT_Y_SPACING
            + SimulationRunOverviewStyledItemDelegate._get_pixel_height_of_text(option.font, CARD_CONTENT_FONT_SIZE)
            + QREG_CONTENT_Y_SPACING
            + SimulationRunOverviewStyledItemDelegate._get_pixel_height_of_text(option.font, QREG_LAYOUT_INFO_FONT_SIZE)
            + QREG_CONTENT_Y_SPACING
        )
        column_header_height: int = SimulationRunOverviewStyledItemDelegate._get_pixel_height_of_text(
            option.font, CARD_CONTENT_FONT_SIZE
        )
        total_qreg_contents_text_height: Final[int] = n_qregs * qreg_contents_text_height
        total_simulation_run_group_box_height = (
            CARD_CONTENT_PADDING
            + group_box_title_height
            + CARD_TITLE_BOTTOM_Y_MARGIN
            + column_header_height
            + QREG_CONTENT_Y_SPACING
            + total_qreg_contents_text_height
            + QREG_CONTENT_Y_SPACING
            + SimulationRunOverviewStyledItemDelegate._get_pixel_height_of_text(option.font, CARD_CONTENT_FONT_SIZE)
            + QREG_CONTENT_Y_SPACING
            + CARD_CONTENT_PADDING
        )

        qreg_name_and_layout_info_column_width: Final[int] = (
            SimulationRunOverviewStyledItemDelegate._get_required_qreg_name_and_layout_column_width(
                option, index, CARD_CONTENT_FONT_SIZE
            )
        )

        qreg_content_header_width: Final[int] = SimulationRunOverviewStyledItemDelegate._get_pixel_width_of_text(
            DEFAULT_INPUT_STATE_QREG_CONTENT_HEADER, option.font, CARD_CONTENT_FONT_SIZE
        )

        max_qreg_qubits_column_width: Final[int] = (
            SimulationRunOverviewStyledItemDelegate._get_estimated_quantum_register_contents_column_width(
                option,
                index.data(LARGEST_QUANTUM_REGISTER_SIZE_QT_ROLE),
                CARD_CONTENT_FONT_SIZE,
                does_content_include_prefix=True,
            )
        )

        required_help_text_width: Final[int] = SimulationRunOverviewStyledItemDelegate._get_pixel_width_of_text(
            QREG_CONTENTS_HELP_TEXT, option.font, QREG_CONTENTS_HELP_TEXT_FONT_SIZE
        )
        max_qreg_content_column_width: Final[int] = max(qreg_content_header_width, max_qreg_qubits_column_width)
        total_simulation_run_group_box_width = max(
            group_box_title_width,
            (
                CARD_CONTENT_PADDING
                + qreg_name_and_layout_info_column_width
                + QREG_CONTENT_X_SPACING
                + max_qreg_content_column_width
                + QREG_CONTENT_X_SPACING
                + QREG_CONTENT_X_SPACING
                + max_qreg_content_column_width
                + QREG_CONTENT_X_SPACING
                + CARD_CONTENT_PADDING
            ),
            required_help_text_width,
        )
        return QtCore.QSize(total_simulation_run_group_box_width, total_simulation_run_group_box_height)

    @override
    def sizeHint(self, option: QtWidgets.QStyleOptionViewItem, index: QtCore.QModelIndex) -> QtCore.QSize:
        required_content_size: Final[QtCore.QSize] = (
            SimulationRunOverviewStyledItemDelegate._get_required_size_for_content(option, index)
        )
        available_content_rect: Final[QtCore.QRect] = option.rect
        return QtCore.QSize(
            min(required_content_size.width(), available_content_rect.width()), required_content_size.height()
        )

    @override
    def paint(self, painter: QtGui.QPainter, option: QtWidgets.QStyleOptionViewItem, index: QtCore.QModelIndex) -> None:
        if not index.isValid() or option.rect.width() == 0:
            return

        associated_sim_run_model: SimulationRunModel = index.data(SIMULATION_RUN_IO_STATE_QT_ROLE)
        largest_qreg_size: Final[int] = index.data(LARGEST_QUANTUM_REGISTER_SIZE_QT_ROLE)

        available_rect_for_content: QtCore.QRect = option.rect.adjusted(
            CARD_CONTENT_PADDING,
            CARD_CONTENT_PADDING,
            -CARD_CONTENT_PADDING,
            -CARD_CONTENT_PADDING,
        )

        painter.save()
        required_text_width_for_header_for_largest_sim_run_number: Final[int] = min(
            QREG_CONTENT_X_SPACING
            + SimulationRunOverviewStyledItemDelegate._get_pixel_width_for_longest_sim_run_header(
                index.data(LARGEST_SIM_RUN_NUMBER_QT_ROLE), option.font, CARD_TITLE_FONT_SIZE
            )
            + QREG_CONTENT_X_SPACING,
            available_rect_for_content.width(),
        )

        header_text_bottom_left_point: QtCore.QPoint = self._draw_card_border_and_header(
            painter,
            option,
            simulation_run_number=index.row(),
            card_content_rect=available_rect_for_content,
            available_header_width=required_text_width_for_header_for_largest_sim_run_number,
        )

        header_column_rects: list[QtCore.QRect] = self._draw_and_determine_column_headers(
            painter, option, index, header_text_bottom_left_point, available_rect_for_content.width()
        )

        header_text_height: Final[int] = SimulationRunOverviewStyledItemDelegate._get_pixel_height_of_text(
            option.font, CARD_CONTENT_FONT_SIZE
        )
        header_row_column_one_text_rect = header_column_rects[0].adjusted(0, header_text_height, 0, header_text_height)
        header_row_column_two_text_rect = header_column_rects[1].adjusted(0, header_text_height, 0, header_text_height)
        header_row_column_three_text_rect = header_column_rects[2].adjusted(
            0, header_text_height, 0, header_text_height
        )

        per_row_y_offset: Final[int] = (
            SimulationRunOverviewStyledItemDelegate._get_pixel_height_of_text(option.font, CARD_CONTENT_FONT_SIZE)
            + QREG_CONTENT_Y_SPACING
        )
        per_qreg_contents_y_offset: Final[int] = (2 * per_row_y_offset) + QREG_CONTENT_Y_SPACING
        for qreg_idx, qreg_layout in enumerate(index.data(QUANTUM_REGISTER_LAYOUT_QT_ROLE)):
            curr_row_y_offset: int = qreg_idx * per_qreg_contents_y_offset
            SimulationRunOverviewStyledItemDelegate._draw_elided_text(
                painter,
                qreg_layout.qreg_name,
                header_row_column_one_text_rect.adjusted(0, curr_row_y_offset, 0, curr_row_y_offset),
                CARD_CONTENT_FONT_SIZE,
            )

            SimulationRunOverviewStyledItemDelegate._draw_elided_text(
                painter,
                SimulationRunOverviewStyledItemDelegate._stringify_some_qubits_of_n_bit_values_container(
                    associated_sim_run_model.input_state,
                    qreg_layout.first_qubit_of_qreg,
                    qreg_layout.qreg_size,
                ),
                header_row_column_two_text_rect.adjusted(0, curr_row_y_offset, 0, curr_row_y_offset),
                CARD_CONTENT_FONT_SIZE,
            )

            SimulationRunOverviewStyledItemDelegate._draw_elided_text(
                painter,
                DEFAULT_QREG_LAYOUT_TEXT_FORMAT.format(
                    first_qubit=qreg_layout.first_qubit_of_qreg, n_qubits=qreg_layout.qreg_size
                ),
                header_row_column_one_text_rect.adjusted(
                    0, curr_row_y_offset + per_row_y_offset, 0, curr_row_y_offset + per_row_y_offset
                ),
                font_size=QREG_LAYOUT_INFO_FONT_SIZE,
                text_color=QtCore.Qt.GlobalColor.gray,
            )

            SimulationRunOverviewStyledItemDelegate._draw_qreg_contents(
                painter,
                option,
                DEFAULT_EXPECTED_OUTPUT_STATE_QREG_CONTENT_PREFIX,
                associated_sim_run_model.expected_output_state,
                first_qubit_of_qreg=qreg_layout.first_qubit_of_qreg,
                qreg_size=qreg_layout.qreg_size,
                largest_qreg_size=largest_qreg_size,
                available_content_rect=header_row_column_three_text_rect.adjusted(
                    0, curr_row_y_offset, 0, curr_row_y_offset
                ),
                font_size=CARD_CONTENT_FONT_SIZE,
            )

            SimulationRunOverviewStyledItemDelegate._draw_qreg_contents(
                painter,
                option,
                DEFAULT_ACTUAL_OUTPUT_STATE_QREG_CONTENT_PREFIX,
                associated_sim_run_model.actual_output_state,
                first_qubit_of_qreg=qreg_layout.first_qubit_of_qreg,
                qreg_size=qreg_layout.qreg_size,
                largest_qreg_size=largest_qreg_size,
                available_content_rect=header_row_column_three_text_rect.adjusted(
                    0, curr_row_y_offset + per_row_y_offset, 0, curr_row_y_offset + per_row_y_offset
                ),
                font_size=CARD_CONTENT_FONT_SIZE,
            )

        n_qregs: Final[int] = len(index.data(QUANTUM_REGISTER_LAYOUT_QT_ROLE))
        y_offset_to_help_text: Final[int] = (n_qregs * per_qreg_contents_y_offset) + QREG_CONTENT_Y_SPACING

        help_text_content_rect: Final[QtCore.QRect] = QtCore.QRect(
            header_row_column_one_text_rect.topLeft().x(),
            header_row_column_one_text_rect.topLeft().y() + y_offset_to_help_text,
            available_rect_for_content.width(),
            SimulationRunOverviewStyledItemDelegate._get_pixel_height_of_text(
                option.font, QREG_CONTENTS_HELP_TEXT_FONT_SIZE
            ),
        )
        SimulationRunOverviewStyledItemDelegate._draw_elided_text(
            painter,
            QREG_CONTENTS_HELP_TEXT,
            help_text_content_rect,
            font_size=QREG_CONTENTS_HELP_TEXT_FONT_SIZE,
            text_color=QtCore.Qt.GlobalColor.gray,
        )
        painter.restore()

    @staticmethod
    def _draw_card_border_and_header(
        painter: QtGui.QPainter,
        option: QtWidgets.QStyleOptionViewItem,
        simulation_run_number: int,
        card_content_rect: QtCore.QRect,
        available_header_width: int,
        draw_rect_corners: bool = False,
    ) -> QtCore.QPoint:
        painter.setRenderHint(QtGui.QPainter.RenderHint.Antialiasing)
        if draw_rect_corners:
            SimulationRunOverviewStyledItemDelegate._paint_rect_edge_points(
                painter, option.rect, 5, QtCore.Qt.GlobalColor.cyan, simulation_run_number
            )
            SimulationRunOverviewStyledItemDelegate._paint_rect_edge_points(
                painter, card_content_rect, 5, QtCore.Qt.GlobalColor.red, simulation_run_number
            )
        painter.drawRoundedRect(option.rect, 3, 3)

        if QtWidgets.QStyle.StateFlag.State_Selected in option.state:
            painter.fillRect(option.rect, option.palette.highlight())
            painter.setBrush(option.palette.highlightedText())

        header_text: str = DEFAULT_SIMULATION_RUN_CARD_HEADER_FORMAT.format(simulation_run_number=simulation_run_number)
        header_text_height: int = SimulationRunOverviewStyledItemDelegate._get_pixel_height_of_text(
            option.font, CARD_TITLE_FONT_SIZE
        )
        header_text_rect = QtCore.QRect(
            card_content_rect.x(), card_content_rect.y(), available_header_width + 10, header_text_height
        )
        SimulationRunOverviewStyledItemDelegate._draw_elided_text(
            painter, header_text, header_text_rect, CARD_TITLE_FONT_SIZE, draw_bold_text=True
        )
        return header_text_rect.bottomLeft()

    def _draw_and_determine_column_headers(
        self,
        painter: QtGui.QPainter,
        option: QtWidgets.QStyleOptionViewItem,
        index: QtCore.QModelIndex,
        header_text_bottom_left_point: QtCore.QPoint,
        available_content_width: int,
        draw_rect_corners: bool = False,
    ) -> list[QtCore.QRect]:
        qreg_name_and_layout_info_column_width: int = (
            self._get_required_qreg_name_and_layout_column_width(option, index, CARD_CONTENT_FONT_SIZE)
            + QREG_CONTENT_X_SPACING
        )
        input_state_qreg_content_column_width: int = (
            QREG_CONTENT_X_SPACING
            + SimulationRunOverviewStyledItemDelegate._get_estimated_quantum_register_contents_column_width(
                option,
                index.data(LARGEST_QUANTUM_REGISTER_SIZE_QT_ROLE),
                CARD_CONTENT_FONT_SIZE,
            )
            + QREG_CONTENT_X_SPACING
        )
        output_state_qreg_content_column_width: int = (
            QREG_CONTENT_X_SPACING
            + SimulationRunOverviewStyledItemDelegate._get_estimated_quantum_register_contents_column_width(
                option,
                index.data(LARGEST_QUANTUM_REGISTER_SIZE_QT_ROLE),
                CARD_CONTENT_FONT_SIZE,
                does_content_include_prefix=True,
            )
            + QREG_CONTENT_X_SPACING
        )

        scaled_column_widths: list[int] = (
            SimulationRunOverviewStyledItemDelegate._scale_column_widths_based_on_ratio_to_total_available_width(
                [
                    qreg_name_and_layout_info_column_width,
                    input_state_qreg_content_column_width,
                    output_state_qreg_content_column_width,
                ],
                available_content_width,
            )
        )
        qreg_name_and_layout_info_column_width = scaled_column_widths[0]
        input_state_qreg_content_column_width = scaled_column_widths[1]
        output_state_qreg_content_column_width = scaled_column_widths[2]
        header_text_height: Final[int] = SimulationRunOverviewStyledItemDelegate._get_pixel_height_of_text(
            option.font, CARD_CONTENT_FONT_SIZE
        )

        header_row_column_one_rect = QtCore.QRect(
            header_text_bottom_left_point.x(),
            header_text_bottom_left_point.y() + CARD_TITLE_BOTTOM_Y_MARGIN,
            qreg_name_and_layout_info_column_width,
            header_text_height,
        )
        header_row_column_one_text_rect: QtCore.QRect = header_row_column_one_rect.adjusted(
            QREG_CONTENT_X_SPACING, 0, -QREG_CONTENT_X_SPACING, 0
        )
        SimulationRunOverviewStyledItemDelegate._draw_elided_text(
            painter,
            DEFAULT_QREG_NAME_COLUMN_HEADER,
            header_row_column_one_text_rect,
            CARD_CONTENT_FONT_SIZE,
            draw_bold_text=True,
        )

        header_row_column_two_rect = QtCore.QRect(
            header_row_column_one_rect.topRight().x(),
            header_row_column_one_rect.topRight().y(),
            input_state_qreg_content_column_width,
            header_text_height,
        )
        header_row_column_two_text_rect: QtCore.QRect = header_row_column_two_rect.adjusted(
            QREG_CONTENT_X_SPACING, 0, -QREG_CONTENT_X_SPACING, 0
        )
        SimulationRunOverviewStyledItemDelegate._draw_elided_text(
            painter,
            DEFAULT_INPUT_STATE_QREG_CONTENT_HEADER,
            header_row_column_two_text_rect,
            CARD_CONTENT_FONT_SIZE,
            draw_bold_text=True,
        )

        header_row_column_three_rect = QtCore.QRect(
            header_row_column_two_rect.topRight().x(),
            header_row_column_two_rect.topRight().y(),
            output_state_qreg_content_column_width,
            header_text_height,
        )
        header_row_column_three_text_rect: QtCore.QRect = header_row_column_three_rect.adjusted(
            QREG_CONTENT_X_SPACING, 0, -QREG_CONTENT_X_SPACING, 0
        )
        SimulationRunOverviewStyledItemDelegate._draw_elided_text(
            painter,
            DEFAULT_OUTPUT_STATE_QREG_CONTENT_HEADER,
            header_row_column_three_text_rect,
            CARD_CONTENT_FONT_SIZE,
            draw_bold_text=True,
        )

        if draw_rect_corners:
            SimulationRunOverviewStyledItemDelegate._paint_rect_edge_points(
                painter, header_row_column_one_rect, 5, QtCore.Qt.GlobalColor.red, index.row()
            )
            SimulationRunOverviewStyledItemDelegate._paint_rect_edge_points(
                painter, header_row_column_two_rect, 5, QtCore.Qt.GlobalColor.blue, index.row()
            )
            SimulationRunOverviewStyledItemDelegate._paint_rect_edge_points(
                painter, header_row_column_three_rect, 5, QtCore.Qt.GlobalColor.green, index.row()
            )
        return [header_row_column_one_text_rect, header_row_column_two_text_rect, header_row_column_three_text_rect]

    @staticmethod
    def _draw_qreg_contents(
        painter: QtGui.QPainter,
        option: QtWidgets.QStyleOptionViewItem,
        qreg_contents_prefix: str,
        qreg_contents_container: NBitValuesContainer | None,
        first_qubit_of_qreg: int,
        qreg_size: int,
        largest_qreg_size: int,
        available_content_rect: QtCore.QRect,
        font_size: int,
    ) -> None:
        # If we do not even have enough available space to print whitespace between the prefix label and the qreg contents then simply draw nothing
        if available_content_rect.width() <= QREG_CONTENT_X_SPACING:
            return

        qreg_contents_text_height: Final[int] = SimulationRunOverviewStyledItemDelegate._get_pixel_height_of_text(
            option.font, font_size
        )
        stringified_qreg_contents: Final[str] = (
            SimulationRunOverviewStyledItemDelegate._stringify_some_qubits_of_n_bit_values_container(
                qreg_contents_container,
                first_qubit_of_qreg,
                qreg_size,
            )
            if qreg_contents_container is not None
            else DEFAULT_UNKNOWN_QREG_CONTENT_PLACEHOLDER_TEXT
        )

        required_qreg_prefix_width: Final[int] = max(
            SimulationRunOverviewStyledItemDelegate._get_pixel_width_of_text(
                DEFAULT_EXPECTED_OUTPUT_STATE_QREG_CONTENT_PREFIX, option.font, font_size
            ),
            SimulationRunOverviewStyledItemDelegate._get_pixel_width_of_text(
                DEFAULT_ACTUAL_OUTPUT_STATE_QREG_CONTENT_PREFIX, option.font, font_size
            ),
        )
        required_qreg_contents_width: Final[int] = (
            SimulationRunOverviewStyledItemDelegate._get_estimated_quantum_register_contents_column_width(
                option, largest_qreg_size, font_size
            )
        )
        total_qreg_contents_width: Final[int] = (
            required_qreg_prefix_width + QREG_CONTENT_X_SPACING + required_qreg_contents_width
        )

        actual_qreg_prefix_width: int = 0
        actual_qreg_contents_width: int = 0
        qreg_contents_text_start_pos: QtCore.QPoint = QtCore.QPoint(0, 0)

        if total_qreg_contents_width >= available_content_rect.width():
            truncated_ratio_based_content_widths: Final[list[int]] = (
                SimulationRunOverviewStyledItemDelegate._scale_column_widths_based_on_ratio_to_total_available_width(
                    [required_qreg_prefix_width, required_qreg_contents_width],
                    available_content_rect.width() - QREG_CONTENT_X_SPACING,
                )
            )
            actual_qreg_prefix_width = truncated_ratio_based_content_widths[0]
            actual_qreg_contents_width = truncated_ratio_based_content_widths[1]

            qreg_contents_text_start_pos = available_content_rect.topLeft()
        else:
            actual_qreg_prefix_width = min(available_content_rect.width(), required_qreg_prefix_width)
            actual_qreg_contents_width = min(
                available_content_rect.width() - actual_qreg_prefix_width, required_qreg_contents_width
            )

            # If the content can 'easily' fit in the available rectangle then we can center out content inside of said rectangle
            qreg_contents_text_start_pos_offset: Final[int] = (available_content_rect.width() // 2) - (
                total_qreg_contents_width // 2
            )
            qreg_contents_text_start_pos = QtCore.QPoint(
                available_content_rect.topLeft().x() + qreg_contents_text_start_pos_offset,
                available_content_rect.topLeft().y(),
            )

        qreg_prefix_text_rect = QtCore.QRect(
            qreg_contents_text_start_pos.x(),
            qreg_contents_text_start_pos.y(),
            actual_qreg_prefix_width,
            qreg_contents_text_height,
        )
        SimulationRunOverviewStyledItemDelegate._draw_elided_text(
            painter,
            qreg_contents_prefix,
            qreg_prefix_text_rect,
            font_size,
            text_alignment=QtCore.Qt.AlignmentFlag.AlignRight,
            text_color=QtCore.Qt.GlobalColor.gray,
        )

        qreg_contents_text_rect = QtCore.QRect(
            qreg_prefix_text_rect.topRight().x() + QREG_CONTENT_X_SPACING,
            qreg_prefix_text_rect.topRight().y(),
            actual_qreg_contents_width,
            qreg_contents_text_height,
        )
        SimulationRunOverviewStyledItemDelegate._draw_elided_text(
            painter,
            stringified_qreg_contents,
            qreg_contents_text_rect,
            font_size,
            text_alignment=QtCore.Qt.AlignmentFlag.AlignLeft,
        )
