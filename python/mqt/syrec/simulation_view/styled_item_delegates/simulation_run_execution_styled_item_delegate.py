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
    DEFAULT_QREG_LAYOUT_TEXT_FORMAT,
    DEFAULT_QREG_NAME_COLUMN_HEADER,
    DEFAULT_SIMULATION_RUN_CARD_HEADER_FORMAT,
    DEFAULT_UNKNOWN_QREG_CONTENT_PLACEHOLDER_TEXT,
    QREG_CONTENT_X_SPACING,
    QREG_CONTENT_Y_SPACING,
    BaseSimulationRunStyledItemDelegate,
)

if TYPE_CHECKING:
    from ..simulation_run_model import (
        SimulationRunModel,
    )

AGGREGATE_RESULT_TOP_Y_MARGIN: Final[int] = 15

INPUT_STATE_QREG_LABEL_TEXT: Final[str] = "Input:"
EXPECTED_OUTPUT_QREG_LABEL_TEXT: Final[str] = "Expected output:"
ACTUAL_OUTPUT_QREG_LABEL_TEXT: Final[str] = "Actual output:"
QREG_OUTPUTS_MATCH_LABEL_TEXT: Final[str] = "Result:"
AGGREGATE_QREG_OUTPUTS_MATCH_LABEL_TEXT: Final[str] = "Aggregate result:"
RUNTIME_LABEL_TEXT: Final[str] = "Runtime [in ms]:"

OUTPUTS_MATCH_TEXT: Final[str] = "OUTPUTS MATCH"
OUTPUTS_MISMATCH_TEXT: Final[str] = "OUTPUTS MISMATCH"
OUTPUTS_MATCH_UNKNOWN_TEXT: Final[str] = "UNKNOWN"


