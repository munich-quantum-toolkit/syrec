# Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
# Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

from __future__ import annotations

import time
from typing import TYPE_CHECKING, Any, Final

# TODO: Correctly configure third-party package for mypy
# The fastest of the supported parser backends according to the documentation (https://pypi.org/project/ijson/#toc-entry-15)
import ijson.backends.yajl2_c as ijson  # type: ignore[import-not-found]
from PyQt6 import QtCore

from mqt import syrec

from ..logger_utils import log_error_to_console, log_info_to_console
from .cancellable_base_worker import CancellableBaseWorker
from .qt_simulation_run_model import SimulationRunModel

if TYPE_CHECKING:
    from pathlib import Path

    from .cancellable_base_worker import BatchTimestamps

SIMULATION_RUNS_JSON_KEY: Final[str] = "simulationRuns"
INPUT_STATE_JSON_KEY: Final[str] = "in"
EXPECTED_OUTPUT_STATE_JSON_KEY: Final[str] = "out"


class SimulationRunJsonImportWorker(CancellableBaseWorker):
    def __init__(self, path_to_json_file: Path, expected_input_state_size: int, batch_size: int):
        super().__init__(do_batches_require_ack=True)

        self.path_to_json_file = path_to_json_file
        self.expected_input_state_size = expected_input_state_size
        self.batch_size = batch_size

    @QtCore.pyqtSlot()  # type: ignore[untyped-decorator]
    def start_import(self) -> None:
        try:
            SimulationRunJsonImportWorker._validate_parameters(self.expected_input_state_size, self.batch_size)
            self_raised_error_msg: str | None = ""

            if self.wait_on_batch_processed_acknowledgement_condition is None:
                self_raised_error_msg = "Internal batch processed acknowledgement condition was not initialized"
                log_error_to_console(self_raised_error_msg)
                self.failed.emit(ValueError(self_raised_error_msg))
                return

            batch_idx: int = 0
            batch_data: list[SimulationRunModel | None] = [None for _ in range(self.batch_size)]

            # Reading bytes instead of strings leads to better parser performance
            with self.path_to_json_file.open("rb") as file:
                batch_start_timestamp: float = SimulationRunJsonImportWorker._get_timestamp()
                batch_timestamps: BatchTimestamps | None = None
                # The json parser starts at the first element matching the prefix which in our case starts at an element with key 'simulationRuns' that is expected
                # to be a property of the top level element (i.e. the path to the 'simulationRuns' element is relative to the top level element).
                # Additionally, with the postfix '.item', only the entries of a JSON array are processed. If the 'simulationRuns' entry value is no
                # array then no objects will be parsed.
                parser = ijson.items(file, prefix=f"{SIMULATION_RUNS_JSON_KEY}.item")
                for arr_elem in parser:
                    if self.is_cancellation_requested():
                        break

                    # the ison.items(...) function converts JSON objects to python dictionaries (https://pypi.org/project/ijson/#options). However, we
                    # need to discard any other type of JSON elements (integers, strings, array, etc.) by checking whether we are actually processing a
                    # python dictionary.
                    if not isinstance(arr_elem, dict):
                        log_info_to_console(
                            f"Expected parsed simulation run JSON array element to be returned as python dictionary from third-party library but its python type was {type(arr_elem)}"
                        )
                        continue

                    batch_data[batch_idx] = SimulationRunJsonImportWorker._try_deserialize_simulation_run(
                        self.expected_input_state_size, arr_elem
                    )
                    batch_idx += 1
                    if batch_idx < self.batch_size:
                        continue

                    batch_timestamps = (
                        SimulationRunJsonImportWorker._calc_batch_duration_and_return_end_timestamp_in_seconds(
                            batch_start_timestamp
                        )
                    )
                    batch_start_timestamp = batch_timestamps.end
                    self.batchCompleted.emit(batch_timestamps.duration, batch_data.copy())

                    with QtCore.QMutexLocker(self.batch_ack_mutex):
                        if not self.is_cancellation_requested():
                            self.wait_on_batch_processed_acknowledgement_condition.wait(self.batch_ack_mutex)
                    # An artificial delay improves the responsiveness of the UI but does not seem like the best solution. However, using
                    # a delayed acknowledgement in the UI thread would increase the complexity of the implementation of the UI.
                    time.sleep(0.1)

                    for i in range(self.batch_size):
                        batch_data[i] = None
                    batch_idx = 0

                if batch_idx != 0 and not self.is_cancellation_requested():
                    del batch_data[batch_idx:]
                    batch_timestamps = (
                        SimulationRunJsonImportWorker._calc_batch_duration_and_return_end_timestamp_in_seconds(
                            batch_start_timestamp
                        )
                    )
                    self.batchCompleted.emit(batch_timestamps.duration, batch_data.copy())
            self.finished.emit(self.cancellation_requested)
        except Exception as error:
            self_raised_error_msg = f"Error in simulaton run import worker! Reason: {type(error)=}, {error=}"
            log_error_to_console(self_raised_error_msg)
            self.failed.emit(error)

    @staticmethod
    def _validate_parameters(expected_input_state_size: int, batch_size: int) -> None:
        if expected_input_state_size < 0:
            msg = f"Expected input state size must be a positive integer but was actually {expected_input_state_size}!"
            raise ValueError(msg)

        if batch_size < 1:
            msg = f"Batch size must be larger than 0 but was actually {batch_size}"
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
