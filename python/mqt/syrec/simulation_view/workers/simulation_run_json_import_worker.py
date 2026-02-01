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

# The fastest of the supported parser backends according to the documentation (https://pypi.org/project/ijson/#toc-entry-15)
# TODO: Try catch and fallback incase that import fails
import ijson.backends.yajl2_c as ijson
from PyQt6 import QtCore

from mqt import syrec

from ...logger_utils import log_error_to_console, log_info_to_console
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
        expected_input_state_size: int,
        worker_send_queue_config: QueueConfig[SimulationRunModel],
    ) -> None:
        super().__init__(worker_send_queue_config)
        self.path_to_json_file = path_to_json_file
        self.expected_input_state_size = expected_input_state_size

    @QtCore.pyqtSlot()  # type: ignore[untyped-decorator]
    def start_import(self) -> None:
        batch_start_timestamp: float = SimulationRunJsonImportWorker.get_timestamp()
        batch_timestamps: BatchTimestamps | None = None

        try:
            self._assert_valid_user_provided_parameter_values()

            n_remaining_input_states_to_import_in_batch: int = self.send_queue_batch_size
            # Reading bytes instead of strings leads to better parser performance
            with self.path_to_json_file.open("rb") as file:
                # The json parser starts at the first element matching the prefix which in our case starts at an element with key 'simulationRuns' that is expected
                # to be a property of the top level element (i.e. the path to the 'simulationRuns' element is relative to the top level element).
                # Additionally, with the postfix '.item', only the entries of a JSON array are processed. If the 'simulationRuns' entry value is no
                # array then no objects will be parsed.
                parser = ijson.items(file, prefix=f"{SIMULATION_RUNS_JSON_KEY}.item")
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
                            self.expected_input_state_size, arr_elem
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

                    n_remaining_input_states_to_import_in_batch = self.send_queue_batch_size
                    self._wait_on_cancellation_or_input_data()

                # If we reached the end of the input .json file without reaching our batch threshold
                # emit the current enqueued elements to the consumer.
                if n_remaining_input_states_to_import_in_batch < self.send_queue_batch_size:
                    batch_timestamps = (
                        SimulationRunJsonImportWorker.calc_batch_duration_and_return_end_timestamp_in_seconds(
                            batch_start_timestamp
                        )
                    )
                    self.batchCompleted.emit(batch_timestamps.duration)
                self.finished.emit(self.is_cancellation_requested())
        except Exception as error:
            self_raised_error_msg = f"Error in simulaton run import worker! Reason: {type(error)=}, {error=}"
            log_error_to_console(self_raised_error_msg)
            self.failed.emit(error)

    @override
    def _assert_valid_user_provided_parameter_values(self) -> None:
        super()._assert_valid_user_provided_parameter_values()
        if self.expected_input_state_size < 1:
            msg = f"Expected input state size must be a positive integer but was actually {self.expected_input_state_size}!"
            raise ValueError(msg)

    @staticmethod
    def _try_deserialize_simulation_run(
        expected_state_size: int, parsed_json_elem_values_dict: dict[str, Any]
    ) -> SimulationRunModel:
        if INPUT_STATE_JSON_KEY not in parsed_json_elem_values_dict:
            msg = f"Values of input state (expected json key '{INPUT_STATE_JSON_KEY}') was not defined in json object!"
            raise ValueError(msg)

        stringified_input_state: Final[str] = parsed_json_elem_values_dict[INPUT_STATE_JSON_KEY]
        if len(stringified_input_state) != expected_state_size:
            msg = f"Parsed input state size (n={len(stringified_input_state)}) did not match expected input state size (n={expected_state_size})!"
            raise ValueError(msg)

        if any(qubit_value not in {"0", "1"} for qubit_value in stringified_input_state):
            msg = f"Qubit values of input state must be defined as an enumeration of '0' and '1' literals combined without any delimiter (i.e. a 4 qubit state must be defined as '0101') but was actually {stringified_input_state}"
            raise ValueError(msg)

        stringified_expected_output_state: Final[str | None] = parsed_json_elem_values_dict.get(
            EXPECTED_OUTPUT_STATE_JSON_KEY
        )

        if stringified_expected_output_state is not None:
            if len(stringified_expected_output_state) != expected_state_size:
                msg = f"Parsed expected output state size (n={len(stringified_expected_output_state)}) did not match expected input state size (n={expected_state_size})!"
                raise ValueError(msg)
            if any(qubit_value not in {"0", "1"} for qubit_value in stringified_expected_output_state):
                msg = f"Qubit values of expected output state must be defined as an enumeration of '0' and '1' literals combined without any delimiter (i.e. a 4 qubit state must be defined as '0101') but was actually {stringified_expected_output_state}"
                raise ValueError(msg)

        input_state: syrec.n_bit_values_container = syrec.n_bit_values_container(expected_state_size)
        expected_output_state: syrec.n_bit_values_container | None = (
            syrec.n_bit_values_container(expected_state_size) if stringified_expected_output_state is not None else None
        )
        for i in range(expected_state_size):
            input_state.set(i, stringified_input_state[i] != "0")

        if expected_output_state is not None:
            for i in range(expected_state_size):
                expected_output_state.set(i, stringified_expected_output_state[i] != "0")  # type: ignore[index]

        return SimulationRunModel(input_state, expected_output_state)
