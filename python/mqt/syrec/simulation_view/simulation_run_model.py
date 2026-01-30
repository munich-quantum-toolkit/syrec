# Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
# Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

from __future__ import annotations

from dataclasses import dataclass
from typing import TYPE_CHECKING, Final

from PyQt6 import QtCore

from mqt import syrec

from ..logger_utils import log_error_to_console

if TYPE_CHECKING:
    from collections.abc import Iterable

# Some debugging tips: https://www.eso.org/~eltmgr/ECS/documents-latest/CUT/sphinx_doc/latest/docs/500_gui_development.html#gdb
# First custom item data role usable according to: https://doc.qt.io/qt-6/qt.html#ItemDataRole-enum
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
        actual_output_state: syrec.n_bit_values_container | None = None,
        create_new_n_bit_values_container_instances: bool = False,
    ) -> None:
        if expected_output_state is not None and input_state.size() != expected_output_state.size():
            msg = f"Expected output state size (n_qubits = {expected_output_state.size()}) did not match input state size (n_qubits = {input_state.size()})"
            log_error_to_console(msg, num_additionally_skipped_stack_frames_starting_from_caller_function=1)
            raise ValueError(msg)
        if actual_output_state is not None and input_state.size() != actual_output_state.size():
            msg = f"Actual output state size (n_qubits = {actual_output_state.size()}) did not match input state size (n_qubits = {input_state.size()})"
            log_error_to_console(msg, num_additionally_skipped_stack_frames_starting_from_caller_function=1)
            raise ValueError(msg)

        if not create_new_n_bit_values_container_instances:
            self.input_state = input_state
            self.expected_output_state = expected_output_state
            self.actual_output_state = actual_output_state
        else:
            self.input_state = syrec.n_bit_values_container(input_state.size())
            for qubit in range(input_state.size()):
                self.input_state.set(qubit, input_state.test(qubit))  # type: ignore[arg-type]
            if expected_output_state is not None:
                self.expected_output_state = syrec.n_bit_values_container(expected_output_state.size())
                for qubit in range(expected_output_state.size()):
                    self.expected_output_state.set(qubit, expected_output_state.test(qubit))  # type: ignore[arg-type]
            if actual_output_state is not None:
                self.actual_output_state = syrec.n_bit_values_container(actual_output_state.size())
                for qubit in range(actual_output_state.size()):
                    self.actual_output_state.set(qubit, actual_output_state.test(qubit))  # type: ignore[arg-type]

    def initialize_expected_output_state_as_copy_of_input_state(self) -> None:
        if self.expected_output_state is not None:
            return

        self.expected_output_state = syrec.n_bit_values_container(self.input_state.size())
        for i in range(self.expected_output_state.size()):
            self.expected_output_state.set(i, self.input_state.test(i))  # type: ignore[arg-type]

    def reset_result_of_execution(self, reset_actual_output_state: bool = True) -> None:
        if reset_actual_output_state:
            self.actual_output_state = None

        self.do_expected_and_actual_outputs_match = None
        self.execution_runtime_in_ms = None

    def set_result_of_simulation_execution(
        self,
        actual_output_state: syrec.n_bit_values_container,
        do_expected_and_actual_output_states_match: bool | None,
        execution_runtime_in_ms: float,
    ) -> None:
        if actual_output_state.size() != self.input_state.size():
            msg = f"Actual output state size (n_qubits = {actual_output_state.size()}) did not match input state size (n_qubits = {self.input_state.size()})"
            log_error_to_console(msg, num_additionally_skipped_stack_frames_starting_from_caller_function=1)
            raise ValueError(msg)
        if execution_runtime_in_ms < 0:
            msg = f"Invalid execution runtime value {execution_runtime_in_ms}"
            log_error_to_console(msg, num_additionally_skipped_stack_frames_starting_from_caller_function=1)
            raise ValueError(msg)

        if self.actual_output_state is None:
            self.actual_output_state = syrec.n_bit_values_container(self.input_state.size())

        for i in range(self.actual_output_state.size()):
            self.actual_output_state.set(i, actual_output_state.test(i))  # type: ignore[arg-type]

        self.do_expected_and_actual_outputs_match = do_expected_and_actual_output_states_match
        self.execution_runtime_in_ms = execution_runtime_in_ms

    def update_input_state_qubit_value(self, qubit: int, new_qubit_value: bool) -> bool:
        return SimulationRunModel._update_n_bit_values_container_qubit_value(self.input_state, qubit, new_qubit_value)

    def update_expected_output_state_qubit_value(self, qubit: int, new_qubit_value: bool) -> bool:
        if self.expected_output_state is None:
            return False

        return SimulationRunModel._update_n_bit_values_container_qubit_value(
            self.expected_output_state, qubit, new_qubit_value
        )

    def update_user_editable_data(
        self,
        edited_input_state: syrec.n_bit_values_container,
        edited_expected_output_state: syrec.n_bit_values_container | None,
    ) -> None:
        if self.input_state.size() != edited_input_state.size():
            msg = f"Updated input state size state size (n_qubits = {edited_input_state.size()}) did not match current input state size (n_qubits = {self.input_state.size()})"
            log_error_to_console(msg, num_additionally_skipped_stack_frames_starting_from_caller_function=1)
            raise ValueError(msg)

        if edited_expected_output_state is not None and edited_expected_output_state.size() != self.input_state.size():
            msg = f"Expected output state size (n_qubits = {edited_expected_output_state.size()}) did not match input state size (n_qubits = {self.input_state.size()})"
            log_error_to_console(msg, num_additionally_skipped_stack_frames_starting_from_caller_function=1)
            raise ValueError(msg)

        did_input_state_change: bool = False
        for i in range(self.input_state.size()):
            did_input_state_change |= self.input_state.test(i) != edited_input_state.test(i)
            self.input_state.set(i, edited_input_state.test(i))  # type: ignore[arg-type]

        # If the edited input state does not match the current input state of this instance then reset the previously determined simulation run execution results
        # since they were based on the current input state
        if did_input_state_change:
            self.reset_result_of_execution()

        if edited_expected_output_state is None:
            self.expected_output_state = None
            # We do not need to reset the actual output state since its value depends only on the input state
            self.reset_result_of_execution(reset_actual_output_state=False)
        else:
            if self.expected_output_state is None:
                self.expected_output_state = syrec.n_bit_values_container(self.input_state.size())
            for i in range(self.expected_output_state.size()):
                self.expected_output_state.set(i, edited_expected_output_state.test(i))  # type: ignore[arg-type]
            # We do not need to reset the actual output state since its value depends only on the input state
            self.reset_result_of_execution(reset_actual_output_state=False)

    @staticmethod
    def do_output_states_match(
        expected_output_state: syrec.n_bit_values_container | None, actual_output_state: syrec.n_bit_values_container
    ) -> bool | None:
        if expected_output_state is None:
            return None

        if expected_output_state.size() != actual_output_state.size():
            msg = f"Expected output state to have {expected_output_state.size()} qubits but actual output state contained {actual_output_state.size()} qubits!"
            log_error_to_console(msg, num_additionally_skipped_stack_frames_starting_from_caller_function=1)
            raise ValueError(msg)

        return all(
            actual_output_state.test(i) == expected_output_state.test(i) for i in range(actual_output_state.size())
        )

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
    ) -> None:
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
            internal_qubit_label: str | None = annotatable_quantum_computation.get_qubit_label(
                qreg.start, syrec.qubit_label_type.internal
            )
            if qreg.size == 0 or QtSimulationRunModel._does_qubit_label_start_with_internal_qubit_label_prefix(
                internal_qubit_label if internal_qubit_label is not None else ""
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
        if 0 <= index < len(self.simulation_run_models):
            return self.simulation_run_models[index]
        return None

    def get_all_simulation_run_models(self) -> Iterable[SimulationRunModel]:
        yield from self.simulation_run_models

    def add_simulation_run_model(self, simulation_run_model: SimulationRunModel) -> bool:
        n_simulation_runs: int = len(self.simulation_run_models)
        self.beginInsertRows(QtCore.QModelIndex(), n_simulation_runs, n_simulation_runs)
        self.simulation_run_models.append(simulation_run_model)
        self.endInsertRows()
        return True

    def add_simulation_run_models(self, to_be_added_simulation_run_models: list[SimulationRunModel]) -> None:
        if len(to_be_added_simulation_run_models) == 0:
            return

        idx_of_first_new_sim_run_model: int = len(self.simulation_run_models)
        idx_of_last_new_sim_run_model: int = idx_of_first_new_sim_run_model + len(to_be_added_simulation_run_models) - 1
        self.beginInsertRows(QtCore.QModelIndex(), idx_of_first_new_sim_run_model, idx_of_last_new_sim_run_model)
        self.simulation_run_models.extend(to_be_added_simulation_run_models)
        self.endInsertRows()

    def delete_simulation_run_model(self, index: QtCore.QModelIndex) -> bool:
        if not index.isValid():
            return False

        self.beginRemoveRows(QtCore.QModelIndex(), index.row(), index.row())
        self.simulation_run_models.pop(index.row())
        self.endRemoveRows()
        return True

    def delete_all_simulation_run_models(self) -> None:
        self.beginResetModel()
        self.simulation_run_models.clear()
        self.endResetModel()

    def reset_prev_simulation_run_execution_results(self) -> None:
        if self.rowCount(QtCore.QModelIndex()) == 0:
            return

        for sim_run_model in self.simulation_run_models:
            sim_run_model.reset_result_of_execution()
        self.dataChanged.emit(self.createIndex(0, 0), self.createIndex(len(self.simulation_run_models) - 1, 0))

    def update_edited_simulation_run_model(
        self, index: QtCore.QModelIndex, updated_simulation_run_data: SimulationRunModel
    ) -> None:
        if not self.is_model_index_valid(index):
            msg = "Invalid model index!"
            log_error_to_console(msg, num_additionally_skipped_stack_frames_starting_from_caller_function=1)
            raise ValueError(msg)

        self.simulation_run_models[index.row()].update_user_editable_data(
            updated_simulation_run_data.input_state, updated_simulation_run_data.expected_output_state
        )
        self.dataChanged.emit(index, index)

    def update_model_using_simulation_run_result(
        self,
        index: QtCore.QModelIndex,
        actual_output_state: syrec.n_bit_values_container,
        do_expected_and_actual_output_states_match: bool | None,
        execution_runtime_in_ms: float,
    ) -> None:
        if not self.is_model_index_valid(index):
            msg = "Invalid model index!"
            log_error_to_console(msg, num_additionally_skipped_stack_frames_starting_from_caller_function=1)
            raise ValueError(msg)

        self.simulation_run_models[index.row()].set_result_of_simulation_execution(
            actual_output_state, do_expected_and_actual_output_states_match, execution_runtime_in_ms
        )
        self.dataChanged.emit(index, index)

    def is_model_index_valid(self, index: QtCore.QModelIndex) -> bool:
        return index.isValid() and index.row() >= 0 and index.row() < len(self.simulation_run_models)  # type: ignore[no-any-return]
