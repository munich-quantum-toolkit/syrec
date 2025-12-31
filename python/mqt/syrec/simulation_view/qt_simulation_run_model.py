# Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
# Copyright (c) 2025 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

from __future__ import annotations

from dataclasses import dataclass

from PyQt6 import QtCore, QtGui, QtWidgets

from mqt import syrec

# First custom item data role usable according to: https://doc.qt.io/qt-6/qt.html#ItemDataRole-enum
SIMULATION_RUN_IO_STATE_QT_ROLE: int = QtCore.Qt.ItemDataRole.UserRole


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


# Progress bar delegate C++ example: https://code.qt.io/cgit/qt/qtbase.git/tree/examples/network/torrent?h=5.15
class SimulationRunModelStyledItemDelegate(QtWidgets.QStyledItemDelegate):  # type: ignore[misc]
    def __init__(self, parent=None):
        super().__init__(parent)
        self.padding = 10

    @staticmethod
    def paint(painter, option, index):
        if not index.isValid():
            return

        associated_input_output_mapping: InputOutputStateMapping = index.data(SIMULATION_RUN_IO_STATE_QT_ROLE)

        painter.save()
        rect = option.rect
        if option.state & QtWidgets.QStyle.StateFlag.State_Selected:
            painter.setBrush(QtGui.QColor("#e6f3ff"))
        else:
            painter.setBrush(QtGui.QColor("#ffffff"))

        painter.setPen(QtGui.QColor("#cccccc"))
        # painter.drawRoundedRect(rect.adjusted(5, 5, -5, -5), 8, 8)

        # 2. Draw Title
        title_rect = QtCore.QRect(rect.left() + 15, rect.top() + 15, 100, 50)
        painter.setPen(QtCore.Qt.GlobalColor.black)
        font = painter.font()
        font.setBold(True)
        painter.setFont(font)
        painter.drawText(title_rect, QtCore.Qt.AlignmentFlag.AlignLeft, "Test")

        col_width = (rect.width() - 150) // 3

        for i in range(15):  # Limit to 9 for example
            row = i // 3
            col = i % 3
            label_rect = QtCore.QRect(
                rect.left() + 15 + (col * col_width), rect.top() + 45 + (row * 25), col_width - 10, 20
            )
            painter.setBrush(QtGui.QColor("#f0f0f0"))
            painter.setPen(QtCore.Qt.PenStyle.NoPen)
            painter.drawRoundedRect(label_rect, 4, 4)
            painter.setPen(QtCore.Qt.GlobalColor.darkGray)
            painter.drawText(
                label_rect, QtCore.Qt.AlignmentFlag.AlignCenter, str(associated_input_output_mapping.input_state)
            )

        painter.restore()

    # def paint(self, painter: QtGui.QPainter, option: QtWidgets.QStyleOptionViewItem, index: QtCore.QModelIndex):
    #    super().paint(painter, option, index)

    #    print(index.row())
    #    print(index.column())


# Example delegate: https://stackoverflow.com/questions/53105343/is-it-possible-to-add-a-custom-widget-into-a-qlistview
class QtSimulationRunModel(QtCore.QAbstractListModel):  # type: ignore[misc]
    def __init__(self, parent=None):
        super().__init__(parent)

        self.input_output_state_mappings: list[InputOutputStateMapping] = []

    def rowCount(self, parent: QtCore.QModelIndex) -> int:  # noqa: N802
        return 0 if parent.isValid() else len(self.input_output_state_mappings)

    def data(self, index: QtCore.QModelIndex, role: int) -> object:
        return (
            None
            if not index.isValid() or role != SIMULATION_RUN_IO_STATE_QT_ROLE
            else self.input_output_state_mappings[index.row()]
        )

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

    def update_input_state_qubit_value(self, index: QtCore.QModelIndex, qubit: int, qubit_value: bool) -> bool:
        if self.is_model_index_valid(index) and self.input_output_state_mappings[
            index.row()
        ].update_input_state_qubit_value(qubit, qubit_value):
            self.dataChanged.emit(index, index)
            return True
        return False

    def update_output_state_qubit_value(self, index: QtCore.QModelIndex, qubit: int, qubit_value: bool) -> bool:
        if self.is_model_index_valid(index) and self.input_output_state_mappings[
            index.row()
        ].update_output_state_qubit_value(qubit, qubit_value):
            self.dataChanged.emit(index, index)
            return True
        return False

    def is_model_index_valid(self, index: QtCore.QModelIndex) -> bool:
        return index.isValid() and index.row() >= 0 and index.row() < len(self.input_output_state_mappings)  # type: ignore[no-any-return]
