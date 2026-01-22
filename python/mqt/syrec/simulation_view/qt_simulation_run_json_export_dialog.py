# Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
# Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

from __future__ import annotations

from typing import TYPE_CHECKING

from PyQt6 import QtCore, QtWidgets

if TYPE_CHECKING:
    from collections.abc import Iterable
    from pathlib import Path

    from PyQt6 import QtGui

    from .qt_simulation_run_model import SimulationRunModel
    from .simulation_run_json_export_worker import SimulationRunJsonExportWorker
from ..logger_utils import log_info_to_console
from ..message_box_utils import MessageBoxType, show_optionally_cancellable_notification
from .dialogs.base_progress_dialog import BaseProgressDialog
from .simulation_run_json_export_worker import SimulationRunJsonExportWorker


class SimulationRunJsonExportDialog(BaseProgressDialog[SimulationRunJsonExportWorker]):
    def __init__(self, parent: QtWidgets.QWidget):
        super().__init__(
            parent,
            dialog_title="Exporting simulation runs...",
            optional_progress_bar_text_format="Exported simulation run %v of %m",
            create_default_layout=False,
        )
        self.num_exported_simulation_runs: int = 0
        self.dialog_button_box.accepted.connect(self.accept)
        self.dialog_button_box.rejected.connect(self._handle_export_to_file_cancel_button_click)

        self.export_location_info_lbl = QtWidgets.QLabel("")
        self.export_location_info_lbl.setAlignment(QtCore.Qt.AlignmentFlag.AlignCenter)

        layout = QtWidgets.QVBoxLayout()
        layout.addWidget(self.title_lbl)
        layout.addWidget(self.export_location_info_lbl)
        layout.addWidget(self.progress_info_text_lbl)
        layout.addWidget(self.error_text_lbl)
        layout.addStretch()
        layout.addWidget(self.progress_bar)
        layout.addWidget(self.total_runtime_info_text_lbl)
        layout.addWidget(self.dialog_button_box)
        self.setLayout(layout)

    def start_export(
        self,
        export_location: Path,
        sim_runs_to_export: Iterable[SimulationRunModel],
        num_sim_runs_to_export: int,
        batch_size: int = 500,
    ) -> None:
        self.title_lbl.setText(f"Exporting simulation runs with batch size {batch_size}!")
        self.export_location_info_lbl.setText(f"Export destination: {export_location!s}")

        if self.progress_bar is not None:
            self.progress_bar.setMinimum(0)
            self.progress_bar.setMaximum(num_sim_runs_to_export)
            self.progress_bar.setValue(0)
        else:
            show_optionally_cancellable_notification(
                message_box_type=MessageBoxType.ERROR,
                message_box_parent=self,
                message_box_title="Required widget not found",
                message_box_content="Simulation run exporter was initialized without a progress bar! This should not happen.",
                is_cancellable=False,
            )

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
        self._change_dialog_cancel_button_enable_state(True)

    def closeEvent(self, event: QtGui.QCloseEvent) -> None:  # noqa: N802
        # Ask for confirmation before closing
        if self._handle_export_to_file_cancel_button_click():
            if not self.error_text_lbl.text():
                self.accept()
            else:
                self.reject()
        else:
            event.ignore()

    @QtCore.pyqtSlot(Exception)  # type: ignore[untyped-decorator]
    def _handle_export_failure(self, err: Exception) -> None:
        self._handle_non_recoverable_error(err)

    @QtCore.pyqtSlot(float, object)  # type: ignore[untyped-decorator]
    def _handle_batch_exported(self, batch_generation_duration_in_seconds: float, batch_data: object) -> None:
        if not isinstance(batch_data, int):
            # TODO: Better logging of mismatched type/s in list
            show_optionally_cancellable_notification(
                message_box_type=MessageBoxType.INFO,
                message_box_parent=self,
                message_box_title="Cannot handle batch data",
                message_box_content=f"Expected batch data to be of type {type(int)} but was actually {type(batch_data)}! This should not happen.",
                is_cancellable=False,
            )
            if self.worker is not None:
                self.worker.ack_batch_processed()
            return

        self._update_progress_text_with_batch_info(batch_data, batch_generation_duration_in_seconds)
        self._accumulate_and_update_total_runtime(batch_generation_duration_in_seconds)
        self.num_exported_simulation_runs += batch_data

        if self.progress_bar is not None:
            self.progress_bar.setValue(self.num_exported_simulation_runs)

    @QtCore.pyqtSlot(bool)  # type: ignore[untyped-decorator]
    def _handle_export_completion(self, was_cancellation_requested: bool) -> None:
        self.progress_info_text_lbl.setText("Simulation run export finished!")
        log_info_to_console("Simulation run export finished!")

        if self.progress_bar is not None:
            self.progress_bar.setVisible(False)

        if not was_cancellation_requested:
            self._request_worker_cancellation()
            self._shutdown_worker_thread_and_await_completion()

        self._change_dialog_cancel_button_enable_state(False)
        self._change_dialog_ok_button_enable_state(True)

    @QtCore.pyqtSlot()  # type: ignore[untyped-decorator]
    def _handle_export_to_file_cancel_button_click(self) -> bool:
        if self.worker is None:
            return True

        if show_optionally_cancellable_notification(
            message_box_type=MessageBoxType.QUESTION,
            message_box_parent=self,
            message_box_title="Cancellation of export to json file!",
            message_box_content="Are you sure that you want to stop the export of simulation runs to the .json file? Already exported data will not be deleted.",
            is_cancellable=True,
            log_contents=False,
        ):
            log_info_to_console("Cancellation of simulation run export requested!")
            self._handle_non_recoverable_error(None)
            return True
        return False

    def _handle_non_recoverable_error(self, err: Exception | None) -> None:
        self.progress_info_text_lbl.setText("")
        if err is not None:
            self._update_displayed_error_text(err, num_additionally_skipped_stack_frames_starting_from_this_function=2)

        if self.worker is not None:
            self._request_worker_cancellation()
        if self.worker_thread is not None:
            self._shutdown_worker_thread_and_await_completion()
