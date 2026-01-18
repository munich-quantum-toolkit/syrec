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

from .qt_simulation_run_model import SimulationRunModel

if TYPE_CHECKING:
    from pathlib import Path

INPUT_STATE_JSON_KEY: Final[str] = "in"
EXPECTED_OUTPUT_STATE_JSON_KEY: Final[str] = "out"


class SimulationRunJsonImportWorker(QtCore.QObject):  # type: ignore[misc]
    batch_imported = QtCore.pyqtSignal(tuple, name="batchImported")
    import_finished = QtCore.pyqtSignal(name="importFinished")
    import_cancelled = QtCore.pyqtSignal(name="importCancelled")
    import_failed = QtCore.pyqtSignal(Exception, name="importFailed")

    def __init__(self, path_to_json_file: Path, expected_input_state_size: int, batch_size: int):
        super().__init__()

        if expected_input_state_size < 0:
            msg = f"Expected input state size must be a positive integer but was actually {expected_input_state_size}!"
            raise ValueError(msg)

        if batch_size < 1:
            msg = f"Batch size must be larger than 0 but was actually {batch_size}"
            raise ValueError(msg)

        self.path_to_json_file: Path = path_to_json_file
        self.expected_input_state_size: Final[int] = expected_input_state_size
        self.batch_size: Final[int] = batch_size
        self.cancellation_requested = False
        self.cancellation_flag_mutex = QtCore.QReadWriteLock()
        self.wait_on_batch_processed_acknowledgement_condition = QtCore.QWaitCondition()

    @QtCore.pyqtSlot()  # type: ignore[untyped-decorator]
    def start_import(self) -> None:
        batch_generation_end_time: float = 0
        batch_generation_duration_in_seconds: float = 0

        batch_data: list[SimulationRunModel | None] = [None for _ in range(self.batch_size)]
        batch_idx: int = 0
        try:
            # Reading bytes instead of strings leads to better parser performance
            with self.path_to_json_file.open("rb") as file:
                batch_generation_start_time: float = time.perf_counter()
                # The json parser starts at the first element matching the prefix which in our case starts at an element with key 'simulationRuns' that is expected
                # to be a property of the top level element (i.e. the path to the 'simulationRuns' element is relative to the top level element).
                # Additionally, with the postfix '.item', only the entries of a JSON array are processed. If the 'simulationRuns' entry value is no
                # array then no objects will be parsed.
                parser = ijson.items(file, prefix="simulationRuns.item")
                for arr_elem in parser:
                    if self._thread_safe_check_whether_cancellation_is_requested():
                        break

                    # the ison.items(...) function converts JSON objects to python dictionaries (https://pypi.org/project/ijson/#options). However, we
                    # need to discard any other type of JSON elements (integers, strings, array, etc.) by checking whether we are actually processing a
                    # python dictionary.
                    if not isinstance(arr_elem, dict):
                        continue

                    batch_data[batch_idx] = SimulationRunJsonImportWorker._try_deserialize_simulation_run(
                        self.expected_input_state_size, arr_elem
                    )
                    batch_idx += 1
                    if batch_idx == self.batch_size:
                        batch_generation_end_time = time.perf_counter()
                        batch_generation_duration_in_seconds = batch_generation_end_time - batch_generation_start_time
                        batch_generation_start_time = batch_generation_end_time

                        self.batch_imported.emit((batch_generation_duration_in_seconds, batch_data))
                        try:
                            self.cancellation_flag_mutex.lockForRead()
                            # Lock needs to be already held for wait condition to not return immediately
                            self.wait_on_batch_processed_acknowledgement_condition.wait(self.cancellation_flag_mutex)
                        finally:
                            self.cancellation_flag_mutex.unlock()
                            # An artificial delay improves the responsiveness of the UI but does not seem like the best solution. However, using
                            # a delayed acknowledgement in the UI thread would increase the complexity of the implementation of the UI.
                            time.sleep(0.1)

                        for i in range(self.batch_size):
                            batch_data[i] = None
                        batch_idx = 0

                if batch_idx != 0:
                    del batch_data[batch_idx:]
                    batch_generation_end_time = time.perf_counter()
                    batch_generation_duration_in_seconds = batch_generation_end_time - batch_generation_start_time
                    batch_generation_start_time = batch_generation_end_time
                    self.batch_imported.emit((batch_generation_duration_in_seconds, batch_data))
            self.import_finished.emit()
        except Exception as err:
            self.import_failed.emit(err)

    # def write_streaming(self, source: Iterable[Any], chunk_size: int = 1_000):
    #     """
    #     Write large JSON array without blocking.
    #     source can be a generator (lazy).
    #     """
    #     try:
    #         total = None
    #         if hasattr(source, "__len__"):
    #             total = len(source)
    #         with self.file_path.open("w", encoding="utf-8") as f:
    #             f.write("[\n")
    #             for idx, item in enumerate(source):
    #                 if self.should_stop():
    #                     self.finished.emit(False)
    #                     return
    #                 if idx:
    #                     f.write(",\n")
    #                 json.dump(item, f, ensure_ascii=False)
    #                 if total:
    #                     self.progress.emit(int(100 * (idx + 1) / total))
    #                 else:
    #                     self.progress.emit(-1)  # indeterminate
    #             f.write("\n]")
    #         self.progress.emit(100)
    #         self.finished.emit(True)
    #     except Exception as exc:
    #         self.error.emit(str(exc))

    # Again we define the slot without the corresponding decorator, for further information we refer to the request_cancellation function.

    def request_cancellation(self) -> None:
        self._thread_safe_set_cancellation_requested_flag(True)
        self.wait_on_batch_processed_acknowledgement_condition.wakeAll()

    def ack_batch_processed(self) -> None:
        self.wait_on_batch_processed_acknowledgement_condition.wakeAll()

    def _thread_safe_check_whether_cancellation_is_requested(self) -> bool:
        cancellation_requested: bool = False
        self.cancellation_flag_mutex.lockForRead()
        cancellation_requested = self.cancellation_requested
        self.cancellation_flag_mutex.unlock()
        return cancellation_requested

    def _thread_safe_set_cancellation_requested_flag(self, flag_value: bool) -> None:
        self.cancellation_flag_mutex.lockForWrite()
        self.cancellation_requested = flag_value
        self.cancellation_flag_mutex.unlock()

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
