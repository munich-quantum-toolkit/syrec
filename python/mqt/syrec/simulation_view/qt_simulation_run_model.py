# Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
# Copyright (c) 2025 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

from __future__ import annotations

from dataclasses import dataclass
from typing import Final

from PyQt6 import QtCore, QtGui, QtWidgets

from mqt import syrec

# TODO: Mark as const: https://stackoverflow.com/a/57596202
# Some debugging tips: https://www.eso.org/~eltmgr/ECS/documents-latest/CUT/sphinx_doc/latest/docs/500_gui_development.html#gdb
# First custom item data role usable according to: https://doc.qt.io/qt-6/qt.html#ItemDataRole-enum

# TODO: Why does the mypy checker report the error "no-any-return" when processing the python function:
#  def _get_vertical_text_width(options: QtWidgets.QStyleOptionsViewItem, font_size: int) -> int:
#   return QtGui.QFontMetrics(QtGui.QFont(options.font.family(), font_size, options.font.weight())).height()
#
# The most common reason is that mypy does not have type information for the QtGui or QtWidgets modules. If you haven't installed the type stubs for your Qt bindings, mypy treats all calls to those libraries as returning Any.
# When you call .height(), mypy sees it as Any. Returning Any from a function marked as -> int triggers the no-any-return warning because mypy cannot verify that the value is actually an integer.
# Solution:
# Install the appropriate type stubs for your framework:
# - For PyQt6: pip install PyQt6-stubs
# - For PySide6: pip install shiboken6 (Type information is usually bundled, but ensure your environment is configured correctly).
SIMULATION_RUN_IO_STATE_QT_ROLE: Final[int] = QtCore.Qt.ItemDataRole.UserRole
QUANTUM_REGISTER_LAYOUT_QT_ROLE: Final[int] = SIMULATION_RUN_IO_STATE_QT_ROLE + 1
LONGEST_QUANTUM_REGISTER_NAME_QT_ROLE: Final[int] = QUANTUM_REGISTER_LAYOUT_QT_ROLE + 1
LARGEST_QUANTUM_REGISTER_SIZE_QT_ROLE: Final[int] = LONGEST_QUANTUM_REGISTER_NAME_QT_ROLE + 1
LARGEST_FIRST_QUBIT_OF_QUANTUM_REGISTER_QT_ROLE: Final[int] = LARGEST_QUANTUM_REGISTER_SIZE_QT_ROLE + 1


@dataclass
class QuantumRegisterLayout:
    quantum_register_name: str
    first_qubit_of_quantum_register: int
    quantum_register_size: int


