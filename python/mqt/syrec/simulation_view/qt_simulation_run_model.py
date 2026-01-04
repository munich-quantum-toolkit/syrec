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

from PyQt6 import QtCore

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
        self.beginRemoveRows(QtCore.QModelIndex(), index.row(), index.row())

        if self.is_model_index_valid(index):
            self.input_output_state_mappings.pop(index.row())
            self.endRemoveRows()
            # self.layoutChanged.emit()
            return True

        self.endRemoveRows()
        return False

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
