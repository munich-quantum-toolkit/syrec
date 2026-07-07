# Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
# Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

from __future__ import annotations

import sys
from typing import TYPE_CHECKING, Any, Final

if sys.version_info >= (3, 12):
    from typing import override
else:
    from typing_extensions import override

from ...logger_utils import log_error_to_console, log_info_to_console

try:
    # The fastest of the supported parser backends according to the documentation (https://pypi.org/project/ijson/#toc-entry-15)
    # Requires that the pre-built python wheel for the yajl c-extension exists for the platform that executed this python script.
    # This should be the case for the majority of all platforms.
    import ijson.backends.yajl2_c as ijson
except ImportError:
    log_error_to_console("yajl2 C-extension not available, falling back to pure-Python parser!")
    # pure-Python fallback is always present but might not be the fastest
    import ijson

from PyQt6 import QtCore

from mqt.syrec import NBitValuesContainer

from ..simulation_run_model import SimulationRunModel
from .cancellable_worker_variants import CancellableProducerWorker

if TYPE_CHECKING:
    from pathlib import Path

    from .cancellable_worker_variants import BatchTimestamps, QueueConfig

SIMULATION_RUNS_JSON_KEY: Final[str] = "simulationRuns"
INPUT_STATE_JSON_KEY: Final[str] = "in"
EXPECTED_OUTPUT_STATE_JSON_KEY: Final[str] = "out"


