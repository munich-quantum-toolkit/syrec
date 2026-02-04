# Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
# Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

from __future__ import annotations

import queue
import sys
from typing import TYPE_CHECKING, Final

if sys.version_info >= (3, 12):
    from typing import override
else:
    from typing_extensions import override

from PyQt6 import QtCore, QtWidgets

if TYPE_CHECKING:
    from pathlib import Path

    from PyQt6 import QtGui

    from ..simulation_run_model import QtSimulationRunModel, SimulationRunModel
from ...logger_utils import log_info_to_console
from ...message_box_utils import MessageBoxType, show_and_request_ok_in_optionally_cancellable_notification
from ..workers.cancellable_worker_variants import QueueConfig
from ..workers.simulation_run_json_export_worker import ExportedBatchData, SimulationRunJsonExportWorker
from .base_progress_dialog import DEFAULT_SMALL_QUEUE_SIZE, DEFAULT_WORKER_CONTINUE_DELAY_IN_MS, BaseProgressDialog

EXPORTED_SIM_RUNS_DATA_LABEL: Final[str] = (
    "In total {n_exported_sim_runs:d} simulation runs were exported with {n_skipped_sim_runs:d} simulation runs being skipped"
)


class SimulationRunJsonExportDialog(BaseProgressDialog[SimulationRunJsonExportWorker]):
    def __init__(self, parent: QtWidgets.QWidget, shared_simulation_runs_model: QtSimulationRunModel) -> None:
        super().__init__(
            parent,
            shared_simulation_runs_model,
            dialog_title="Exporting simulation runs...",
            optional_progress_bar_text_format="Processed simulation run %v of %m",
            create_default_layout=False,
        )
        self._num_processed_sim_runs: int = 0
        self._total_num_exported_sim_runs: int = 0
        self._total_num_skipped_sim_runs: int = 0
        self._last_exported_sim_run_num: int = 0

        self._worker_recv_queue_batch_size: int = 0
        self._worker_send_queue: queue.SimpleQueue[ExportedBatchData] = queue.SimpleQueue()
        self._worker_recv_queue: queue.SimpleQueue[SimulationRunModel | None] = queue.SimpleQueue()

        self._dialog_button_box.accepted.connect(self.accept)
        self._dialog_button_box.rejected.connect(self._handle_export_to_file_cancel_button_click)

        self._export_location_info_lbl = QtWidgets.QLabel("")
        self._export_location_info_lbl.setAlignment(QtCore.Qt.AlignmentFlag.AlignCenter)

        self._exported_sim_runs_data_lbl = QtWidgets.QLabel("")
        self._exported_sim_runs_data_lbl.setAlignment(QtCore.Qt.AlignmentFlag.AlignCenter)
        self._exported_sim_runs_data_lbl.setStyleSheet("QLabel { color : gray; }")

        layout = QtWidgets.QVBoxLayout()
        layout.addWidget(self._title_lbl)
        layout.addWidget(self._export_location_info_lbl)
        layout.addWidget(self._progress_info_text_lbl)
        layout.addWidget(self._error_text_lbl)
        layout.addStretch()
        layout.addWidget(self._progress_bar)
        layout.addWidget(self._total_runtime_info_text_lbl)
        layout.addWidget(self._exported_sim_runs_data_lbl)
        layout.addWidget(self._dialog_button_box)
        self.setLayout(layout)

    def start_export(
        self,
        export_location: Path,
        associated_stringified_syrec_program: str,
        num_sim_runs_to_export: int,
        worker_recv_queue_batch_size: int = DEFAULT_SMALL_QUEUE_SIZE,
    ) -> None:
        self._title_lbl.setText(f"Exporting simulation runs with batch size {worker_recv_queue_batch_size}!")
        self._export_location_info_lbl.setText(f"Export destination: {export_location!s}")

        if worker_recv_queue_batch_size < 1 or num_sim_runs_to_export < 1:
            show_and_request_ok_in_optionally_cancellable_notification(
                message_box_type=MessageBoxType.ERROR,
                message_box_parent=self,
                message_box_title="Invalid input parameters detected",
                message_box_content=f"Expected worker receive queue batch size (value={worker_recv_queue_batch_size}) and number of expected simulation runs (value={num_sim_runs_to_export}) to be a positive integers!",
                is_cancellable=False,
            )
            super().reject()
            return

        if self._progress_bar is not None:
            if not self._can_value_can_be_used_as_progress_bar_max_value(num_sim_runs_to_export):
                # We do not ask for confirmation to close the dialog since we faulted before the export started.
                super().reject()
                return

            self._progress_bar.setMinimum(0)
            self._progress_bar.setMaximum(num_sim_runs_to_export)
            self._progress_bar.setValue(0)
        else:
            show_and_request_ok_in_optionally_cancellable_notification(
                message_box_type=MessageBoxType.ERROR,
                message_box_parent=self,
                message_box_title="Required widget not found",
                message_box_content="Simulation run exporter was initialized without a progress bar! This should not happen.",
                is_cancellable=False,
            )

        self._worker_recv_queue_batch_size = worker_recv_queue_batch_size
        # To avoid redundant comments we refer to the SimulationRunJsonImportDialog.start_import(...) function for details regarding the worker-object to perform a long running operation
        self._worker = SimulationRunJsonExportWorker(
            export_location,
            associated_stringified_syrec_program,
            worker_send_queue_config=QueueConfig(queue_instance=self._worker_send_queue, queue_batch_size=1),
            worker_recv_queue_config=QueueConfig(
                queue_instance=self._worker_recv_queue, queue_batch_size=self._worker_recv_queue_batch_size
            ),
        )
        self._worker_thread = QtCore.QThread()
        self._worker.moveToThread(self._worker_thread)
        self._worker.batchCompleted.connect(self._handle_batch_exported, QtCore.Qt.ConnectionType.QueuedConnection)
        self._worker.requestingData.connect(
            self._enqueue_next_simulation_runs_to_export, QtCore.Qt.ConnectionType.QueuedConnection
        )
        self._worker.finished.connect(self._handle_export_completion, QtCore.Qt.ConnectionType.QueuedConnection)
        self._worker.failed.connect(self._handle_export_failure, QtCore.Qt.ConnectionType.QueuedConnection)

        self._worker_thread.started.connect(
            self._worker.start_export,
            QtCore.Qt.ConnectionType.QueuedConnection,
        )
        self._worker_thread.finished.connect(self._worker_thread.deleteLater)
        self._worker_thread.finished.connect(self._reset_workers)
        self._worker_thread.start(QtCore.QThread.Priority.LowPriority)
        self._change_dialog_cancel_button_enable_state(True)
        self._enqueue_next_simulation_runs_to_export()

    # Pressing the ESC key will only close the dialog but not close it thus no closeEvent will be triggered.
    @override
    def reject(self) -> None:
        if self._handle_export_to_file_cancel_button_click():
            super().reject()

    @override
    def closeEvent(self, event: QtGui.QCloseEvent) -> None:
        # Ask for confirmation before closing
        if self._handle_export_to_file_cancel_button_click():
            if not self._error_text_lbl.text():
                self.accept()
            else:
                # Avoid requiring duplicate confirmation of close operation by calling reject() function of super class instead of overridden reject function.
                super().reject()
        else:
            event.ignore()

    @QtCore.pyqtSlot(Exception)  # type: ignore[untyped-decorator]
    def _handle_export_failure(self, err: Exception) -> None:
        self._handle_non_recoverable_error(err)

    @QtCore.pyqtSlot(float)  # type: ignore[untyped-decorator]
    def _handle_batch_exported(self, batch_generation_duration_in_seconds: float) -> None:
        batch_data: ExportedBatchData = ExportedBatchData(exported_sim_runs=0, skipped_sim_runs=0)
        try:
            batch_data = self._worker_send_queue.get_nowait()
        except queue.Empty:
            show_and_request_ok_in_optionally_cancellable_notification(
                message_box_type=MessageBoxType.WARNING,
                message_box_parent=self,
                message_box_title="Encountered empty queue!",
                message_box_content="The send queue of the simulation run export worker should at least contain one element (since only a single entry per batch is created) but the queue was empty.",
                is_cancellable=False,
                log_contents=True,
            )
            QtCore.QTimer.singleShot(DEFAULT_WORKER_CONTINUE_DELAY_IN_MS, self._allow_worker_to_continue)
            return
        except Exception as err:
            self._handle_non_recoverable_error(err)
            return

        self._progress_info_text_lbl.setText(
            f"Batch completed! Exported {batch_data.exported_sim_runs} and skipping {batch_data.skipped_sim_runs} simulation runs. Runtime [in seconds]: {batch_generation_duration_in_seconds}"
        )
        self._accumulate_and_update_total_runtime(batch_generation_duration_in_seconds)
        self._num_processed_sim_runs += batch_data.exported_sim_runs + batch_data.skipped_sim_runs

        if self._progress_bar is not None:
            self._progress_bar.setValue(self._num_processed_sim_runs)

        self._total_num_exported_sim_runs += batch_data.exported_sim_runs
        self._total_num_skipped_sim_runs += batch_data.skipped_sim_runs
        self._exported_sim_runs_data_lbl.setText(
            EXPORTED_SIM_RUNS_DATA_LABEL.format(
                n_exported_sim_runs=self._total_num_exported_sim_runs,
                n_skipped_sim_runs=self._total_num_skipped_sim_runs,
            )
        )
        QtCore.QTimer.singleShot(DEFAULT_WORKER_CONTINUE_DELAY_IN_MS, self._allow_worker_to_continue)

    @QtCore.pyqtSlot()  # type: ignore[untyped-decorator]
    def _enqueue_next_simulation_runs_to_export(self) -> None:
        try:
            for i in range(
                self._last_exported_sim_run_num, self._last_exported_sim_run_num + self._worker_recv_queue_batch_size
            ):
                to_be_enqueued_sim_run_model: SimulationRunModel | None = (
                    self._shared_simulation_runs_model.get_simulation_run_model(i)
                )
                self._last_exported_sim_run_num += 1
                self._worker_recv_queue.put_nowait(to_be_enqueued_sim_run_model)
                if to_be_enqueued_sim_run_model is None:
                    break
            QtCore.QTimer.singleShot(DEFAULT_WORKER_CONTINUE_DELAY_IN_MS, self._allow_worker_to_continue)
        except Exception as err:
            self._handle_non_recoverable_error(
                f"Error during enqueue of new simulation runs, reason: {SimulationRunJsonExportDialog._stringify_error(err)}"
            )

    @QtCore.pyqtSlot(bool)  # type: ignore[untyped-decorator]
    def _handle_export_completion(self, was_cancellation_requested: bool) -> None:
        self._progress_info_text_lbl.setText("Simulation run export finished!")
        log_info_to_console("Simulation run export finished!")

        if self._progress_bar is not None:
            self._progress_bar.setVisible(False)

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
        if self._worker is None:
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

    def _handle_non_recoverable_error(self, err: Exception | str | None) -> None:
        self._progress_info_text_lbl.setText("")
        if err is not None:
            self._update_displayed_error_text(err, num_additionally_skipped_stack_frames_starting_from_this_function=2)

        self._request_worker_cancellation()
        self._shutdown_worker_thread_and_await_completion()
