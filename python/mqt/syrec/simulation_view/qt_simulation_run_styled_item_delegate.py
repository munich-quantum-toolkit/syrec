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
        InputOutputStateMapping,
    )


# Progress bar delegate C++ example: https://doc.qt.io/qt-6/qtnetwork-torrent-example.html
class SimulationRunModelStyledItemDelegate(QtWidgets.QStyledItemDelegate):  # type: ignore[misc]
    def __init__(self, parent=None):
        super().__init__(parent)

        # TODO: Mark as const: https://stackoverflow.com/a/57596202
        self.simulation_run_group_box_title_font_size: Final[int] = 14
        self.simulation_run_group_box_content_font_size: Final[int] = 10
        self.quantum_register_layout_info_text_font_size: Final[int] = 8
        self.stringified_quantum_register_y_spacing: Final[int] = 4
        self.stringified_quantum_register_x_spacing: Final[int] = 6
        self.simulation_run_contents_padding_size: Final[int] = 20
        self.simulation_run_group_box_y_spacing: Final[int] = 10

        self.quantum_register_layout_text_format = "(First qubit: {first_qubit:d} - Num. qubits: {n_qubits:d})"
        self.quantum_register_name_column_header = "Quantum register"
        self.input_state_value_column_header = "INPUT"
        self.output_state_value_column_header = "OUTPUT"

    @staticmethod
    def _get_horizontal_text_width(text: str, options: QtWidgets.QStyleOptionViewItem, font_size: int) -> int:
        return int(
            QtGui.QFontMetrics(QtGui.QFont(options.font.family(), font_size, options.font.weight())).horizontalAdvance(
                text
            )
        )

    @staticmethod
    def _get_vertical_text_width(options: QtWidgets.QStyleOptionViewItem, font_size: int) -> int:
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
            SimulationRunModelStyledItemDelegate._get_horizontal_text_width(
                self.quantum_register_name_column_header, option, font_size
            ),
            SimulationRunModelStyledItemDelegate._get_horizontal_text_width(
                index.data(LONGEST_QUANTUM_REGISTER_NAME_QT_ROLE), option, font_size
            ),
            SimulationRunModelStyledItemDelegate._get_horizontal_text_width(
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
        ) + SimulationRunModelStyledItemDelegate._get_horizontal_text_width(
            "".join(["0" for i in range(largest_quantum_register_size_in_qubits)]), option, font_size
        )

    # TODO: Long quantum registers that cause the total width to be larger than the containing bounding rect should be truncated (i.e. with a text ellipsis) with total estimated content width truncated to max. width of containing bounding rectangle?
    def _get_estimated_bounding_rect(
        self, option: QtWidgets.QStyleOptionViewItem, index: QtCore.QModelIndex
    ) -> QtCore.QSize:
        if not index.isValid():
            return QtCore.QSize(0, 0)

        n_qregs: int = len(index.data(QUANTUM_REGISTER_LAYOUT_QT_ROLE))
        simulation_run_content_height: int = (
            self.simulation_run_group_box_y_spacing
            + self.simulation_run_contents_padding_size
            + SimulationRunModelStyledItemDelegate._get_vertical_text_width(
                option, self.simulation_run_group_box_title_font_size
            )
            + self.stringified_quantum_register_y_spacing
            + n_qregs
            * (
                SimulationRunModelStyledItemDelegate._get_vertical_text_width(
                    option, self.quantum_register_layout_info_text_font_size
                )
                + SimulationRunModelStyledItemDelegate._get_vertical_text_width(
                    option, self.simulation_run_group_box_content_font_size
                )
            )
            + (
                (2 * (n_qregs - 1) * self.stringified_quantum_register_y_spacing)
                if n_qregs > 1
                else self.stringified_quantum_register_y_spacing
            )
            + self.simulation_run_contents_padding_size
            + self.simulation_run_group_box_y_spacing
        )

        quantum_register_content_width: int = self._get_estimated_quantum_register_contents_column_width(
            option,
            index.data(LARGEST_QUANTUM_REGISTER_SIZE_QT_ROLE),
            self.simulation_run_group_box_content_font_size,
            True,
        )
        simulation_run_content_width = (
            self.simulation_run_contents_padding_size
            + self._get_estimated_quantum_register_name_column_width(
                option, index, self.simulation_run_group_box_content_font_size
            )
            + (2 * quantum_register_content_width)
            + self.simulation_run_contents_padding_size
        )
        return QtCore.QSize(
            min(simulation_run_content_width, option.rect.bottomRight().x()),
            max(simulation_run_content_height, option.rect.topRight().y()),
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

    def paint(self, painter: QtGui.QPainter, option: QtWidgets.QStyleOptionViewItem, index: QtCore.QModelIndex) -> None:
        if not index.isValid():
            return

        associated_input_output_mapping: InputOutputStateMapping = index.data(SIMULATION_RUN_IO_STATE_QT_ROLE)

        painter.save()
        estimated_simulation_run_container_size: QtCore.QSize = self._get_estimated_bounding_rect(option, index)
        # simulation_run_container_rect = QtCore.QRect(option.rect.topLeft().x(), (index.row() * estimated_simulation_run_container_size.height()) + option.rect.topLeft().y(), estimated_simulation_run_container_size.width(), estimated_simulation_run_container_size.height())
        simulation_run_container_rect = QtCore.QRect(
            option.rect.topLeft().x(),
            option.rect.topLeft().y() + self.simulation_run_group_box_y_spacing,
            estimated_simulation_run_container_size.width(),
            estimated_simulation_run_container_size.height() - 2 * self.simulation_run_group_box_y_spacing,
        )

        if QtWidgets.QStyle.StateFlag.State_Selected in option.state:
            # print(str(index.row()) + " selected!")
            painter.fillRect(simulation_run_container_rect, option.palette.highlight())
            painter.setBrush(option.palette.highlightedText())

        group_box_opt = QtWidgets.QStyleOptionGroupBox()
        group_box_opt.rect = simulation_run_container_rect
        group_box_opt.text = "Simulation run #" + str(index.row())
        group_box_opt.color = QtCore.Qt.GlobalColor.black
        group_box_opt.textAlignment = QtCore.Qt.AlignmentFlag.AlignLeft
        group_box_opt.subControls = (
            QtWidgets.QStyle.SubControl.SC_GroupBoxFrame | QtWidgets.QStyle.SubControl.SC_GroupBoxLabel
        )
        group_box_opt.state = QtWidgets.QStyle.StateFlag.State_Raised
        group_box_opt.features = QtWidgets.QStyleOptionFrame.FrameFeature.Rounded

        # 2. Draw the control using the current application style
        # Using the widget's style ensures it respects OS themes

        # 3. Draw the GroupBox
        app_style = QtWidgets.QApplication.style()
        app_style.drawComplexControl(QtWidgets.QStyle.ComplexControl.CC_GroupBox, group_box_opt, painter)

        SimulationRunModelStyledItemDelegate._paint_rect_edge_points(
            painter, simulation_run_container_rect, 5, QtCore.Qt.GlobalColor.darkMagenta, index
        )

        # 3. Calculate where the content inside the box should go
        # We use subControlRect to find where the frame actually is
        # 4. Calculate Content Area
        # This returns a rect relative to the group_box_opt.rect
        relative_group_box_content_rect = app_style.subControlRect(
            QtWidgets.QStyle.ComplexControl.CC_GroupBox,
            group_box_opt,
            QtWidgets.QStyle.SubControl.SC_GroupBoxContents,
            None,
        )
        # The calculation for the position of the contents of the group box does not seems to set the y-coordinate correctly while the x coordinate, width and height are correctly set? Return value of function call might be relative to parent?
        relative_group_box_content_rect.setTop(
            simulation_run_container_rect.top() + relative_group_box_content_rect.top()
        )
        SimulationRunModelStyledItemDelegate._paint_rect_edge_points(
            painter, relative_group_box_content_rect, 5, QtCore.Qt.GlobalColor.magenta, index
        )

        quantum_register_name_column_width: int = self._get_estimated_quantum_register_name_column_width(
            option, index, self.simulation_run_group_box_content_font_size
        )
        quantum_register_name_column_start_x: int = (
            relative_group_box_content_rect.topLeft().x() + self.simulation_run_contents_padding_size
        )

        initial_column_one_rect = QtCore.QRect(
            quantum_register_name_column_start_x + self.simulation_run_contents_padding_size,
            relative_group_box_content_rect.topLeft().y() + self.stringified_quantum_register_y_spacing,
            quantum_register_name_column_width,
            SimulationRunModelStyledItemDelegate._get_vertical_text_width(
                option, self.simulation_run_group_box_content_font_size
            ),
        )
        painter.drawText(
            initial_column_one_rect,
            QtCore.Qt.AlignmentFlag.AlignTop | QtCore.Qt.AlignmentFlag.AlignCenter,
            "Quantum register",
        )

        quantum_register_content_width_without_padding: int = (
            self._get_estimated_quantum_register_contents_column_width(
                option,
                index.data(LARGEST_QUANTUM_REGISTER_SIZE_QT_ROLE),
                self.simulation_run_group_box_content_font_size,
                False,
            )
        )
        quantum_register_input_values_start_x: int = (
            initial_column_one_rect.topRight().x() + self.stringified_quantum_register_x_spacing
        )

        initial_column_two_rect = QtCore.QRect(
            quantum_register_input_values_start_x,
            initial_column_one_rect.topLeft().y(),
            quantum_register_content_width_without_padding,
            initial_column_one_rect.height(),
        )
        painter.drawText(
            initial_column_two_rect, QtCore.Qt.AlignmentFlag.AlignTop | QtCore.Qt.AlignmentFlag.AlignCenter, "INPUT"
        )

        quantum_register_output_values_start_x: int = (
            initial_column_two_rect.topRight().x() + self.stringified_quantum_register_x_spacing
        )
        initial_column_three_rect = QtCore.QRect(
            quantum_register_output_values_start_x,
            initial_column_one_rect.topLeft().y(),
            quantum_register_content_width_without_padding,
            initial_column_two_rect.height(),
        )
        painter.drawText(
            initial_column_three_rect, QtCore.Qt.AlignmentFlag.AlignTop | QtCore.Qt.AlignmentFlag.AlignCenter, "OUTPUT"
        )

        SimulationRunModelStyledItemDelegate._paint_rect_edge_points(
            painter, initial_column_one_rect, 5, QtCore.Qt.GlobalColor.red, index
        )
        SimulationRunModelStyledItemDelegate._paint_rect_edge_points(
            painter, initial_column_two_rect, 5, QtCore.Qt.GlobalColor.blue, index
        )
        SimulationRunModelStyledItemDelegate._paint_rect_edge_points(
            painter, initial_column_three_rect, 5, QtCore.Qt.GlobalColor.green, index
        )

        row_idx: int = 1
        row_i_y_offset: int = (
            self.stringified_quantum_register_y_spacing
            + SimulationRunModelStyledItemDelegate._get_vertical_text_width(
                option, self.simulation_run_group_box_content_font_size
            )
        )
        for qreg_layout in index.data(QUANTUM_REGISTER_LAYOUT_QT_ROLE):
            curr_row_y_offset: int = row_idx * row_i_y_offset

            row_i_column_one = initial_column_one_rect.adjusted(0, curr_row_y_offset, 0, curr_row_y_offset)
            painter.drawText(
                row_i_column_one,
                QtCore.Qt.AlignmentFlag.AlignTop | QtCore.Qt.AlignmentFlag.AlignCenter,
                qreg_layout.quantum_register_name,
            )

            row_i_column_two = initial_column_two_rect.adjusted(0, curr_row_y_offset, 0, curr_row_y_offset)
            painter.drawText(
                row_i_column_two,
                QtCore.Qt.AlignmentFlag.AlignTop | QtCore.Qt.AlignmentFlag.AlignCenter,
                SimulationRunModelStyledItemDelegate._stringify_some_qubits_of_n_bit_values_container(
                    associated_input_output_mapping.input_state,
                    qreg_layout.first_qubit_of_quantum_register,
                    qreg_layout.quantum_register_size,
                ),
            )

            row_i_column_three = initial_column_three_rect.adjusted(0, curr_row_y_offset, 0, curr_row_y_offset)
            painter.drawText(
                row_i_column_three,
                QtCore.Qt.AlignmentFlag.AlignTop | QtCore.Qt.AlignmentFlag.AlignCenter,
                SimulationRunModelStyledItemDelegate._stringify_some_qubits_of_n_bit_values_container(
                    associated_input_output_mapping.output_state,
                    qreg_layout.first_qubit_of_quantum_register,
                    qreg_layout.quantum_register_size,
                )
                if associated_input_output_mapping.output_state is not None
                else "<UNKNOWN>",
            )

            painter.save()
            quantum_layout_info_text_font = QtGui.QFont(
                painter.font().family(), self.quantum_register_layout_info_text_font_size
            )
            painter.setPen(QtCore.Qt.GlobalColor.gray)
            painter.setFont(quantum_layout_info_text_font)

            row_i_plus_column_one = row_i_column_one.adjusted(0, row_i_y_offset, 0, row_i_y_offset)
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
        return