class SimulationRunJsonImportWorker(CancellableProducerWorker[SimulationRunModel]):
    def __init__(
        self,
        path_to_json_file: Path,
        expected_n_data_qubits: int,
        expected_max_n_qubits: int,
        worker_send_queue_config: QueueConfig[SimulationRunModel],
    ) -> None:
        super().__init__(worker_send_queue_config)
        self._path_to_json_file: Final[Path] = path_to_json_file
        self._expected_n_data_qubits: Final[int] = expected_n_data_qubits
        self._expected_max_n_qubits: Final[int] = expected_max_n_qubits

    @QtCore.pyqtSlot()
    def start_import(self) -> None:
        batch_start_timestamp: float = SimulationRunJsonImportWorker.get_timestamp()
        batch_timestamps: BatchTimestamps | None = None

        try:
            self._assert_valid_user_provided_parameter_values()

            n_remaining_input_states_to_import_in_batch: int = self._send_queue_batch_size
            # Reading bytes instead of strings leads to better parser performance
            with self._path_to_json_file.open("rb") as file:
                # The json parser starts at the first element matching the prefix which in our case starts at an element with key 'simulationRuns' that is expected
                # to be a property of the top level element (i.e. the path to the 'simulationRuns' element is relative to the top level element).
                # Additionally, with the postfix '.item', only the entries of a JSON array are processed. If the 'simulationRuns' entry value is no
                # array then no objects will be parsed.
                parser = ijson.items(file, prefix=f"{SIMULATION_RUNS_JSON_KEY}.item")  # ty: ignore[unresolved-attribute]
                for arr_elem in parser:
                    # the ison.items(...) function converts JSON objects to python dictionaries (https://pypi.org/project/ijson/#options). However, we
                    # need to discard any other type of JSON elements (integers, strings, array, etc.) by checking whether we are actually processing a
                    # python dictionary.
                    if not isinstance(arr_elem, dict):
                        log_info_to_console(
                            f"Expected parsed simulation run JSON array element to be returned as python dictionary from third-party library but its python type was {type(arr_elem)}"
                        )
                        continue

                    self.send_queue.put_nowait(
                        SimulationRunJsonImportWorker._try_deserialize_simulation_run(
                            self._expected_n_data_qubits, self._expected_max_n_qubits, arr_elem
                        )
                    )
                    n_remaining_input_states_to_import_in_batch -= 1
                    if self.is_cancellation_requested():
                        break

                    if n_remaining_input_states_to_import_in_batch > 0:
                        continue

                    batch_timestamps = (
                        SimulationRunJsonImportWorker.calc_batch_duration_and_return_end_timestamp_in_seconds(
                            batch_start_timestamp
                        )
                    )
                    self.batchCompleted.emit(batch_timestamps.duration)
                    batch_start_timestamp = batch_timestamps.end

                    n_remaining_input_states_to_import_in_batch = self._send_queue_batch_size
                    self._wait_on_cancellation_or_input_data()

                # If we reached the end of the input .json file without reaching our batch threshold
                # emit the current enqueued elements to the consumer.
                if n_remaining_input_states_to_import_in_batch < self._send_queue_batch_size:
                    batch_timestamps = (
                        SimulationRunJsonImportWorker.calc_batch_duration_and_return_end_timestamp_in_seconds(
                            batch_start_timestamp
                        )
                    )
                    self.batchCompleted.emit(batch_timestamps.duration)
                self.finished.emit(self.is_cancellation_requested())
        except Exception as error:  # noqa: BLE001
            self_raised_error_msg = f"Error in simulaton run import worker! Reason: {type(error)=}, {error=}"
            log_error_to_console(self_raised_error_msg)
            self.failed.emit(error)

    @override
    def _assert_valid_user_provided_parameter_values(self) -> None:
        super()._assert_valid_user_provided_parameter_values()
        if self._expected_n_data_qubits < 1:
            msg = f"Expected the number of data qubits in the quantum computation to be a positive integer but was actually {self._expected_n_data_qubits}!"
            raise ValueError(msg)

        if self._expected_max_n_qubits < 1:
            msg = f"Expected the total number of qubits in the quantum computation to be a positive integer but was actually {self._expected_max_n_qubits}!"
            raise ValueError(msg)

        if self._expected_n_data_qubits > self._expected_max_n_qubits:
            msg = f"Expected the number of data qubits ({self._expected_n_data_qubits}) to be smaller or equal to the number of total qubits ({self._expected_max_n_qubits}) in the quantum computation!"
            raise ValueError(msg)

    @staticmethod
    def _try_deserialize_simulation_run(
        expected_n_data_qubits: int,
        max_num_qubits_in_input_or_output_state: int,
        parsed_json_elem_values_dict: dict[str, Any],
    ) -> SimulationRunModel:
        if INPUT_STATE_JSON_KEY not in parsed_json_elem_values_dict:
            msg = f"Values of input state (expected json key '{INPUT_STATE_JSON_KEY}') was not defined in json object!"
            raise ValueError(msg)

        raw_input_state_json_value: Final[Any] = parsed_json_elem_values_dict[INPUT_STATE_JSON_KEY]
        if not isinstance(raw_input_state_json_value, str):
            msg = f"Expected input state (expected json key '{INPUT_STATE_JSON_KEY}') to be defined as a string but was actually {type(raw_input_state_json_value)}"
            raise TypeError(msg)

        length_of_imported_input_state: Final[int] = len(raw_input_state_json_value)
        if length_of_imported_input_state not in {expected_n_data_qubits, max_num_qubits_in_input_or_output_state}:
            msg = f"Parsed input state size (n={length_of_imported_input_state}) must either match the number of data qubits ({expected_n_data_qubits}) or the total number of qubits {max_num_qubits_in_input_or_output_state} in the associated quantum computation!"
            raise ValueError(msg)

        if any(qubit_value not in {"0", "1"} for qubit_value in raw_input_state_json_value):
            msg = f"Qubit values of input state must be defined as an enumeration of '0' and '1' literals combined without any delimiter (i.e. a 4 qubit state must be defined as '0101') but was actually {raw_input_state_json_value}"
            raise ValueError(msg)

        expected_output_state: NBitValuesContainer | None = None
        raw_expected_output_state: Final[Any | None] = parsed_json_elem_values_dict.get(EXPECTED_OUTPUT_STATE_JSON_KEY)
        if raw_expected_output_state is not None:
            if not isinstance(raw_expected_output_state, str):
                msg = f"Expected output state (expected json key '{EXPECTED_OUTPUT_STATE_JSON_KEY}') to be defined as a string but was actually {type(raw_expected_output_state)}"
                raise TypeError(msg)

            length_of_imported_output_state: Final[int] = len(raw_expected_output_state)
            if length_of_imported_output_state not in {expected_n_data_qubits, max_num_qubits_in_input_or_output_state}:
                msg = f"Parsed output state size (n={length_of_imported_output_state}) must either match the number of data qubits ({expected_n_data_qubits}) or the total number of qubits {max_num_qubits_in_input_or_output_state} in the associated quantum computation!"
                raise ValueError(msg)

            if length_of_imported_output_state != length_of_imported_input_state:
                msg = f"Size of imported output state (n={length_of_imported_output_state}) must match the size of the imported input state (n={length_of_imported_input_state})!"
                raise ValueError(msg)

            if any(qubit_value not in {"0", "1"} for qubit_value in raw_expected_output_state):
                msg = f"Qubit values of expected output state must be defined as an enumeration of '0' and '1' literals combined without any delimiter (i.e. a 4 qubit state must be defined as '0101') but was actually {raw_expected_output_state}"
                raise ValueError(msg)

            expected_output_state = NBitValuesContainer(max_num_qubits_in_input_or_output_state)
            for i in range(expected_n_data_qubits):
                expected_output_state.set(i, raw_expected_output_state[i] != "0")

        input_state: NBitValuesContainer = NBitValuesContainer(max_num_qubits_in_input_or_output_state)
        for i in range(expected_n_data_qubits):
            input_state.set(i, raw_input_state_json_value[i] != "0")

        return SimulationRunModel(
            input_state,
            expected_output_state,
            actual_output_state=None,
            n_data_qubits_in_state=expected_n_data_qubits,
            create_new_n_bit_values_container_instances=False,
        )
