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
ANNOTATABLE_QUANTUM_COMPUTATION_QT_ROLE: Final[int] = LARGEST_FIRST_QUBIT_OF_QUANTUM_REGISTER_QT_ROLE + 1
LARGEST_SIM_RUN_NUMBER_QT_ROLE: Final[int] = ANNOTATABLE_QUANTUM_COMPUTATION_QT_ROLE + 1


@dataclass(frozen=True)
class QuantumRegisterLayout:
    qreg_name: str
    first_qubit_of_qreg: int
    qreg_size: int


class SimulationRunModel:
    input_state: syrec.n_bit_values_container
    expected_output_state: syrec.n_bit_values_container | None = None
    actual_output_state: syrec.n_bit_values_container | None = None
    do_expected_and_actual_outputs_match: bool | None = None
    execution_runtime_in_ms: float | None = None

    def __init__(
        self,
        input_state: syrec.n_bit_values_container,
        expected_output_state: syrec.n_bit_values_container | None = None,
    ):
        if expected_output_state is not None and input_state.size() != expected_output_state.size():
            msg = f"Expected output state size (n_qubits = {expected_output_state.size()}) did not match input state size (n_qubits = {input_state.size()})"
            raise ValueError(msg)

        self.input_state = input_state
        self.expected_output_state = expected_output_state

    def initialize_expected_output_state_as_copy_of_input_state(self) -> None:
        if self.expected_output_state is not None:
            return

        self.expected_output_state = syrec.n_bit_values_container(self.input_state.size())
        for i in range(self.expected_output_state.size()):
            self.expected_output_state.set(i, self.input_state.test(i))

    def reset_result_of_execution(self) -> None:
        self.actual_output_state = None
        self.do_expected_and_actual_outputs_match = None
        self.execution_runtime_in_ms = None

    def set_result_of_simulation_execution(
        self, actual_output_state: syrec.n_bit_values_container, execution_runtime_in_ms: float
    ) -> None:
        if actual_output_state.size() != self.input_state.size():
            msg = f"Actual output state size (n_qubits = {actual_output_state.size()}) did not match input state size (n_qubits = {self.input_state.size()})"
            raise ValueError(msg)
        if self.expected_output_state is None:
            msg = "Tried to set actual output state when expected output state was not set!"
            raise ValueError(msg)
        if self.expected_output_state.size() != actual_output_state.size():
            msg = f"Actual output state size (n_qubits = {actual_output_state.size()}) did not match expected output state size (n_qubits = {self.expected_output_state.size()})"
            raise ValueError(msg)
        if execution_runtime_in_ms < 0:
            msg = f"Invalid execution runtime value {execution_runtime_in_ms}"
            raise ValueError(msg)

        if self.actual_output_state is None:
            self.actual_output_state = actual_output_state
        else:
            self.actual_output_state = syrec.n_bit_values_container(self.input_state.size())
            for i in range(self.expected_output_state.size()):
                self.actual_output_state.set(actual_output_state.test(i))

        self.execution_runtime_in_ms = execution_runtime_in_ms

    def update_input_state_qubit_value(self, qubit: int, new_qubit_value: bool) -> bool:
        return SimulationRunModel._update_n_bit_values_container_qubit_value(self.input_state, qubit, new_qubit_value)

    def update_expected_output_state_qubit_value(self, qubit: int, new_qubit_value: bool) -> bool:
        if self.expected_output_state is None:
            return False

        return SimulationRunModel._update_n_bit_values_container_qubit_value(
            self.expected_output_state, qubit, new_qubit_value
        )

    @staticmethod
    def do_output_states_match(
        expected_output_state: syrec.n_bit_values_container | None, actual_output_state: syrec.n_bit_values_container
    ) -> bool | None:
        if expected_output_state is None:
            return None

        if expected_output_state.size() != actual_output_state.size():
            msg = f"Expected output state to have {expected_output_state.size()} qubits but actual output state contained {actual_output_state.size()} qubits!"
            raise ValueError(msg)

        do_expected_and_actual_input_states_match = True
        for qubit in range(actual_output_state.size()):
            do_expected_and_actual_input_states_match &= actual_output_state.test(qubit) == expected_output_state.test(
                qubit
            )
            if not do_expected_and_actual_input_states_match:
                break
        return do_expected_and_actual_input_states_match

    @staticmethod
    def _update_n_bit_values_container_qubit_value(
        n_bit_values_container: syrec.n_bit_values_container, qubit: int, new_qubit_value: bool
    ) -> bool:
        if qubit < 0 or qubit >= n_bit_values_container.size():
            return False

        n_bit_values_container.set(qubit, new_qubit_value)
        return True


