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
    from pathlib import Path

    from PyQt6 import QtGui

    from ..simulation_run_model import QtSimulationRunModel, SimulationRunModel

from ...logger_utils import log_error_to_console, log_info_to_console
from ...message_box_utils import MessageBoxType, show_and_request_ok_in_optionally_cancellable_notification
from ..simulation_run_model import SimulationRunModel
from ..workers.simulation_run_json_import_worker import SimulationRunJsonImportWorker
from .base_progress_dialog import BaseProgressDialog


class SimulationRunJsonImportDialog(BaseProgressDialog[SimulationRunJsonImportWorker]):
    def __init__(self, parent: QtWidgets.QWidget):
        super().__init__(
            parent,
            dialog_title="Importing simulation runs...",
            optional_progress_bar_text_format=None,
            create_default_layout=False,
        )
        self.num_imported_simulation_runs: int = 0
        self.shared_simulation_runs_model: QtSimulationRunModel | None = None

        self.dialog_button_box.accepted.connect(self.accept)
        self.dialog_button_box.rejected.connect(self._handle_import_from_file_cancel_button_click)

        self.import_origin_info_lbl = QtWidgets.QLabel("")
        self.import_origin_info_lbl.setAlignment(QtCore.Qt.AlignmentFlag.AlignCenter)

        self.num_imported_simulation_runs_info_lbl = QtWidgets.QLabel("")
        self.num_imported_simulation_runs_info_lbl.setAlignment(QtCore.Qt.AlignmentFlag.AlignCenter)

        layout = QtWidgets.QVBoxLayout()
        layout.addWidget(self.title_lbl)
        layout.addWidget(self.import_origin_info_lbl)
        layout.addWidget(self.progress_info_text_lbl)
        layout.addWidget(self.error_text_lbl)
        layout.addStretch()

        aggregate_stats_controls_layout = QtWidgets.QHBoxLayout()
        aggregate_stats_controls_layout.addWidget(self.num_imported_simulation_runs_info_lbl)
        aggregate_stats_controls_layout.addWidget(self.total_runtime_info_text_lbl)
        layout.addLayout(aggregate_stats_controls_layout)

        layout.addWidget(self.dialog_button_box)
        self.setLayout(layout)

    def start_generation(
        self,
        path_to_json_file: Path,
        shared_simulation_runs_model: QtSimulationRunModel,
        expected_input_state_size: int,
        batch_size: int = 1000,
    ) -> None:
        self.shared_simulation_runs_model = shared_simulation_runs_model
        self.title_lbl.setText(f"Importing simulation runs from .json file with batch size {batch_size}!")
        self.import_origin_info_lbl.setText(f"Import source: {path_to_json_file!s}")

        # Some helpful links are the official QThread documentation but some helpful explanaitions were also found in:
        # - https://www.haccks.com/posts/how-to-use-qthread-correctly-p1/
        # We are creating a worker object that will perform a long running operation in the future with the worker
        # instance being a member of the dialog class/object. Since the worker will define slots for the cancellation of
        # the long running operation that it executes but also emits signals, said worker needs to be implemented as a QObject that will run its own event loop
        # instead of subclassing QThread which executes its slots in the thread in which the QThread was created and might not execute its own event loop.
        self.worker = SimulationRunJsonImportWorker(path_to_json_file, expected_input_state_size, batch_size)
        # Create a new QThread that manages one system thread WITHOUT BEING AN ACTUAL THREAD (see: https://doc.qt.io/qtforpython-6/PySide6/QtCore/QThread.html#detailed-description)
        self.worker_thread = QtCore.QThread()
        # We are now modifying the thread affinity (https://doc.qt.io/qt-6/qobject.html#thread-affinity) of the worker object to the worker thread.
        # This will control in which thread the received events of the worker are processed. The worker instance is still available in the dialog
        # so the latter can still access member variables, etc. of the former.
        self.worker.moveToThread(self.worker_thread)
        # If the worker has completed a batch in its long running operation then it will emit a corresponding signal that should be processed by the dialog instance.
        # Since we have changed the thread affinity of the worker, the worker signal -> dialog slot connection needs to be marked as a queued connection so that the
        # signal of the worker will enqueue an entry into the event queue of the dialog and then continue its long running operation in the worker thread. Since the
        # worker and main thread do not share the same event queue, the slot called in the dialog is then executed in the main thread.
        #
        # At runtime Qt could decide at runtime whether a direct or queued connection is required based on the thread affinity between the connected signal and slot but
        # we try to mark this behaviour explicitly by defining the signal-slot connection as a queued connection.
        self.worker.batchCompleted.connect(
            self._handle_imported_sim_run_batch, QtCore.Qt.ConnectionType.QueuedConnection
        )
        # The worker thread executing the long running worker operation will still continue running after the 'finished' signal of the worker was received thus
        # we need to manually handle the correct cancellation of the worker thread
        self.worker.finished.connect(self._handle_import_completion, QtCore.Qt.ConnectionType.QueuedConnection)
        # Assuming that we are correctly catching all errors of the long running worker operation in the function execution said operation (executed in the worker thread)
        # the worker will emit a signal containing the caught error with the worker thread still running thus again we need to manually handle its cancellation
        self.worker.failed.connect(self._handle_importer_failure, QtCore.Qt.ConnectionType.QueuedConnection)

        # We initially tried to move the constructor parameters of the SimulationRunJsonImportWorker to the function executing the long running operation by using a lambda
        # that will trigger the latter but since python lambdas seemingly do not have thread affinity (https://stackoverflow.com/a/28626472) the lambda is executed in the main
        # thread and thus the long running operation would be executed in the main thread blocking the GUI and potentially causing thread starvation if generated batches need to
        # be acknowledged.
        self.worker_thread.started.connect(self.worker.start_import, QtCore.Qt.ConnectionType.QueuedConnection)
        # Since we are manually triggering the worker thread shutdown, after the worker thread has finished the associated QThread should be deleted
        self.worker_thread.finished.connect(self.worker_thread.deleteLater)
        # Additionally we 'clean' up the worker and worker thread instances (by setting them to None) after the worker thread has finished
        self.worker_thread.finished.connect(self._reset_workers)
        # Only this call will actually start a new thread
        self.worker_thread.start(QtCore.QThread.Priority.LowPriority)
        self._change_dialog_cancel_button_enable_state(True)

    def closeEvent(self, event: QtGui.QCloseEvent) -> None:  # noqa: N802
        # Ask for confirmation before closing
        if self._handle_import_from_file_cancel_button_click():
            if not self.error_text_lbl.text():
                self.accept()
            else:
                self.reject()
        else:
            event.ignore()

    @QtCore.pyqtSlot(Exception)  # type: ignore[untyped-decorator]
    def _handle_importer_failure(self, err: Exception) -> None:
        self._handle_non_recoverable_error(err)

    @QtCore.pyqtSlot(float, object)  # type: ignore[untyped-decorator]
    def _handle_imported_sim_run_batch(self, batch_generation_duration_in_seconds: float, batch_data: object) -> None:
        if self.stop_processing_recv_batches:
            return

        if not SimulationRunJsonImportWorker.is_batch_data_list_of_expected_type(
            batch_data, SimulationRunModel, parent_widget_for_error_notification=self
        ):
            if self.worker is not None:
                self.worker.ack_batch_processed()
            return

        generated_simulation_run_models: Final[list[SimulationRunModel]] = batch_data  # type: ignore[assignment]
        self._update_progress_text_with_batch_info(
            len(generated_simulation_run_models), batch_generation_duration_in_seconds
        )

        if self.shared_simulation_runs_model is None:
            log_error_to_console("Shared simulation runs model was not initialized during handling of batch!")
            self._handle_non_recoverable_error(None)
            return

        try:
            self.shared_simulation_runs_model.add_simulation_run_models(generated_simulation_run_models)
        except Exception as sim_run_model_err:
            self._handle_non_recoverable_error(sim_run_model_err)
            return

        if self.worker is not None:
            self.worker.ack_batch_processed()

        self._accumulate_and_update_total_runtime(batch_generation_duration_in_seconds)
        self.num_imported_simulation_runs += len(generated_simulation_run_models)
        self.num_imported_simulation_runs_info_lbl.setText(
            f"Num. imported simulation runs: {self.num_imported_simulation_runs}"
        )

    @QtCore.pyqtSlot(bool)  # type: ignore[untyped-decorator]
    def _handle_import_completion(self, was_cancellation_requested: bool) -> None:
        self.progress_info_text_lbl.setText("Simulation run import finished!")
        log_info_to_console("Simulation run export finished!")

        if not was_cancellation_requested:
            self._request_worker_cancellation()
            self._shutdown_worker_thread_and_await_completion()

        self._change_dialog_cancel_button_enable_state(False)
        self._change_dialog_ok_button_enable_state(True)

    @QtCore.pyqtSlot()  # type: ignore[untyped-decorator]
    def _handle_import_from_file_cancel_button_click(self) -> bool:
        if self.worker is None:
            return True

        if show_and_request_ok_in_optionally_cancellable_notification(
            message_box_type=MessageBoxType.QUESTION,
            message_box_parent=self,
            message_box_title="Cancellation of import from json file!",
            message_box_content="Are you sure that you want to stop the import of simulation runs from the file? This will cause the deletion of all already generated simulation runs.",
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

        if self.shared_simulation_runs_model is not None:
            try:
                self.shared_simulation_runs_model.delete_all_simulation_run_models()
            except Exception:
                show_and_request_ok_in_optionally_cancellable_notification(
                    message_box_type=MessageBoxType.ERROR,
                    message_box_parent=self,
                    message_box_title="Internal error!",
                    message_box_content="Failed to delete all simulation run models during handling of non-recoverable error!\nThis should not happen, cancelling long running operation!",
                    is_cancellable=False,
                )
        else:
            show_and_request_ok_in_optionally_cancellable_notification(
                message_box_type=MessageBoxType.ERROR,
                message_box_parent=self,
                message_box_title="Internal state error!",
                message_box_content="Shared simulation runs model was not initialized during handling of non-recoverable error!\nThis should not happen, cancelling long running operation!",
                is_cancellable=False,
            )

        if self.worker is not None:
            self._request_worker_cancellation()
        if self.worker_thread is not None:
            self._shutdown_worker_thread_and_await_completion()