@dataclass
class InputOutputStateMapping:
    input_state: syrec.n_bit_values_container
    output_state: syrec.n_bit_values_container | None

    def initialize_output_state_as_copy_of_input_state(self) -> bool:
        if self.output_state is not None:
            return False

        self.output_state = syrec.n_bit_values_container(self.input_state.size())
        for i in range(self.output_state.size()):
            self.output_state.set(self.input_state.test(i))
        return True

    def update_input_state_qubit_value(self, qubit: int, qubit_value: bool) -> bool:
        if qubit < 0 or qubit >= self.input_state.size():
            return False

        self.input_state.set(qubit, qubit_value)
        return True

    def update_output_state_qubit_value(self, qubit: int, qubit_value: bool) -> bool:
        if self.output_state is None or qubit < 0 or qubit >= self.output_state.size():
            return False

        self.output_state.set(qubit, qubit_value)
        return True


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
    def _get_vertical_text_width(options: QtWidgets.QStyleOptionsViewItem, font_size: int) -> int:
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
        self, option: QtWidgets.QStyleOptionViewItem, font_size: int, with_leading_whitespace: bool
    ) -> int:
        return (
            2 * self.stringified_quantum_register_x_spacing if with_leading_whitespace else 0
        ) + SimulationRunModelStyledItemDelegate._get_horizontal_text_width(
            "".join(["0" for i in range(32)]), option, font_size
        )

    # TODO: Group box header?
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
            option, self.simulation_run_group_box_content_font_size, True
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
        painter: QtGui.QPainter, rect: QtCore.QRect, font_size: int, color: QtGui.Color
    ) -> None:
        painter.save()
        custom_pen = QtGui.QPen(color)
        custom_pen.setWidth(font_size)
        painter.setPen(custom_pen)

        painter.drawPoint(QtCore.QPoint(rect.topLeft()))
        painter.drawPoint(QtCore.QPoint(rect.topRight()))
        painter.drawPoint(QtCore.QPoint(rect.bottomLeft()))
        painter.drawPoint(QtCore.QPoint(rect.bottomRight()))
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
            painter, simulation_run_container_rect, 5, QtCore.Qt.GlobalColor.darkMagenta
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
            painter, relative_group_box_content_rect, 5, QtCore.Qt.GlobalColor.magenta
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
                option, self.simulation_run_group_box_content_font_size, False
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
            painter, initial_column_one_rect, 5, QtCore.Qt.GlobalColor.red
        )
        SimulationRunModelStyledItemDelegate._paint_rect_edge_points(
            painter, initial_column_two_rect, 5, QtCore.Qt.GlobalColor.blue
        )
        SimulationRunModelStyledItemDelegate._paint_rect_edge_points(
            painter, initial_column_three_rect, 5, QtCore.Qt.GlobalColor.green
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


# Example delegate: https://stackoverflow.com/questions/53105343/is-it-possible-to-add-a-custom-widget-into-a-qlistview
class QtSimulationRunModel(QtCore.QAbstractListModel):  # type: ignore[misc]
    def __init__(
        self, annotatable_quantum_computation: syrec.annotatable_quantum_computation, parent: QtCore.QObject = None
    ):
        super().__init__(parent)
        self.input_output_state_mappings: list[InputOutputStateMapping] = []
        self.quantum_register_layouts: list[QuantumRegisterLayout] = (
            QtSimulationRunModel.__record_quantum_register_layouts(annotatable_quantum_computation)
        )
        self.longest_quantum_register_name: str = ""
        self.largest_quantum_register_size: int = 0
        self.largest_first_qubit_of_quantum_registers: int = 0

        for qreg_layout in self.quantum_register_layouts:
            self.longest_quantum_register_name = (
                qreg_layout.quantum_register_name
                if len(qreg_layout.quantum_register_name) > len(self.longest_quantum_register_name)
                else self.longest_quantum_register_name
            )
            self.largest_quantum_register_size = max(
                qreg_layout.quantum_register_size, self.largest_quantum_register_size
            )

        if len(self.quantum_register_layouts) > 0:
            self.largest_first_qubit_of_quantum_registers = self.quantum_register_layouts[
                len(self.quantum_register_layouts) - 1
            ].first_qubit_of_quantum_register

    @staticmethod
    def _does_qubit_label_start_with_internal_qubit_label_prefix(qubit_label: str) -> bool:
        return qubit_label.startswith("__q")

    @staticmethod
    def __record_quantum_register_layouts(
        annotatable_quantum_computation: syrec.annotatable_quantum_computation,
    ) -> list[QuantumRegisterLayout]:
        quantum_register_layouts: list[QuantumRegisterLayout] = []
        for qreg in annotatable_quantum_computation.qregs.values():
            if qreg.size == 0 or QtSimulationRunModel._does_qubit_label_start_with_internal_qubit_label_prefix(
                annotatable_quantum_computation.get_qubit_label(qreg.start, syrec.qubit_label_type.internal)
            ):
                continue

            quantum_register_layouts.append(QuantumRegisterLayout(qreg.name, qreg.start, qreg.size))
        return quantum_register_layouts

    def rowCount(self, parent: QtCore.QModelIndex) -> int:  # noqa: N802
        return 0 if parent.isValid() else len(self.input_output_state_mappings)

    def data(self, index: QtCore.QModelIndex, role: int) -> object:
        if not index.isValid():
            return None

        if role == SIMULATION_RUN_IO_STATE_QT_ROLE:
            return self.input_output_state_mappings[index.row()]

        if role == QUANTUM_REGISTER_LAYOUT_QT_ROLE:
            return self.quantum_register_layouts

        if role == LONGEST_QUANTUM_REGISTER_NAME_QT_ROLE:
            return self.longest_quantum_register_name

        if role == LARGEST_QUANTUM_REGISTER_SIZE_QT_ROLE:
            return self.largest_quantum_register_size

        if role == LARGEST_FIRST_QUBIT_OF_QUANTUM_REGISTER_QT_ROLE:
            return self.largest_first_qubit_of_quantum_registers

        return None

    # TODO: Check for duplicates?
    def add_simulation_run(self, input_output_state_mapping: InputOutputStateMapping) -> bool:
        n_simulation_runs: int = len(self.input_output_state_mappings)
        self.beginInsertRows(QtCore.QModelIndex(), n_simulation_runs, n_simulation_runs)
        self.input_output_state_mappings.append(input_output_state_mapping)
        self.endInsertRows()
        return True

    def delete_simulation_run(self, index: QtCore.QModelIndex) -> bool:
        # self.beginRemoveRows()
        if self.is_model_index_valid(index):
            self.input_output_state_mappings.remove(index.row())
            self.layoutChanged.emit()
            return True
        return False
        # self.endRemoveRows()

    # TODO: Check for duplicates?
    def update_input_state_qubit_value(self, index: QtCore.QModelIndex, qubit: int, qubit_value: bool) -> bool:
        if self.is_model_index_valid(index) and self.input_output_state_mappings[
            index.row()
        ].update_input_state_qubit_value(qubit, qubit_value):
            self.dataChanged.emit(index, index)
            return True
        return False

    # TODO: Check for duplicates?
    def update_output_state_qubit_value(self, index: QtCore.QModelIndex, qubit: int, qubit_value: bool) -> bool:
        if self.is_model_index_valid(index) and self.input_output_state_mappings[
            index.row()
        ].update_output_state_qubit_value(qubit, qubit_value):
            self.dataChanged.emit(index, index)
            return True
        return False

    def is_model_index_valid(self, index: QtCore.QModelIndex) -> bool:
        return index.isValid() and index.row() >= 0 and index.row() < len(self.input_output_state_mappings)  # type: ignore[no-any-return]