# Example delegate: https://stackoverflow.com/questions/53105343/is-it-possible-to-add-a-custom-widget-into-a-qlistview
class QtSimulationRunModel(QtCore.QAbstractListModel):  # type: ignore[misc]
    def __init__(
        self, annotatable_quantum_computation: syrec.annotatable_quantum_computation, parent: QtCore.QObject = None
    ):
        super().__init__(parent)
        self.n_data_qubits: int = annotatable_quantum_computation.num_data_qubits
        self.simulation_run_models: list[SimulationRunModel] = []
        self.quantum_register_layouts: list[QuantumRegisterLayout] = (
            QtSimulationRunModel._record_quantum_register_layouts(annotatable_quantum_computation)
        )
        self.longest_quantum_register_name: str = ""
        self.largest_quantum_register_size: int = 0
        self.largest_first_qubit_of_quantum_registers: int = 0
        self.annotatable_quantum_computation = annotatable_quantum_computation

        for qreg_layout in self.quantum_register_layouts:
            self.longest_quantum_register_name = (
                qreg_layout.qreg_name
                if len(qreg_layout.qreg_name) > len(self.longest_quantum_register_name)
                else self.longest_quantum_register_name
            )
            self.largest_quantum_register_size = max(qreg_layout.qreg_size, self.largest_quantum_register_size)

        if len(self.quantum_register_layouts) > 0:
            self.largest_first_qubit_of_quantum_registers = self.quantum_register_layouts[
                len(self.quantum_register_layouts) - 1
            ].first_qubit_of_qreg

    @staticmethod
    def _does_qubit_label_start_with_internal_qubit_label_prefix(qubit_label: str) -> bool:
        return qubit_label.startswith("__q")

    @staticmethod
    def _record_quantum_register_layouts(
        annotatable_quantum_computation: syrec.annotatable_quantum_computation,
    ) -> list[QuantumRegisterLayout]:
        quantum_register_layouts: list[QuantumRegisterLayout] = []
        for qreg in annotatable_quantum_computation.qregs.values():
            if qreg.size == 0 or QtSimulationRunModel._does_qubit_label_start_with_internal_qubit_label_prefix(
                annotatable_quantum_computation.get_qubit_label(qreg.start, syrec.qubit_label_type.internal)
            ):
                continue

            quantum_register_layouts.append(QuantumRegisterLayout(qreg.name, qreg.start, qreg.size))

        quantum_register_layouts.sort(key=lambda qreg_layout: qreg_layout.first_qubit_of_qreg)
        return quantum_register_layouts

    def rowCount(self, parent: QtCore.QModelIndex) -> int:  # noqa: N802
        return 0 if parent.isValid() else len(self.simulation_run_models)

    def data(self, index: QtCore.QModelIndex, role: int) -> object:
        if not index.isValid():
            return None

        if role == SIMULATION_RUN_IO_STATE_QT_ROLE:
            return self.simulation_run_models[index.row()]

        if role == QUANTUM_REGISTER_LAYOUT_QT_ROLE:
            return self.quantum_register_layouts

        if role == LONGEST_QUANTUM_REGISTER_NAME_QT_ROLE:
            return self.longest_quantum_register_name

        if role == LARGEST_QUANTUM_REGISTER_SIZE_QT_ROLE:
            return self.largest_quantum_register_size

        if role == LARGEST_FIRST_QUBIT_OF_QUANTUM_REGISTER_QT_ROLE:
            return self.largest_first_qubit_of_quantum_registers

        if role == ANNOTATABLE_QUANTUM_COMPUTATION_QT_ROLE:
            return self.annotatable_quantum_computation

        if role == LARGEST_SIM_RUN_NUMBER_QT_ROLE:
            return self.rowCount(QtCore.QModelIndex())

        return None

    def get_simulation_run_model(self, index: int) -> SimulationRunModel | None:
        if index >= 0 and index < len(self.simulation_run_models):
            return self.simulation_run_models[index]
        return None

    # TODO: Check for duplicates?
    def add_simulation_run_model(self, simulation_run_model: SimulationRunModel) -> bool:
        n_simulation_runs: int = len(self.simulation_run_models)
        self.beginInsertRows(QtCore.QModelIndex(), n_simulation_runs, n_simulation_runs)
        self.simulation_run_models.append(simulation_run_model)
        self.endInsertRows()
        return True

    def delete_simulation_run_model(self, index: QtCore.QModelIndex) -> bool:
        self.beginRemoveRows(QtCore.QModelIndex(), index.row(), index.row())

        if self.is_model_index_valid(index):
            self.simulation_run_models.pop(index.row())
            self.endRemoveRows()
            # self.layoutChanged.emit()
            return True

        self.endRemoveRows()
        return False

    def delete_all_simulation_run_models(self) -> None:
        self.beginResetModel()
        self.simulation_run_models.clear()
        self.endResetModel()

    def add_all_possible_simulation_run_models(self) -> bool:
        if self.rowCount(QtCore.QModelIndex()) > 0:
            return False

        self.beginInsertRows(QtCore.QModelIndex(), 0, 0)
        for i in range(2**self.n_data_qubits):
            binary_string_of_i = format(i, "b")
            input_state = syrec.n_bit_values_container(self.n_data_qubits)

            n_qubits_to_process_in_binary_string: int = min(self.n_data_qubits, len(binary_string_of_i))
            qubit_idx_in_binary_string: int = n_qubits_to_process_in_binary_string - 1
            for qubit in range(n_qubits_to_process_in_binary_string):
                qubit_value: bool = binary_string_of_i[qubit_idx_in_binary_string] == "1"
                input_state.set(qubit, qubit_value)
                qubit_idx_in_binary_string -= 1

            output_state: syrec.n_bit_values_container | None = None
            self.simulation_run_models.append(SimulationRunModel(input_state, output_state))
        self.endInsertRows()
        return True

    def update_edited_simulation_run_model(
        self, index: QtCore.QModelIndex, updated_simulation_run_data: SimulationRunModel
    ) -> None:
        if not self.is_model_index_valid(index):
            msg = "Invalid model index!"
            raise ValueError(msg)

        # TODO: Further validation

        self.simulation_run_models[index.row()] = updated_simulation_run_data
        self.dataChanged.emit(index, index)

    # TODO: Check that no duplicate input or expected output_state is added
    # TODO: Add custom error messages if validation fails
    def update_model_using_simulation_run_result(
        self,
        index: QtCore.QModelIndex,
        actual_output_state: syrec.n_bit_values_container,
        do_expected_and_actual_outputs_match: bool | None,
        execution_runtime_in_ms: float,
    ) -> None:
        if not self.is_model_index_valid(index):
            msg = "Invalid model index!"
            raise ValueError(msg)

        to_be_updated_simulation_run_model: SimulationRunModel = self.simulation_run_models[index.row()]
        # TODO: Should we validate that the current expected output state is equal to the input state
        # if updated_simulation_run_model.expected_output_state is not None and updated_simulation_run_model.expected_output_state.size() != to_be_updated_simulation_run_model.input_state.size():
        #     msg = "Input state sizes did not match"
        #     raise ValueError(msg)

        if (
            actual_output_state is not None
            and actual_output_state.size() != to_be_updated_simulation_run_model.input_state.size()
        ):
            msg = "Input state sizes did not match"
            raise ValueError(msg)

        self.simulation_run_models[index.row()].actual_output_state = actual_output_state
        self.simulation_run_models[
            index.row()
        ].do_expected_and_actual_outputs_match = do_expected_and_actual_outputs_match
        self.simulation_run_models[index.row()].execution_runtime_in_ms = execution_runtime_in_ms
        self.dataChanged.emit(index, index)

    def is_model_index_valid(self, index: QtCore.QModelIndex) -> bool:
        return index.isValid() and index.row() >= 0 and index.row() < len(self.simulation_run_models)  # type: ignore[no-any-return]
