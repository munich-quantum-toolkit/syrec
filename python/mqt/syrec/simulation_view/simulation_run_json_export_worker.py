# Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
# Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

from __future__ import annotations

import json
import time
from typing import TYPE_CHECKING, Any

from PyQt6 import QtCore

from .qt_simulation_run_model import SimulationRunModel

if TYPE_CHECKING:
    from collections.abc import Iterable
    from pathlib import Path


class SimulationRunJsonExportWorker(QtCore.QObject):  # type: ignore[misc]
    batch_exported = QtCore.pyqtSignal(tuple, name="batchExported")
    export_failed = QtCore.pyqtSignal(Exception, name="exportFailed")
    export_finished = QtCore.pyqtSignal(name="exportFinished")

    def __init__(self):
        super().__init__()

        self.cancellation_requested = False
        self.cancellation_flag_mutex = QtCore.QReadWriteLock()

    # TODO: Pretty printing
    # TODO: This untyped decorator should not be invocable via a signal?
    @QtCore.pyqtSlot()  # type: ignore[untyped-decorator]
    def start_export(
        self,
        path_to_json_file: Path,
        simulation_runs_to_export: Iterable[SimulationRunModel],
        export_batch_size: int,
    ) -> None:
        if export_batch_size < 1:
            return

        n_generated_batches: int = 0
        try:
            batch_idx: int = 0
            with path_to_json_file.open("w", encoding="utf-8") as file:
                # file.write("{\n\t\"simulationRuns\": [\n")
                file.write('{"simulationRuns":[')
                batch_export_start_time: float = time.perf_counter()
                batch_export_end_time: float = 0
                batch_export_duration: float = 0
                for sim_run in simulation_runs_to_export:
                    if self._thread_safe_check_whether_cancellation_is_requested():
                        break

                    # if batch_idx > 0:
                    #     file.write(",\n")
                    # file.write(json.dumps(sim_run, default=SimulationRunJsonExportWorker.serialize_to_json, indent=2))

                    if batch_idx > 0 or (batch_idx == 0 and n_generated_batches > 0):
                        file.write(",")
                    file.write(json.dumps(sim_run, default=SimulationRunJsonExportWorker.serialize_to_json))

                    batch_idx += 1
                    if batch_idx == export_batch_size:
                        batch_export_end_time = time.perf_counter()
                        batch_export_duration = batch_export_end_time - batch_export_start_time
                        batch_export_start_time = batch_export_end_time
                        self.batch_exported.emit((batch_export_duration, export_batch_size))
                        batch_idx = 0
                        n_generated_batches += 1
                # file.write("\n\t]\n}")
                file.write("]}")

            if batch_idx > 0 and not self._thread_safe_check_whether_cancellation_is_requested():
                batch_export_end_time = time.perf_counter()
                batch_export_duration = batch_export_end_time - batch_export_start_time
                self.batch_exported.emit((batch_export_duration, batch_idx))
            self.export_finished.emit()
        except Exception as err:
            self.export_failed.emit(err)
        return

    def request_cancellation(self) -> None:
        self._thread_safe_set_cancellation_requested_flag(True)

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
    def serialize_to_json(obj: Any) -> object:
        if isinstance(obj, SimulationRunModel):
            if obj.expected_output_state is None:
                return {"in": str(obj.input_state)}
            return {"in": str(obj.input_state), "out": str(obj.expected_output_state)}
        msg = f"Cannot serialize object of {type(obj)}"
        raise TypeError(msg)
