# Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
# Copyright (c) 2025 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

from __future__ import annotations

import time
from typing import Final

from PyQt6 import QtCore

from mqt import syrec

from .qt_simulation_run_model import SimulationRunModel

# input_state_batch_type = list[syrec.n_bit_values_container]
# QtCore.qRegisterMetaType(input_state_batch_type, "input_state_batch_type")


class AllInputStatesGeneratorWorker(QtCore.QObject):  # type: ignore[misc]
    batch_generated = QtCore.pyqtSignal(list, name="batchGenerated")
    generation_failed = QtCore.pyqtSignal(Exception, name="generationFailed")
    generation_cancelled = QtCore.pyqtSignal(name="generationCancelled")
    generation_finished = QtCore.pyqtSignal(float, name="generationFinished")

    def __init__(self, expected_input_state_size: int, batch_size: int):
        super().__init__()

        if expected_input_state_size < 0:
            msg = f"Expected input state size must be a positive integer but was actually {expected_input_state_size}!"
            raise ValueError(msg)

        if batch_size < 1:
            msg = f"Batch size must be larger than 0 but was actually {batch_size}"
            raise ValueError(msg)

        self.expected_input_state_size: Final[int] = expected_input_state_size
        self.batch_size: Final[int] = batch_size
        self.cancellation_requested = False

    @QtCore.pyqtSlot()  # type: ignore[untyped-decorator]
    def start_generation(self) -> None:
        n_states_to_generate: int = 2**self.expected_input_state_size
        n_batches: int = n_states_to_generate // self.batch_size

        batch_data: list[SimulationRunModel | None] = [None for i in range(self.batch_size)]
        integer_defining_input_state: int = 0

        generation_start_time: float = time.perf_counter()
        curr_batch_elem_count: int = 0
        generated_batches: int = 0
        try:
            for _ in range((n_batches * self.batch_size) + 1):
                if self.cancellation_requested:
                    break

                if curr_batch_elem_count == self.batch_size:
                    self.batch_generated.emit(batch_data)
                    generated_batches += 1
                    if generated_batches == n_batches:
                        break

                    curr_batch_elem_count = 0
                    for j in range(self.batch_size):
                        batch_data[j] = None

                batch_data[curr_batch_elem_count] = (
                    AllInputStatesGeneratorWorker._generate_sim_run_model_for_input_state(
                        self.expected_input_state_size, integer_defining_input_state
                    )
                )
                integer_defining_input_state += 1
                curr_batch_elem_count += 1

            n_elems_in_last_batch: int = n_states_to_generate % self.batch_size
            curr_batch_elem_count = 0
            if n_elems_in_last_batch != 0:
                # Truncate batch result container to size of last batch
                del batch_data[n_elems_in_last_batch:]
                for i in range(n_elems_in_last_batch):
                    if self.cancellation_requested:
                        break

                    batch_data[i] = AllInputStatesGeneratorWorker._generate_sim_run_model_for_input_state(
                        self.expected_input_state_size, integer_defining_input_state
                    )
                    integer_defining_input_state += 1
                self.batch_generated.emit(batch_data)
        except Exception as err:
            # TODO: Slot in dialog is missing positional argument err?
            self.generation_failed.emit(err)

        generation_end_time: float = time.perf_counter()
        total_generation_runtime: float = generation_end_time - generation_start_time
        self.generation_finished.emit(total_generation_runtime)

    @QtCore.pyqtSlot()  # type: ignore[untyped-decorator]
    def request_cancellation(self) -> None:
        self.cancellation_requested = True

    @staticmethod
    def _generate_sim_run_model_for_input_state(
        expected_input_state_size: int, integer_defining_input_state: int
    ) -> SimulationRunModel:
        binary_string_of_i = format(integer_defining_input_state, "b")
        input_state = syrec.n_bit_values_container(expected_input_state_size)

        n_qubits_to_process_in_binary_string: int = min(expected_input_state_size, len(binary_string_of_i))
        qubit_idx_in_binary_string: int = n_qubits_to_process_in_binary_string - 1
        for qubit in range(n_qubits_to_process_in_binary_string):
            qubit_value: bool = binary_string_of_i[qubit_idx_in_binary_string] == "1"
            input_state.set(qubit, qubit_value)
            qubit_idx_in_binary_string -= 1
        return SimulationRunModel(input_state, expected_output_state=None)
