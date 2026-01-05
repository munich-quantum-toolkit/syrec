# Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
# Copyright (c) 2025 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

from __future__ import annotations

from typing import TYPE_CHECKING, Final

from PyQt6 import QtCore, QtGui, QtWidgets

from .qt_simulation_run_model import (
    LARGEST_FIRST_QUBIT_OF_QUANTUM_REGISTER_QT_ROLE,
    LARGEST_QUANTUM_REGISTER_SIZE_QT_ROLE,
    LONGEST_QUANTUM_REGISTER_NAME_QT_ROLE,
    QUANTUM_REGISTER_LAYOUT_QT_ROLE,
    SIMULATION_RUN_IO_STATE_QT_ROLE,
)

if TYPE_CHECKING:
    from mqt import syrec

    from .qt_simulation_run_model import (
        SimulationRunModel,
    )


# Progress bar delegate C++ example: https://doc.qt.io/qt-6/qtnetwork-torrent-example.html
class SimulationRunModelStyledItemDelegate(QtWidgets.QStyledItemDelegate):  # type: ignore[misc]
    def __init__(self, parent=None):
        super().__init__(parent)

        # TODO: Mark as const: https://stackoverflow.com/a/57596202
        self.simulation_run_group_box_title_font_size: Final[int] = 14
        self.simulation_run_group_box_content_font_size: Final[int] = 10
        self.quantum_register_layout_info_text_font_size: Final[int] = 8
        self.simulation_run_title_bottom_margin_y: Final[int] = 8
        self.stringified_quantum_register_y_spacing: Final[int] = 4
        self.stringified_quantum_register_x_spacing: Final[int] = 6
        self.simulation_run_contents_padding_size: Final[int] = 20
        self.simulation_run_group_box_y_spacing: Final[int] = 10

        self.quantum_register_layout_text_format = "(First qubit: {first_qubit:d} - Num. qubits: {n_qubits:d})"
        self.quantum_register_name_column_header = "Quantum register"
        self.input_state_value_column_header = "INPUT"
        self.output_state_value_column_header = "OUTPUT"
        self.unknown_output_state_value_placeholder = "<UNKNOWN>"

    @staticmethod
    def _get_text_width_for_font_size(text: str, options: QtWidgets.QStyleOptionViewItem, font_size: int) -> int:
        return int(
            QtGui.QFontMetrics(QtGui.QFont(options.font.family(), font_size, options.font.weight())).horizontalAdvance(
                text
            )
        )

    @staticmethod
    def _get_text_height_for_font_size(options: QtWidgets.QStyleOptionViewItem, font_size: int) -> int:
        return int(QtGui.QFontMetrics(QtGui.QFont(options.font.family(), font_size, options.font.weight())).height())

    def _get_estimated_quantum_register_name_column_width(
        self, option: QtWidgets.QStyleOptionViewItem, index: QtCore.QModelIndex, font_size: int
    ) -> int:
        if not index.isValid():
            return 0

        index.data(LONGEST_QUANTUM_REGISTER_NAME_QT_ROLE)
        largest_quantum_register_size: int = index.data(LARGEST_QUANTUM_REGISTER_SIZE_QT_ROLE)
        largest_first_qubit_of_quantum_registers: int = index.data(LARGEST_FIRST_QUBIT_OF_QUANTUM_REGISTER_QT_ROLE)

        return (2 * self.stringified_quantum_register_x_spacing) + max(
            SimulationRunModelStyledItemDelegate._get_text_width_for_font_size(
                self.quantum_register_name_column_header, option, font_size
            ),
            SimulationRunModelStyledItemDelegate._get_text_width_for_font_size(
                index.data(LONGEST_QUANTUM_REGISTER_NAME_QT_ROLE), option, font_size
            ),
            SimulationRunModelStyledItemDelegate._get_text_width_for_font_size(
                self.quantum_register_layout_text_format.format(
                    first_qubit=largest_first_qubit_of_quantum_registers, n_qubits=largest_quantum_register_size
                ),
                option,
                font_size,
            ),
        )

    def _get_estimated_quantum_register_contents_column_width(
        self,
        option: QtWidgets.QStyleOptionViewItem,
        largest_quantum_register_size_in_qubits: int,
        font_size: int,
        with_leading_whitespace: bool,
    ) -> int:
        return (
            2 * self.stringified_quantum_register_x_spacing if with_leading_whitespace else 0
        ) + SimulationRunModelStyledItemDelegate._get_text_width_for_font_size(
            "".join(["0" for i in range(largest_quantum_register_size_in_qubits)]), option, font_size
        )

    # TODO: Long quantum registers that cause the total width to be larger than the containing bounding rect should be truncated (i.e. with a text ellipsis) with total estimated content width truncated to max. width of containing bounding rectangle?
    def _get_estimated_bounding_rect(
        self, option: QtWidgets.QStyleOptionViewItem, index: QtCore.QModelIndex
    ) -> QtCore.QSize:
        if not index.isValid():
            return QtCore.QSize(0, 0)

        n_qregs: int = len(index.data(QUANTUM_REGISTER_LAYOUT_QT_ROLE))
        # Quantum register contents are displayed as two rows containing the following information:
        # R0: <QREG_NAME> <STRINGIFIED_INPUT_QUBIT_VALUES>  <STRINGIFIED_OUTPUT_QUBIT_VALUES>
        # R1: <QREG_LAYOUT_INFO>
        group_box_title_height: int = SimulationRunModelStyledItemDelegate._get_text_height_for_font_size(
            option, self.simulation_run_group_box_title_font_size
        )

        qreg_contents_text_height: int = (
            self.stringified_quantum_register_y_spacing
            + SimulationRunModelStyledItemDelegate._get_text_height_for_font_size(
                option, self.simulation_run_group_box_content_font_size
            )
            + self.stringified_quantum_register_y_spacing
            + SimulationRunModelStyledItemDelegate._get_text_height_for_font_size(
                option, self.quantum_register_layout_info_text_font_size
            )
        )
        total_qreg_contents_text_height: int = n_qregs * qreg_contents_text_height
        total_simulation_run_group_box_height = (
            self.simulation_run_contents_padding_size
            + group_box_title_height
            + self.simulation_run_title_bottom_margin_y
            + total_qreg_contents_text_height
            + self.simulation_run_contents_padding_size
        )

        qreg_name_and_layout_info_column_width: int = self._get_estimated_quantum_register_name_column_width(
            option, index, self.simulation_run_group_box_title_font_size
        )

        qreg_content_header_width: int = self._get_text_width_for_font_size(
            self.input_state_value_column_header, option, self.simulation_run_group_box_content_font_size
        )

        max_qreg_qubits_column_width: int = self._get_estimated_quantum_register_contents_column_width(
            option,
            index.data(LARGEST_QUANTUM_REGISTER_SIZE_QT_ROLE),
            self.simulation_run_group_box_content_font_size,
            True,
        )

        max_qreg_content_column_width: int = max(qreg_content_header_width, max_qreg_qubits_column_width)

        max_per_qreg_content_column_width_with_spacing: int = (
            self.stringified_quantum_register_x_spacing
            + max_qreg_content_column_width
            + self.stringified_quantum_register_x_spacing
            + max_qreg_content_column_width
        )

        total_simulation_run_group_box_width = (
            self.simulation_run_contents_padding_size
            + qreg_name_and_layout_info_column_width
            + max_per_qreg_content_column_width_with_spacing
            + max_per_qreg_content_column_width_with_spacing
            + self.simulation_run_contents_padding_size
        )
        return QtCore.QSize(
            min(total_simulation_run_group_box_width, option.rect.bottomRight().x()),
            max(total_simulation_run_group_box_height, option.rect.topRight().y()),
        )

    def sizeHint(self, option: QtWidgets.QStyleOptionViewItem, index: QtCore.QModelIndex) -> QtCore.QSize:  # noqa: N802
        return self._get_estimated_bounding_rect(option, index)

    @staticmethod
    def _paint_rect_edge_points(
        painter: QtGui.QPainter, rect: QtCore.QRect, font_size: int, color: QtGui.QColor, index: QtCore.QModelIndex
    ) -> None:
        painter.save()
        custom_pen = QtGui.QPen(color)
        custom_pen.setWidth(font_size)
        painter.setPen(custom_pen)

        painter.drawPoint(QtCore.QPoint(rect.topLeft()))
        painter.drawText(rect.topLeft().x(), rect.topLeft().y(), str(index.row()) + "-TL")
        painter.drawPoint(QtCore.QPoint(rect.topRight()))
        painter.drawText(rect.topRight().x(), rect.topRight().y(), str(index.row()) + "-TR")
        painter.drawPoint(QtCore.QPoint(rect.bottomLeft()))
        painter.drawText(rect.bottomLeft().x(), rect.bottomLeft().y(), str(index.row()) + "-BL")
        painter.drawPoint(QtCore.QPoint(rect.bottomRight()))
        painter.drawText(rect.bottomRight().x(), rect.bottomRight().y(), str(index.row()) + "-BR")
        painter.restore()

    @staticmethod
    def _stringify_some_qubits_of_n_bit_values_container(
        n_bit_values_container: syrec.n_bit_values_container, first_qubit: int, n_qubits: int
    ) -> str:
        if first_qubit >= n_bit_values_container.size() or first_qubit + n_qubits >= n_bit_values_container.size():
            return ""

        return "".join([
            "1" if n_bit_values_container.test(i) else "0" for i in range(first_qubit, first_qubit + n_qubits)
        ])

    # TODO:
    # @staticmethod
    # def _get_elided_text_for_pixel_width(
    #     text: str, text_font: QtGui.QFontMetrics, available_pixel_width_for_text: int
    # ) -> str:
    #     return text_font.elidedText(text, QtCore.Qt.TextElideMode.Qt.ElideRight, available_pixel_width_for_text)

    def paint(self, painter: QtGui.QPainter, option: QtWidgets.QStyleOptionViewItem, index: QtCore.QModelIndex) -> None:
        if not index.isValid():
            return

        associated_input_output_mapping: SimulationRunModel = index.data(SIMULATION_RUN_IO_STATE_QT_ROLE)

        painter.save()
        painter.setRenderHint(QtGui.QPainter.RenderHint.Antialiasing)
        SimulationRunModelStyledItemDelegate._paint_rect_edge_points(
            painter, option.rect, 5, QtCore.Qt.GlobalColor.cyan, index
        )
        painter.drawRoundedRect(option.rect, 3, 3)

        simulation_run_contents_rect = option.rect.adjusted(
            self.simulation_run_contents_padding_size,
            self.simulation_run_contents_padding_size,
            -self.simulation_run_contents_padding_size,
            -self.simulation_run_contents_padding_size,
        )
        SimulationRunModelStyledItemDelegate._paint_rect_edge_points(
            painter, simulation_run_contents_rect, 5, QtCore.Qt.GlobalColor.red, index
        )

        if QtWidgets.QStyle.StateFlag.State_Selected in option.state:
            painter.fillRect(option.rect, option.palette.highlight())
            painter.setBrush(option.palette.highlightedText())

        painter.save()
        header_font = QtGui.QFont(painter.font().family(), self.simulation_run_group_box_title_font_size)
        header_font.setBold(True)
        painter.setFont(header_font)

        header_title = "Simulation run #" + str(index.row() + 1)
        header_title_height: int = SimulationRunModelStyledItemDelegate._get_text_height_for_font_size(
            option, self.simulation_run_group_box_title_font_size
        )
        painter.drawText(simulation_run_contents_rect.x(), simulation_run_contents_rect.y(), header_title)
        painter.restore()

        header_text_rect = QtCore.QRect(
            simulation_run_contents_rect.topLeft().x(),
            simulation_run_contents_rect.topLeft().y(),
            SimulationRunModelStyledItemDelegate._get_text_width_for_font_size(
                header_title, option, self.simulation_run_group_box_title_font_size
            ),
            header_title_height,
        )

        qreg_name_column_width: int = self._get_estimated_quantum_register_name_column_width(
            option, index, self.simulation_run_group_box_content_font_size
        )

        header_row_column_one_rect = QtCore.QRect(
            header_text_rect.topLeft().x(),
            header_text_rect.topLeft().y() + 2 * self.simulation_run_title_bottom_margin_y,
            qreg_name_column_width,
            SimulationRunModelStyledItemDelegate._get_text_height_for_font_size(
                option, self.simulation_run_group_box_content_font_size
            ),
        )
        header_row_column_one_text_rect = header_row_column_one_rect.adjusted(
            self.stringified_quantum_register_x_spacing, 0, -self.stringified_quantum_register_x_spacing, 0
        )
        # TODO: What if header text is larger than contents?
        painter.drawText(
            header_row_column_one_text_rect,
            QtCore.Qt.AlignmentFlag.AlignTop | QtCore.Qt.AlignmentFlag.AlignCenter,
            self.quantum_register_name_column_header,
        )

        max_qreg_content_width: int = self._get_estimated_quantum_register_contents_column_width(
            option,
            index.data(LARGEST_QUANTUM_REGISTER_SIZE_QT_ROLE),
            self.simulation_run_group_box_content_font_size,
            False,
        )
        header_row_column_two_rect = QtCore.QRect(
            header_row_column_one_rect.topRight().x(),
            header_row_column_one_rect.topRight().y(),
            max_qreg_content_width,
            SimulationRunModelStyledItemDelegate._get_text_height_for_font_size(
                option, self.simulation_run_group_box_content_font_size
            ),
        )
        header_row_column_two_text_rect = header_row_column_two_rect.adjusted(
            self.stringified_quantum_register_x_spacing, 0, -self.stringified_quantum_register_x_spacing, 0
        )
        # TODO: What if header text is larger than contents?
        painter.drawText(
            header_row_column_two_text_rect,
            QtCore.Qt.AlignmentFlag.AlignTop | QtCore.Qt.AlignmentFlag.AlignCenter,
            self.input_state_value_column_header,
        )

        header_row_column_three_rect = QtCore.QRect(
            header_row_column_two_rect.topRight().x(),
            header_row_column_two_rect.topRight().y(),
            max_qreg_content_width,
            SimulationRunModelStyledItemDelegate._get_text_height_for_font_size(
                option, self.simulation_run_group_box_content_font_size
            ),
        )
        header_row_column_three_text_rect = header_row_column_three_rect.adjusted(
            self.stringified_quantum_register_x_spacing, 0, -self.stringified_quantum_register_x_spacing, 0
        )
        # TODO: What if header text is larger than contents?
        painter.drawText(
            header_row_column_three_text_rect,
            QtCore.Qt.AlignmentFlag.AlignTop | QtCore.Qt.AlignmentFlag.AlignCenter,
            self.output_state_value_column_header,
        )

        SimulationRunModelStyledItemDelegate._paint_rect_edge_points(
            painter, header_row_column_one_rect, 5, QtCore.Qt.GlobalColor.red, index
        )
        SimulationRunModelStyledItemDelegate._paint_rect_edge_points(
            painter, header_row_column_two_rect, 5, QtCore.Qt.GlobalColor.blue, index
        )
        SimulationRunModelStyledItemDelegate._paint_rect_edge_points(
            painter, header_row_column_three_rect, 5, QtCore.Qt.GlobalColor.green, index
        )

        row_idx: int = 1
        per_row_y_offset: int = (
            self.stringified_quantum_register_y_spacing
            + SimulationRunModelStyledItemDelegate._get_text_height_for_font_size(
                option, self.simulation_run_group_box_content_font_size
            )
        )
        for qreg_layout in index.data(QUANTUM_REGISTER_LAYOUT_QT_ROLE):
            curr_row_y_offset: int = row_idx * per_row_y_offset

            row_i_column_one = header_row_column_one_text_rect.adjusted(0, curr_row_y_offset, 0, curr_row_y_offset)
            painter.drawText(
                row_i_column_one,
                QtCore.Qt.AlignmentFlag.AlignTop | QtCore.Qt.AlignmentFlag.AlignCenter,
                qreg_layout.quantum_register_name,
            )

            row_i_column_two = header_row_column_two_text_rect.adjusted(0, curr_row_y_offset, 0, curr_row_y_offset)
            painter.drawText(
                row_i_column_two,
                QtCore.Qt.AlignmentFlag.AlignTop | QtCore.Qt.AlignmentFlag.AlignCenter,
                SimulationRunModelStyledItemDelegate._stringify_some_qubits_of_n_bit_values_container(
                    associated_input_output_mapping.input_state,
                    qreg_layout.first_qubit_of_quantum_register,
                    qreg_layout.quantum_register_size,
                ),
            )

            row_i_column_three = header_row_column_three_text_rect.adjusted(0, curr_row_y_offset, 0, curr_row_y_offset)
            painter.drawText(
                row_i_column_three,
                QtCore.Qt.AlignmentFlag.AlignTop | QtCore.Qt.AlignmentFlag.AlignCenter,
                SimulationRunModelStyledItemDelegate._stringify_some_qubits_of_n_bit_values_container(
                    associated_input_output_mapping.expected_output_state,
                    qreg_layout.first_qubit_of_quantum_register,
                    qreg_layout.quantum_register_size,
                )
                if associated_input_output_mapping.expected_output_state is not None
                else self.unknown_output_state_value_placeholder,
            )

            painter.save()
            quantum_layout_info_text_font = QtGui.QFont(
                painter.font().family(), self.quantum_register_layout_info_text_font_size
            )
            painter.setPen(QtCore.Qt.GlobalColor.gray)
            painter.setFont(quantum_layout_info_text_font)

            row_i_plus_column_one = row_i_column_one.adjusted(0, per_row_y_offset, 0, per_row_y_offset)
            painter.drawText(
                row_i_plus_column_one,
                QtCore.Qt.AlignmentFlag.AlignTop | QtCore.Qt.AlignmentFlag.AlignCenter,
                self.quantum_register_layout_text_format.format(
                    first_qubit=qreg_layout.first_qubit_of_quantum_register, n_qubits=qreg_layout.quantum_register_size
                ),
            )
            painter.restore()

            row_idx += 2
        painter.restore()
