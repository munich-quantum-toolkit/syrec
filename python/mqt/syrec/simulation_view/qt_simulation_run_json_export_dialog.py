# Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
# Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

from __future__ import annotations

from typing import TYPE_CHECKING, Final

from PyQt6 import QtCore, QtWidgets

if TYPE_CHECKING:
    from collections.abc import Iterable
    from pathlib import Path

    from PyQt6 import QtGui

    from .qt_simulation_run_model import SimulationRunModel
    from .simulation_run_json_export_worker import SimulationRunJsonExportWorker
from .simulation_run_json_export_worker import SimulationRunJsonExportWorker

AGGREGATE_EXPORT_DATA_TEXT_FORMAT: Final[str] = (
    "Exported simulation runs: {num_exported_simulation_runs:d} | Total runtime for simulation run export [in seconds]: {total_runtime_in_sec:f}"
)
EXPORT_LOCATION_INFO_TEXT_FORMAT: Final[str] = "Exporting simulation runs to file {path_to_json_file:s}"
EXPORTED_BATCH_PROGRESS_INFO_TEXT_FORMAT: Final[str] = (
    "Finished export of simulation runs batch to file (runtime [in sec]: {batch_export_duration_in_seconds:f}!"
)


class SimulationRunJsonExportDialog(QtWidgets.QDialog):  # type: ignore[misc]
    def __init__(self, parent: QtWidgets.QWidget):
        super().__init__(parent)

        self.worker_thread: QtCore.QThread | None = None
        self.worker: SimulationRunJsonExportWorker | None = None

        self.num_exported_simulation_runs: int = 0
        self.total_sim_run_export_duration_in_secs: float = 0

        self.setModal(True)
        self.setSizeGripEnabled(True)
        self.setWindowTitle("Exporting simulation runs...")
        left = 0
        top = 0
        width = 400
        height = 200
        self.setGeometry(left, top, width, height)

        main_layout = QtWidgets.QVBoxLayout()
        self.export_location_info_lbl = QtWidgets.QLabel("")
        self.export_location_info_lbl.setAlignment(QtCore.Qt.AlignmentFlag.AlignCenter)

        self.progress_text_lbl = QtWidgets.QLabel("")
        self.progress_text_lbl.setAlignment(QtCore.Qt.AlignmentFlag.AlignCenter)
        self.progress_text_lbl.setStyleSheet("QLabel { color : gray; }")

        self.err_text_lbl = QtWidgets.QLabel("")
        self.err_text_lbl.setAlignment(QtCore.Qt.AlignmentFlag.AlignCenter)
        self.err_text_lbl.setStyleSheet("QLabel { color : red; }")

        main_layout.addWidget(self.export_location_info_lbl)
        main_layout.addWidget(self.progress_text_lbl)
        main_layout.addWidget(self.err_text_lbl)
        main_layout.addStretch()

        self.progress_bar = QtWidgets.QProgressBar()
        self.progress_bar.setFormat("Exported simulation run %v of %m")
        self.progress_bar.setAlignment(QtCore.Qt.AlignmentFlag.AlignCenter)

        self.total_runtime_text_lbl = QtWidgets.QLabel("")
        self.total_runtime_text_lbl.setAlignment(QtCore.Qt.AlignmentFlag.AlignCenter)
        main_layout.addWidget(self.total_runtime_text_lbl)

        self.dialog_button_box = QtWidgets.QDialogButtonBox(
            QtWidgets.QDialogButtonBox.StandardButton.Ok | QtWidgets.QDialogButtonBox.StandardButton.Cancel
        )
        self.dialog_button_box.setCenterButtons(True)
        self.dialog_button_box.rejected.connect(self._handle_export_to_file_cancel_button_click)
        self.dialog_button_box.accepted.connect(self.accept)

        self._change_dialog_ok_button_enable_state(False)
        self._change_dialog_cancellation_button_enable_state(False)

        main_layout.addWidget(self.dialog_button_box)
        self.setLayout(main_layout)

    def start_export(
        self,
        export_location: Path,
        sim_runs_to_export: Iterable[SimulationRunModel],
        num_sim_runs_to_export: int,
        batch_size: int = 500,
    ) -> None:
        self.export_location_info_lbl.setText(
            EXPORT_LOCATION_INFO_TEXT_FORMAT.format(path_to_json_file=str(export_location))
        )
        self.progress_bar.setMaximum(num_sim_runs_to_export)

        # To avoid redundant comments we refer to the SimulationRunJsonImportDialog.start_import(...) function for details regarding the worker-object to perform a long running operation
        self.worker = SimulationRunJsonExportWorker(export_location, sim_runs_to_export, batch_size)
        self.worker_thread = QtCore.QThread()
        self.worker.moveToThread(self.worker_thread)
        self.worker.batchCompleted.connect(self._handle_batch_exported, QtCore.Qt.ConnectionType.QueuedConnection)
        self.worker.finished.connect(self._handle_export_completion, QtCore.Qt.ConnectionType.QueuedConnection)
        self.worker.failed.connect(self._handle_export_failure, QtCore.Qt.ConnectionType.QueuedConnection)

        self.worker_thread.started.connect(
            self.worker.start_export,
            QtCore.Qt.ConnectionType.QueuedConnection,
        )
        self.worker_thread.finished.connect(self.worker_thread.deleteLater)
        self.worker_thread.finished.connect(self._reset_workers)
        self.worker_thread.start(QtCore.QThread.Priority.LowPriority)
        self._change_dialog_cancellation_button_enable_state(True)

    def closeEvent(self, event: QtGui.QCloseEvent) -> None:  # noqa: N802
        # Ask for confirmation before closing
        if self.worker is None or self._handle_export_to_file_cancel_button_click():
            if not self.err_text_lbl.text():
                self.accept()
            else:
                self.reject()
        else:
            event.ignore()

    @QtCore.pyqtSlot(Exception)  # type: ignore[untyped-decorator]
    def _handle_export_failure(self, err: Exception) -> None:
        self.progress_text_lbl.setText("")
        self.progress_bar.setVisible(False)

        self.err_text_lbl.setText(f"Unexpected {err=}, {type(err)=} during export of simulation runs")
        self._request_worker_cancellation()
        self._await_worker_thread_completion()
        self._change_dialog_cancellation_button_enable_state(False)

    @QtCore.pyqtSlot(float, object)  # type: ignore[untyped-decorator]
    def _handle_batch_exported(self, batch_generation_duration_in_seconds: float, batch_data: object) -> None:
        if not isinstance(batch_data, int):
            # TODO: Error logging?
            # TODO: Cancel worker?
            return

        self.progress_text_lbl.setText(
            EXPORTED_BATCH_PROGRESS_INFO_TEXT_FORMAT.format(
                batch_export_duration_in_seconds=batch_generation_duration_in_seconds
            )
        )
        self.num_exported_simulation_runs += batch_data
        self.progress_bar.setValue(self.num_exported_simulation_runs)

        self.total_sim_run_export_duration_in_secs += batch_generation_duration_in_seconds
        self.progress_text_lbl.setText(
            AGGREGATE_EXPORT_DATA_TEXT_FORMAT.format(
                num_exported_simulation_runs=self.num_exported_simulation_runs,
                total_runtime_in_sec=self.total_sim_run_export_duration_in_secs,
            )
        )

    @QtCore.pyqtSlot(bool)  # type: ignore[untyped-decorator]
    def _handle_export_completion(self, was_cancellation_requested: bool) -> None:
        self.progress_bar.setVisible(False)

        if not was_cancellation_requested:
            self._request_worker_cancellation()
            self._await_worker_thread_completion()

        self._change_dialog_cancellation_button_enable_state(False)
        self._change_dialog_ok_button_enable_state(True)

    def _request_worker_cancellation(self) -> None:
        self.progress_text_lbl.setText("Requesting cancellation of simulation run importer!")
        if self.worker is not None:
            self.worker.request_cancellation()

    def _change_dialog_cancellation_button_enable_state(self, should_button_be_enabled: bool) -> None:
        dialog_cancel_button: QtWidgets.QPushButton | None = self.dialog_button_box.button(
            QtWidgets.QDialogButtonBox.StandardButton.Cancel
        )

        if dialog_cancel_button is None:
            return

        dialog_cancel_button.setEnabled(should_button_be_enabled)

    def _change_dialog_ok_button_enable_state(self, should_button_be_enabled: bool) -> None:
        dialog_ok_button: QtWidgets.QPushButton | None = self.dialog_button_box.button(
            QtWidgets.QDialogButtonBox.StandardButton.Ok
        )

        if dialog_ok_button is None:
            return

        dialog_ok_button.setEnabled(should_button_be_enabled)

    def _reset_workers(self) -> None:
        self.worker_thread = None
        self.worker = None

    def _await_worker_thread_completion(self) -> None:
        if self.worker_thread is not None:
            self.worker_thread.quit()
            self.worker_thread.wait()
            self.progress_text_lbl.setText("Simulation run exporter thread finished!")

    @QtCore.pyqtSlot()  # type: ignore[untyped-decorator]
    def _handle_export_to_file_cancel_button_click(self) -> bool:
        clicked_button_in_confirmation_dialog: QtWidgets.QMessageBox.StandardButton = QtWidgets.QMessageBox.warning(
            self,
            "Cancellation of export to json file!",
            "Are you sure that you want to stop the export of simulation runs to the .json file?",
            buttons=QtWidgets.QMessageBox.StandardButton.Ok | QtWidgets.QMessageBox.StandardButton.Cancel,
            defaultButton=QtWidgets.QMessageBox.StandardButton.Ok,
        )

        if clicked_button_in_confirmation_dialog == QtWidgets.QMessageBox.StandardButton.Ok:
            self._request_worker_cancellation()
            return True
        return False
