# Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
# Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

from __future__ import annotations

import sys
from typing import TYPE_CHECKING, Final, cast

if sys.version_info >= (3, 12):
    from typing import override
else:
    from typing_extensions import override

from PyQt6 import QtCore, QtWidgets

if TYPE_CHECKING:
    from collections.abc import Iterable
    from pathlib import Path

    from PyQt6 import QtGui

    from ..simulation_run_model import SimulationRunModel
from ...logger_utils import log_info_to_console
from ...message_box_utils import MessageBoxType, show_and_request_ok_in_optionally_cancellable_notification
from ..workers.simulation_run_json_export_worker import ExportedBatchData, SimulationRunJsonExportWorker
from .base_progress_dialog import BaseProgressDialog

EXPORTED_SIM_RUNS_DATA_LABEL: Final[str] = (
    "In total {n_exported_sim_runs:d} simulation runs were exported with {n_skipped_sim_runs:d} simulation runs being skipped"
)


class SimulationRunJsonExportDialog(BaseProgressDialog[SimulationRunJsonExportWorker]):
    def __init__(self, parent: QtWidgets.QWidget) -> None:
        super().__init__(
            parent,
            dialog_title="Exporting simulation runs...",
            optional_progress_bar_text_format="Processed simulation run %v of %m",
            create_default_layout=False,
        )
        self.num_processed_sim_runs: int = 0
        self.total_num_exported_sim_runs: int = 0
        self.total_num_skipped_sim_runs: int = 0
        self.dialog_button_box.accepted.connect(self.accept)
        self.dialog_button_box.rejected.connect(self._handle_export_to_file_cancel_button_click)

        self.export_location_info_lbl = QtWidgets.QLabel("")
        self.export_location_info_lbl.setAlignment(QtCore.Qt.AlignmentFlag.AlignCenter)

        self.exported_sim_runs_data_lbl = QtWidgets.QLabel("")
        self.exported_sim_runs_data_lbl.setAlignment(QtCore.Qt.AlignmentFlag.AlignCenter)
        self.exported_sim_runs_data_lbl.setStyleSheet("QLabel { color : gray; }")

        layout = QtWidgets.QVBoxLayout()
        layout.addWidget(self.title_lbl)
        layout.addWidget(self.export_location_info_lbl)
        layout.addWidget(self.progress_info_text_lbl)
        layout.addWidget(self.error_text_lbl)
        layout.addStretch()
        layout.addWidget(self.progress_bar)
        layout.addWidget(self.total_runtime_info_text_lbl)
        layout.addWidget(self.exported_sim_runs_data_lbl)
        layout.addWidget(self.dialog_button_box)
        self.setLayout(layout)

    def start_export(
        self,
        export_location: Path,
        associated_stringified_syrec_program: str,
        sim_runs_to_export: Iterable[SimulationRunModel],
        num_sim_runs_to_export: int,
        batch_size: int = 500,
    ) -> None:
        self.title_lbl.setText(f"Exporting simulation runs with batch size {batch_size}!")
        self.export_location_info_lbl.setText(f"Export destination: {export_location!s}")

        if self.progress_bar is not None:
            if not self._can_value_can_be_used_as_progress_bar_max_value(num_sim_runs_to_export):
                # We do not ask for confirmation to close the dialog since we faulted before the export started.
                super().reject()
                return

            self.progress_bar.setMinimum(0)
            self.progress_bar.setMaximum(num_sim_runs_to_export)
            self.progress_bar.setValue(0)
        else:
            show_and_request_ok_in_optionally_cancellable_notification(
                message_box_type=MessageBoxType.ERROR,
                message_box_parent=self,
                message_box_title="Required widget not found",
                message_box_content="Simulation run exporter was initialized without a progress bar! This should not happen.",
                is_cancellable=False,
            )

        # To avoid redundant comments we refer to the SimulationRunJsonImportDialog.start_import(...) function for details regarding the worker-object to perform a long running operation
        self.worker = SimulationRunJsonExportWorker(
            export_location, associated_stringified_syrec_program, sim_runs_to_export, batch_size
        )
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

    # Pressing the ESC key will only close the dialog but not close it thus no closeEvent will be triggered.
    @override
    def reject(self) -> None:
        if self._handle_export_to_file_cancel_button_click():
            super().reject()

    @override
    def closeEvent(self, event: QtGui.QCloseEvent) -> None:
        # Ask for confirmation before closing
        if self._handle_export_to_file_cancel_button_click():
            if not self.error_text_lbl.text():
                self.accept()
            else:
                # Avoid requiring duplicate confirmation of close operation by calling reject() function of super class instead of overridden reject function.
                super().reject()
        else:
            event.ignore()

    @QtCore.pyqtSlot(Exception)  # type: ignore[untyped-decorator]
    def _handle_export_failure(self, err: Exception) -> None:
        self._handle_non_recoverable_error(err)

    @QtCore.pyqtSlot(float, object)  # type: ignore[untyped-decorator]
    def _handle_batch_exported(self, batch_generation_duration_in_seconds: float, batch_data: object) -> None:
        if not SimulationRunJsonExportWorker.is_batch_data_of_type(
            batch_data, ExportedBatchData, parent_widget_for_error_notification=self
        ):
            if self.worker is not None:
                self.worker.ack_batch_processed()
            return

        casted_batch_data: Final[ExportedBatchData] = cast("ExportedBatchData", batch_data)
        self.progress_info_text_lbl.setText(
            f"Batch completed! Exported {casted_batch_data.exported_sim_runs} and skipping {casted_batch_data.skipped_sim_runs} simulation runs. Runtime [in seconds]: {batch_generation_duration_in_seconds}"
        )
        self._accumulate_and_update_total_runtime(batch_generation_duration_in_seconds)
        self.num_processed_sim_runs += casted_batch_data.exported_sim_runs + casted_batch_data.skipped_sim_runs

        if self.progress_bar is not None:
            self.progress_bar.setValue(self.num_processed_sim_runs)

        self.total_num_exported_sim_runs += casted_batch_data.exported_sim_runs
        self.total_num_skipped_sim_runs += casted_batch_data.skipped_sim_runs
        self.exported_sim_runs_data_lbl.setText(
            EXPORTED_SIM_RUNS_DATA_LABEL.format(
                n_exported_sim_runs=self.total_num_exported_sim_runs, n_skipped_sim_runs=self.total_num_skipped_sim_runs
            )
        )

    @QtCore.pyqtSlot(bool)  # type: ignore[untyped-decorator]
    def _handle_export_completion(self, was_cancellation_requested: bool) -> None:
        self.progress_info_text_lbl.setText("Simulation run export finished!")
        log_info_to_console("Simulation run export finished!")

        if self.progress_bar is not None:
            self.progress_bar.setVisible(False)

        # Cancelling the long running operation through a click on the cancel button of the dialog will already request a shutdown of the worker
        # and its associated thread but the same operation also needs to be execute when the worker completes successfully. However, when cancellation
        # was already requested, skip this operation.
        if not was_cancellation_requested:
            self._request_worker_cancellation()
            self._shutdown_worker_thread_and_await_completion()

        self._change_dialog_cancel_button_enable_state(False)
        self._change_dialog_ok_button_enable_state(True)

    @QtCore.pyqtSlot()  # type: ignore[untyped-decorator]
    def _handle_export_to_file_cancel_button_click(self) -> bool:
        if self.worker is None:
            return True

        if show_and_request_ok_in_optionally_cancellable_notification(
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

        self._request_worker_cancellation()
        self._shutdown_worker_thread_and_await_completion()