# Progress bar delegate C++ example: https://doc.qt.io/qt-6/qtnetwork-torrent-example.html
class SimulationRunExecutionStyledItemDelegate(BaseSimulationRunStyledItemDelegate, QtWidgets.QStyledItemDelegate):  # type: ignore[misc]
    def __init__(self, parent=None):
        super().__init__(parent)

    @staticmethod
    def _get_required_width_for_labels_column(option: QtWidgets.QStyleItemOptionViewItem, font_size: int) -> int:
        return (
            QREG_CONTENT_X_SPACING
            + max(
                SimulationRunExecutionStyledItemDelegate._get_pixel_width_of_text(
                    DEFAULT_QREG_NAME_COLUMN_HEADER, option.font, font_size
                ),
                SimulationRunExecutionStyledItemDelegate._get_pixel_width_of_text(
                    INPUT_STATE_QREG_LABEL_TEXT, option.font, font_size
                ),
                SimulationRunExecutionStyledItemDelegate._get_pixel_width_of_text(
                    EXPECTED_OUTPUT_QREG_LABEL_TEXT, option.font, font_size
                ),
                SimulationRunExecutionStyledItemDelegate._get_pixel_width_of_text(
                    ACTUAL_OUTPUT_QREG_LABEL_TEXT, option.font, font_size
                ),
                SimulationRunExecutionStyledItemDelegate._get_pixel_width_of_text(
                    QREG_OUTPUTS_MATCH_LABEL_TEXT, option.font, font_size
                ),
                SimulationRunExecutionStyledItemDelegate._get_pixel_width_of_text(
                    AGGREGATE_QREG_OUTPUTS_MATCH_LABEL_TEXT, option.font, font_size
                ),
            )
            + QREG_CONTENT_X_SPACING
        )

    @staticmethod
    def _get_required_width_for_qreg_contents_and_outputs_match_result(
        option: QtWidgets.QStyleOptionViewItem, index: QtCore.QModelIndex, font_size: int
    ) -> int:

        largest_quantum_register_size: int = index.data(LARGEST_QUANTUM_REGISTER_SIZE_QT_ROLE)
        largest_first_qubit_of_quantum_registers: int = index.data(LARGEST_FIRST_QUBIT_OF_QUANTUM_REGISTER_QT_ROLE)

        required_width_for_qreg_name_and_layout_info: int = (
            SimulationRunExecutionStyledItemDelegate._get_pixel_width_of_text(
                index.data(LONGEST_QUANTUM_REGISTER_NAME_QT_ROLE), option.font, font_size
            )
            + SimulationRunExecutionStyledItemDelegate._get_pixel_width_of_text(
                DEFAULT_QREG_LAYOUT_TEXT_FORMAT.format(
                    first_qubit=largest_first_qubit_of_quantum_registers, n_qubits=largest_quantum_register_size
                ),
                option.font,
                font_size,
            )
        )
        required_width_for_largest_qreg_contents: int = (
            SimulationRunExecutionStyledItemDelegate._get_estimated_quantum_register_contents_column_width(
                option, largest_quantum_register_size, font_size
            )
        )
        required_width_for_outputs_match_result: int = max(
            SimulationRunExecutionStyledItemDelegate._get_pixel_width_of_text(
                OUTPUTS_MATCH_TEXT, option.font, font_size
            ),
            SimulationRunExecutionStyledItemDelegate._get_pixel_width_of_text(
                OUTPUTS_MISMATCH_TEXT, option.font, font_size
            ),
            SimulationRunExecutionStyledItemDelegate._get_pixel_width_of_text(
                OUTPUTS_MATCH_UNKNOWN_TEXT, option.font, font_size
            ),
        )
        return (
            QREG_CONTENT_X_SPACING
            + max(
                required_width_for_qreg_name_and_layout_info,
                required_width_for_largest_qreg_contents,
                required_width_for_outputs_match_result,
            )
            + QREG_CONTENT_X_SPACING
        )

    @staticmethod
    def _get_required_size_for_content(
        option: QtWidgets.QStyleOptionViewItem, index: QtCore.QModelIndex
    ) -> QtCore.QSize:
        if not index.isValid():
            return QtCore.QSize(0, 0)

        card_title_height: int = SimulationRunExecutionStyledItemDelegate._get_pixel_height_of_text(
            option.font, CARD_TITLE_FONT_SIZE
        )

        card_title_width: int = SimulationRunExecutionStyledItemDelegate._get_pixel_width_for_longest_sim_run_header(
            index.data(LARGEST_SIM_RUN_NUMBER_QT_ROLE), option.font, CARD_TITLE_FONT_SIZE
        )

        n_qregs: int = len(index.data(QUANTUM_REGISTER_LAYOUT_QT_ROLE))
        # Quantum register contents are displayed in the following format for every quantum register:
        # R0: <QREG_NAME_LABEL>:        <QREG_NAME> <QREG_LAYOUT_INFO>
        # R1: <INPUT_LABEL>:            <INPUT_STATE_QREG_VALUES>
        # R2: <EXPECTED_OUTPUT_LABEL>:  <EXPECTED_OUTPUT_STATE_QREG_VALUES>
        # R3: <ACTUAL_OUTPUT_LABEL>:    <ACTUAL_OUTPUT_STATE_QREG_VALUES>
        #
        # Additionally, below the content of all quantum registers the aggregate result of the simulation run is displayed as:
        # R5: <AGGR_RESULT_LABEL>:      <AGGR_RESULT_TEXT>
        # R6: <RUNTIME>:                <RUNTIME_IN_MS>
        required_text_line_height: int = (
            QREG_CONTENT_Y_SPACING
            + SimulationRunExecutionStyledItemDelegate._get_pixel_height_of_text(option.font, CARD_CONTENT_FONT_SIZE)
        )
        required_qreg_contents_height: int = 4 * required_text_line_height
        required_total_qreg_contents_height: int = (n_qregs * required_qreg_contents_height) + (
            (n_qregs - 1) * QREG_CONTENT_Y_SPACING if n_qregs > 1 else 0
        )
        required_aggregate_result_text_height: int = 2 * required_text_line_height
        required_total_card_height: int = (
            CARD_CONTENT_PADDING
            + card_title_height
            + CARD_TITLE_BOTTOM_Y_MARGIN
            + required_total_qreg_contents_height
            + AGGREGATE_RESULT_TOP_Y_MARGIN
            + required_aggregate_result_text_height
            + CARD_CONTENT_PADDING
        )
        required_total_card_width: int = max(
            card_title_width,
            SimulationRunExecutionStyledItemDelegate._get_required_width_for_qreg_contents_and_outputs_match_result(
                option, index, CARD_CONTENT_FONT_SIZE
            ),
        )
        return QtCore.QSize(required_total_card_width, required_total_card_height)

    @staticmethod
    def sizeHint(option: QtWidgets.QStyleOptionViewItem, index: QtCore.QModelIndex) -> QtCore.QSize:  # noqa: N802
        return SimulationRunExecutionStyledItemDelegate._get_required_size_for_content(option, index)

    def paint(self, painter: QtGui.QPainter, option: QtWidgets.QStyleOptionViewItem, index: QtCore.QModelIndex) -> None:
        if not index.isValid() or option.rect.width() == 0:
            return

        n_qregs: int = len(index.data(QUANTUM_REGISTER_LAYOUT_QT_ROLE))
        if n_qregs == 0:
            return

        associated_input_output_mapping: SimulationRunModel = index.data(SIMULATION_RUN_IO_STATE_QT_ROLE)
        SimulationRunExecutionStyledItemDelegate._get_required_size_for_content(option, index)
        available_rect_for_content: QtCore.QRect = option.rect.adjusted(
            CARD_CONTENT_PADDING,
            CARD_CONTENT_PADDING,
            -CARD_CONTENT_PADDING,
            -CARD_CONTENT_PADDING,
        )

        painter.save()
        required_text_width_for_header_for_largest_sim_run_number: Final[int] = min(
            QREG_CONTENT_X_SPACING
            + SimulationRunExecutionStyledItemDelegate._get_pixel_width_for_longest_sim_run_header(
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

        group_box_content_font_size: int = SimulationRunExecutionStyledItemDelegate._get_pixel_height_of_text(
            option.font, CARD_CONTENT_FONT_SIZE
        )
        group_box_content_line_height_with_spacing: Final[int] = group_box_content_font_size + QREG_CONTENT_Y_SPACING

        available_content_width: Final[int] = available_rect_for_content.width()
        required_label_column_width: Final[int] = (
            SimulationRunExecutionStyledItemDelegate._get_required_width_for_labels_column(
                option, group_box_content_font_size
            )
        )
        required_values_column_width: Final[int] = (
            SimulationRunExecutionStyledItemDelegate._get_required_width_for_qreg_contents_and_outputs_match_result(
                option, index, group_box_content_font_size
            )
        )

        scaled_column_widths: list[int] = (
            SimulationRunExecutionStyledItemDelegate._scale_column_widths_based_on_ratio_to_total_available_width(
                [required_label_column_width, required_values_column_width],
                available_content_width,
            )
        )
        scaled_label_column_width: Final[int] = scaled_column_widths[0]
        scaled_values_column_width: Final[int] = scaled_column_widths[1]

        base_label_column_rect: QtCore.QRect = QtCore.QRect(
            header_text_bottom_left_point.x(),
            header_text_bottom_left_point.y() + CARD_TITLE_BOTTOM_Y_MARGIN,
            scaled_label_column_width,
            group_box_content_line_height_with_spacing,
        )
        base_value_column_rect: QtCore.QRect = QtCore.QRect(
            QREG_CONTENT_X_SPACING + base_label_column_rect.topRight().x(),
            base_label_column_rect.topRight().y(),
            scaled_values_column_width,
            group_box_content_line_height_with_spacing,
        )

        longest_qreg_name: str = index.data(LONGEST_QUANTUM_REGISTER_NAME_QT_ROLE)
        largest_qreg_size: int = index.data(LARGEST_QUANTUM_REGISTER_SIZE_QT_ROLE)
        largest_first_qubit_of_qreg: int = index.data(LARGEST_FIRST_QUBIT_OF_QUANTUM_REGISTER_QT_ROLE)

        required_width_for_longest_qreg_name: Final[int] = (
            SimulationRunExecutionStyledItemDelegate._get_pixel_width_of_text(
                longest_qreg_name, option.font, CARD_CONTENT_FONT_SIZE
            )
            + QREG_CONTENT_X_SPACING
        )
        required_width_for_largest_qreg_layout_info_text: Final[int] = (
            SimulationRunExecutionStyledItemDelegate._get_pixel_width_of_text(
                DEFAULT_QREG_LAYOUT_TEXT_FORMAT.format(
                    first_qubit=largest_first_qubit_of_qreg, n_qubits=largest_qreg_size
                ),
                option.font,
                CARD_CONTENT_FONT_SIZE,
            )
        )

        scaled_qreg_name_and_layout_column_widths: list[int] = (
            SimulationRunExecutionStyledItemDelegate._scale_column_widths_based_on_ratio_to_total_available_width(
                [required_width_for_longest_qreg_name, required_width_for_largest_qreg_layout_info_text],
                scaled_values_column_width,
            )
        )
        scaled_width_for_longest_qreg_name: Final[int] = scaled_qreg_name_and_layout_column_widths[0]
        scaled_width_for_largest_qreg_layout_info_text: Final[int] = scaled_qreg_name_and_layout_column_widths[1]

        row_idx: int = 0
        qreg_contents_height_without_spacing: Final[int] = 4 * group_box_content_line_height_with_spacing
        qreg_contents_height: Final[int] = qreg_contents_height_without_spacing + QREG_CONTENT_Y_SPACING
        for qreg_layout in index.data(QUANTUM_REGISTER_LAYOUT_QT_ROLE):
            curr_row_y_offset: int = row_idx * qreg_contents_height
            base_row_i_label_col_rect: QtCore.QRect = base_label_column_rect.adjusted(
                0, curr_row_y_offset, 0, curr_row_y_offset
            )
            base_row_i_value_col_rect: QtCore.QRect = base_value_column_rect.adjusted(
                0, curr_row_y_offset, 0, curr_row_y_offset
            )

            SimulationRunExecutionStyledItemDelegate._draw_elided_text(
                painter,
                DEFAULT_QREG_NAME_COLUMN_HEADER,
                base_row_i_label_col_rect,
                CARD_CONTENT_FONT_SIZE,
                draw_bold_text=True,
                text_alignment=QtCore.Qt.AlignmentFlag.AlignTop | QtCore.Qt.AlignmentFlag.AlignRight,
            )

            qreg_name_rect: QtCore.QRect = QtCore.QRect(
                base_row_i_value_col_rect.topLeft().x(),
                base_row_i_value_col_rect.topLeft().y(),
                scaled_width_for_longest_qreg_name,
                group_box_content_font_size,
            )
            SimulationRunExecutionStyledItemDelegate._draw_elided_text(
                painter,
                qreg_layout.qreg_name,
                qreg_name_rect,
                CARD_CONTENT_FONT_SIZE,
                draw_bold_text=False,
                text_alignment=QtCore.Qt.AlignmentFlag.AlignTop | QtCore.Qt.AlignmentFlag.AlignLeft,
            )

            qreg_layout_info_rect: QtCore.QRect = QtCore.QRect(
                qreg_name_rect.topRight().x(),
                qreg_name_rect.topRight().y(),
                scaled_width_for_largest_qreg_layout_info_text,
                group_box_content_font_size,
            )
            SimulationRunExecutionStyledItemDelegate._draw_elided_text(
                painter,
                DEFAULT_QREG_LAYOUT_TEXT_FORMAT.format(
                    first_qubit=qreg_layout.first_qubit_of_qreg, n_qubits=qreg_layout.qreg_size
                ),
                qreg_layout_info_rect,
                CARD_CONTENT_FONT_SIZE,
                draw_bold_text=False,
                text_alignment=QtCore.Qt.AlignmentFlag.AlignTop | QtCore.Qt.AlignmentFlag.AlignLeft,
                text_color=QtCore.Qt.GlobalColor.gray,
            )

            base_row_i_label_col_rect.adjust(
                0, group_box_content_line_height_with_spacing, 0, group_box_content_line_height_with_spacing
            )
            base_row_i_value_col_rect.adjust(
                0, group_box_content_line_height_with_spacing, 0, group_box_content_line_height_with_spacing
            )
            SimulationRunExecutionStyledItemDelegate._draw_label_and_value(
                painter,
                base_row_i_label_col_rect,
                INPUT_STATE_QREG_LABEL_TEXT,
                base_row_i_value_col_rect,
                SimulationRunExecutionStyledItemDelegate._stringify_some_qubits_of_n_bit_values_container(
                    associated_input_output_mapping.input_state,
                    qreg_layout.first_qubit_of_qreg,
                    qreg_layout.qreg_size,
                ),
            )

            base_row_i_label_col_rect.adjust(
                0, group_box_content_line_height_with_spacing, 0, group_box_content_line_height_with_spacing
            )
            base_row_i_value_col_rect.adjust(
                0, group_box_content_line_height_with_spacing, 0, group_box_content_line_height_with_spacing
            )
            SimulationRunExecutionStyledItemDelegate._draw_label_and_value(
                painter,
                base_row_i_label_col_rect,
                EXPECTED_OUTPUT_QREG_LABEL_TEXT,
                base_row_i_value_col_rect,
                SimulationRunExecutionStyledItemDelegate._stringify_some_qubits_of_n_bit_values_container(
                    associated_input_output_mapping.expected_output_state,
                    qreg_layout.first_qubit_of_qreg,
                    qreg_layout.qreg_size,
                )
                if associated_input_output_mapping.expected_output_state is not None
                else DEFAULT_UNKNOWN_QREG_CONTENT_PLACEHOLDER_TEXT,
                value_col_text_color=QtCore.Qt.GlobalColor.gray
                if associated_input_output_mapping.expected_output_state is None
                else QtCore.Qt.GlobalColor.black,
            )

            base_row_i_label_col_rect.adjust(
                0, group_box_content_line_height_with_spacing, 0, group_box_content_line_height_with_spacing
            )
            base_row_i_value_col_rect.adjust(
                0, group_box_content_line_height_with_spacing, 0, group_box_content_line_height_with_spacing
            )
            SimulationRunExecutionStyledItemDelegate._draw_label_and_value(
                painter,
                base_row_i_label_col_rect,
                ACTUAL_OUTPUT_QREG_LABEL_TEXT,
                base_row_i_value_col_rect,
                SimulationRunExecutionStyledItemDelegate._stringify_some_qubits_of_n_bit_values_container(
                    associated_input_output_mapping.actual_output_state,
                    qreg_layout.first_qubit_of_qreg,
                    qreg_layout.qreg_size,
                )
                if associated_input_output_mapping.actual_output_state is not None
                else DEFAULT_UNKNOWN_QREG_CONTENT_PLACEHOLDER_TEXT,
                value_col_text_color=QtCore.Qt.GlobalColor.gray
                if associated_input_output_mapping.actual_output_state is None
                else QtCore.Qt.GlobalColor.black,
            )
            row_idx += 1

        painter.drawLine(base_row_i_label_col_rect.bottomLeft(), base_row_i_value_col_rect.bottomRight())

        y_offset_from_card_header_to_aggregate_result_row: int = (
            qreg_contents_height_without_spacing + AGGREGATE_RESULT_TOP_Y_MARGIN
        )
        if row_idx > 1:
            y_offset_from_card_header_to_aggregate_result_row += (row_idx - 1) * qreg_contents_height

        aggregate_result_row_outputs_match_label_col_rect: QtCore.QRect = base_label_column_rect.adjusted(
            0, y_offset_from_card_header_to_aggregate_result_row, 0, y_offset_from_card_header_to_aggregate_result_row
        )
        aggregate_result_row_outputs_match_value_col_rect: QtCore.QRect = base_value_column_rect.adjusted(
            0, y_offset_from_card_header_to_aggregate_result_row, 0, y_offset_from_card_header_to_aggregate_result_row
        )
        SimulationRunExecutionStyledItemDelegate._draw_label_and_value(
            painter,
            aggregate_result_row_outputs_match_label_col_rect,
            AGGREGATE_QREG_OUTPUTS_MATCH_LABEL_TEXT,
            aggregate_result_row_outputs_match_value_col_rect,
            SimulationRunExecutionStyledItemDelegate._stringify_outputs_match_result(
                associated_input_output_mapping.do_expected_and_actual_outputs_match
            ),
            value_col_text_color=SimulationRunExecutionStyledItemDelegate._determine_color_for_outputs_match_result_text(
                associated_input_output_mapping.do_expected_and_actual_outputs_match
            ),
        )

        aggregate_result_row_runtime_label_col_rect: QtCore.QRect = (
            aggregate_result_row_outputs_match_label_col_rect.adjusted(
                0, group_box_content_line_height_with_spacing, 0, group_box_content_line_height_with_spacing
            )
        )
        aggregate_result_row_runtime_value_col_rect: QtCore.QRect = (
            aggregate_result_row_outputs_match_value_col_rect.adjusted(
                0, group_box_content_line_height_with_spacing, 0, group_box_content_line_height_with_spacing
            )
        )
        SimulationRunExecutionStyledItemDelegate._draw_label_and_value(
            painter,
            aggregate_result_row_runtime_label_col_rect,
            RUNTIME_LABEL_TEXT,
            aggregate_result_row_runtime_value_col_rect,
            str(associated_input_output_mapping.execution_runtime_in_ms),
            value_col_text_color=QtCore.Qt.GlobalColor.gray
            if associated_input_output_mapping.execution_runtime_in_ms is None
            else QtCore.Qt.GlobalColor.black,
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
            SimulationRunExecutionStyledItemDelegate._paint_rect_edge_points(
                painter, option.rect, 5, QtCore.Qt.GlobalColor.cyan, simulation_run_number
            )
            SimulationRunExecutionStyledItemDelegate._paint_rect_edge_points(
                painter, card_content_rect, 5, QtCore.Qt.GlobalColor.red, simulation_run_number
            )
        painter.drawRoundedRect(option.rect, 3, 3)

        if QtWidgets.QStyle.StateFlag.State_Selected in option.state:
            painter.fillRect(option.rect, option.palette.highlight())
            painter.setBrush(option.palette.highlightedText())

        header_text: str = DEFAULT_SIMULATION_RUN_CARD_HEADER_FORMAT.format(simulation_run_number=simulation_run_number)
        header_text_height: int = SimulationRunExecutionStyledItemDelegate._get_pixel_height_of_text(
            option.font, CARD_TITLE_FONT_SIZE
        )
        header_text_rect = QtCore.QRect(
            card_content_rect.x(), card_content_rect.y(), available_header_width + 10, header_text_height
        )
        SimulationRunExecutionStyledItemDelegate._draw_elided_text(
            painter, header_text, header_text_rect, CARD_TITLE_FONT_SIZE, draw_bold_text=True
        )
        return header_text_rect.bottomLeft()

    @staticmethod
    def _draw_label_and_value(
        painter: QtGui.QPainter,
        label_col_rect: QtCore.QRect,
        label_text: str,
        value_col_rect: QtCore.QRect,
        value_text: str,
        value_col_text_color: QtCore.Qt.GlobalColor = QtCore.Qt.GlobalColor.black,
    ) -> None:
        SimulationRunExecutionStyledItemDelegate._draw_elided_text(
            painter,
            label_text,
            label_col_rect,
            CARD_CONTENT_FONT_SIZE,
            draw_bold_text=True,
            text_alignment=QtCore.Qt.AlignmentFlag.AlignTop | QtCore.Qt.AlignmentFlag.AlignRight,
        )

        SimulationRunExecutionStyledItemDelegate._draw_elided_text(
            painter,
            value_text,
            value_col_rect,
            CARD_CONTENT_FONT_SIZE,
            text_alignment=QtCore.Qt.AlignmentFlag.AlignTop | QtCore.Qt.AlignmentFlag.AlignLeft,
            text_color=value_col_text_color,
        )

        # SimulationRunExecutionStyledItemDelegate._paint_rect_edge_points(
        #     painter, label_col_rect, 5, QtCore.Qt.GlobalColor.red, 0
        # )
        # SimulationRunExecutionStyledItemDelegate._paint_rect_edge_points(
        #     painter, value_col_rect, 5, QtCore.Qt.GlobalColor.blue, 0
        # )

    @staticmethod
    def _determine_color_for_outputs_match_result_text(do_outputs_match: bool | None) -> QtCore.Qt.GlobalColor:
        if do_outputs_match is None:
            return QtCore.Qt.GlobalColor.gray

        return QtCore.Qt.GlobalColor.green if do_outputs_match else QtCore.Qt.GlobalColor.red

    @staticmethod
    def _stringify_outputs_match_result(do_outputs_match: bool | None) -> str:
        if do_outputs_match is None:
            return OUTPUTS_MATCH_UNKNOWN_TEXT

        return OUTPUTS_MATCH_TEXT if do_outputs_match else OUTPUTS_MISMATCH_TEXT
