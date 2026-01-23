# Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
# Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

from __future__ import annotations

from typing import TYPE_CHECKING, Final

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
    DEFAULT_INPUT_STATE_QREG_CONTENT_HEADER,
    DEFAULT_OUTPUT_STATE_QREG_CONTENT_HEADER,
    DEFAULT_QREG_LAYOUT_TEXT_FORMAT,
    DEFAULT_QREG_NAME_COLUMN_HEADER,
    DEFAULT_SIMULATION_RUN_CARD_HEADER_FORMAT,
    DEFAULT_UNKNOWN_QREG_CONTENT_PLACEHOLDER_TEXT,
    QREG_CONTENT_X_SPACING,
    QREG_CONTENT_Y_SPACING,
    QREG_LAYOUT_INFO_FONT_SIZE,
    BaseSimulationRunStyledItemDelegate,
)

if TYPE_CHECKING:
    from ..simulation_run_model import (
        SimulationRunModel,
    )


# Progress bar delegate C++ example: https://doc.qt.io/qt-6/qtnetwork-torrent-example.html
class SimulationRunOverviewStyledItemDelegate(BaseSimulationRunStyledItemDelegate, QtWidgets.QStyledItemDelegate):  # type: ignore[misc]
    def __init__(self, parent=None):
        super().__init__(parent)

    @staticmethod
    def _get_required_qreg_name_and_layout_column_width(
        option: QtWidgets.QStyleOptionViewItem, index: QtCore.QModelIndex, font_size: int
    ) -> int:
        if not index.isValid():
            return 0

        index.data(LONGEST_QUANTUM_REGISTER_NAME_QT_ROLE)
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

        n_qregs: int = len(index.data(QUANTUM_REGISTER_LAYOUT_QT_ROLE))
        # Quantum register contents are displayed as two rows containing the following information:
        # R0: <QREG_NAME> <STRINGIFIED_INPUT_QUBIT_VALUES>  <STRINGIFIED_OUTPUT_QUBIT_VALUES>
        # R1: <QREG_LAYOUT_INFO>
        group_box_title_height: int = SimulationRunOverviewStyledItemDelegate._get_pixel_height_of_text(
            option.font, CARD_TITLE_FONT_SIZE
        )
        group_box_title_width: int = (
            SimulationRunOverviewStyledItemDelegate._get_pixel_width_for_longest_sim_run_header(
                index.data(LARGEST_SIM_RUN_NUMBER_QT_ROLE), option.font, CARD_TITLE_FONT_SIZE
            )
        )

        qreg_contents_text_height: int = (
            QREG_CONTENT_Y_SPACING
            + SimulationRunOverviewStyledItemDelegate._get_pixel_height_of_text(option.font, CARD_CONTENT_FONT_SIZE)
            + QREG_CONTENT_Y_SPACING
            + SimulationRunOverviewStyledItemDelegate._get_pixel_height_of_text(option.font, QREG_LAYOUT_INFO_FONT_SIZE)
        )
        column_header_height: int = SimulationRunOverviewStyledItemDelegate._get_pixel_height_of_text(
            option.font, QREG_LAYOUT_INFO_FONT_SIZE
        )
        total_qreg_contents_text_height: int = n_qregs * qreg_contents_text_height
        total_simulation_run_group_box_height = (
            CARD_CONTENT_PADDING
            + group_box_title_height
            + CARD_TITLE_BOTTOM_Y_MARGIN
            + column_header_height
            + total_qreg_contents_text_height
            + CARD_CONTENT_PADDING
        )

        qreg_name_and_layout_info_column_width: int = (
            SimulationRunOverviewStyledItemDelegate._get_required_qreg_name_and_layout_column_width(
                option, index, CARD_TITLE_FONT_SIZE
            )
        )

        qreg_content_header_width: int = SimulationRunOverviewStyledItemDelegate._get_pixel_width_of_text(
            DEFAULT_INPUT_STATE_QREG_CONTENT_HEADER, option.font, CARD_CONTENT_FONT_SIZE
        )

        max_qreg_qubits_column_width: int = (
            SimulationRunOverviewStyledItemDelegate._get_estimated_quantum_register_contents_column_width(
                option,
                index.data(LARGEST_QUANTUM_REGISTER_SIZE_QT_ROLE),
                CARD_CONTENT_FONT_SIZE,
            )
        )

        max_qreg_content_column_width: int = max(qreg_content_header_width, max_qreg_qubits_column_width)
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
        )
        return QtCore.QSize(total_simulation_run_group_box_width, total_simulation_run_group_box_height)

    @staticmethod
    def sizeHint(option: QtWidgets.QStyleOptionViewItem, index: QtCore.QModelIndex) -> QtCore.QSize:  # noqa: N802
        return SimulationRunOverviewStyledItemDelegate._get_required_size_for_content(option, index)

    def paint(self, painter: QtGui.QPainter, option: QtWidgets.QStyleOptionViewItem, index: QtCore.QModelIndex) -> None:
        if not index.isValid() or option.rect.width() == 0:
            return

        associated_sim_run_model: SimulationRunModel = index.data(SIMULATION_RUN_IO_STATE_QT_ROLE)
        SimulationRunOverviewStyledItemDelegate._get_required_size_for_content(option, index)
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
        header_row_column_one_text_rect = header_column_rects[0]
        header_row_column_two_text_rect = header_column_rects[1]
        header_row_column_three_text_rect = header_column_rects[2]

        row_idx: int = 1
        per_row_y_offset: int = (
            QREG_CONTENT_Y_SPACING
            + SimulationRunOverviewStyledItemDelegate._get_pixel_height_of_text(option.font, CARD_CONTENT_FONT_SIZE)
        )
        for qreg_layout in index.data(QUANTUM_REGISTER_LAYOUT_QT_ROLE):
            curr_row_y_offset: int = row_idx * per_row_y_offset
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
                SimulationRunOverviewStyledItemDelegate._stringify_some_qubits_of_n_bit_values_container(
                    associated_sim_run_model.expected_output_state,
                    qreg_layout.first_qubit_of_qreg,
                    qreg_layout.qreg_size,
                )
                if associated_sim_run_model.expected_output_state is not None
                else DEFAULT_UNKNOWN_QREG_CONTENT_PLACEHOLDER_TEXT,
                header_row_column_three_text_rect.adjusted(0, curr_row_y_offset, 0, curr_row_y_offset),
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
            row_idx += 2
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
        SimulationRunOverviewStyledItemDelegate._get_pixel_width_of_text(header_text, option.font, CARD_TITLE_FONT_SIZE)
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
        output_state_qreg_content_column_width: int = input_state_qreg_content_column_width

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

        header_row_column_one_rect = QtCore.QRect(
            header_text_bottom_left_point.x(),
            header_text_bottom_left_point.y() + 2 * CARD_TITLE_BOTTOM_Y_MARGIN,
            qreg_name_and_layout_info_column_width,
            SimulationRunOverviewStyledItemDelegate._get_pixel_height_of_text(option.font, CARD_CONTENT_FONT_SIZE),
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
            SimulationRunOverviewStyledItemDelegate._get_pixel_height_of_text(option.font, CARD_CONTENT_FONT_SIZE),
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
            SimulationRunOverviewStyledItemDelegate._get_pixel_height_of_text(option.font, CARD_CONTENT_FONT_SIZE),
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
