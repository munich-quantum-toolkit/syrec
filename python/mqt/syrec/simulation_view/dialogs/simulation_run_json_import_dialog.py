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
from typing import TYPE_CHECKING

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
from ..workers.simulation_run_json_import_worker import SimulationRunJsonImportWorker
from .base_progress_dialog import DEFAULT_MEDIUM_QUEUE_SIZE, DEFAULT_WORKER_CONTINUE_DELAY_IN_MS, BaseProgressDialog


class SimulationRunJsonImportDialog(BaseProgressDialog[SimulationRunJsonImportWorker]):
    def __init__(self, parent: QtWidgets.QWidget, shared_simulation_runs_model: QtSimulationRunModel) -> None:
        super().__init__(
            parent,
            shared_simulation_runs_model,
            dialog_title="Importing simulation runs...",
            optional_progress_bar_text_format=None,
            create_default_layout=False,
        )
        self._num_imported_simulation_runs: int = 0

        self._worker_send_queue_batch_size: int = 0
        self._worker_send_queue: queue.SimpleQueue[SimulationRunModel] = queue.SimpleQueue()

        self._dialog_button_box.accepted.connect(self.accept)
        self._dialog_button_box.rejected.connect(self._handle_import_from_file_cancel_button_click)

        self._import_origin_info_lbl = QtWidgets.QLabel("")
        self._import_origin_info_lbl.setAlignment(QtCore.Qt.AlignmentFlag.AlignCenter)

        self._num_imported_simulation_runs_info_lbl = QtWidgets.QLabel("")
        self._num_imported_simulation_runs_info_lbl.setAlignment(QtCore.Qt.AlignmentFlag.AlignCenter)

        layout = QtWidgets.QVBoxLayout()
        layout.addWidget(self._title_lbl)
        layout.addWidget(self._import_origin_info_lbl)
        layout.addWidget(self._progress_info_text_lbl)
        layout.addWidget(self._error_text_lbl)
        layout.addStretch()

        aggregate_stats_controls_layout = QtWidgets.QHBoxLayout()
        aggregate_stats_controls_layout.addWidget(self._num_imported_simulation_runs_info_lbl)
        aggregate_stats_controls_layout.addWidget(self._total_runtime_info_text_lbl)
        layout.addLayout(aggregate_stats_controls_layout)

        layout.addWidget(self._dialog_button_box)
        self.setLayout(layout)

    def start_import(
        self,
        path_to_json_file: Path,
        expected_input_state_size: int,
        worker_send_queue_batch_size: int = DEFAULT_MEDIUM_QUEUE_SIZE,
    ) -> None:
        self._title_lbl.setText(
            f"Importing simulation runs from .json file with batch size {worker_send_queue_batch_size}!"
        )
        self._import_origin_info_lbl.setText(f"Import source: {path_to_json_file!s}")

        if worker_send_queue_batch_size < 1 or expected_input_state_size < 1:
            show_and_request_ok_in_optionally_cancellable_notification(
                message_box_type=MessageBoxType.ERROR,
                message_box_parent=self,
                message_box_title="Invalid input parameters detected",
                message_box_content=f"Expected worker send queue batch size (value={worker_send_queue_batch_size}) and expected input state size(value={expected_input_state_size}) to be a positive integers!",
                is_cancellable=False,
            )
            super().reject()
            return

        self._worker_send_queue_batch_size = worker_send_queue_batch_size
        # Some helpful links are the official QThread documentation but some helpful explanaitions were also found in:
        # - https://www.haccks.com/posts/how-to-use-qthread-correctly-p1/
        # We are creating a worker object that will perform a long running operation in the future with the worker
        # instance being a member of the dialog class/object. Since the worker will define slots for the cancellation of
        # the long running operation that it executes but also emits signals, said worker needs to be implemented as a QObject that will run its own event loop
        # instead of subclassing QThread which executes its slots in the thread in which the QThread was created and might not execute its own event loop.
        self._worker = SimulationRunJsonImportWorker(
            path_to_json_file,
            expected_input_state_size,
            worker_send_queue_config=QueueConfig(
                queue_instance=self._worker_send_queue, queue_batch_size=self._worker_send_queue_batch_size
            ),
        )
        # Create a new QThread that manages one system thread WITHOUT BEING AN ACTUAL THREAD (see: https://doc.qt.io/qtforpython-6/PySide6/QtCore/QThread.html#detailed-description)
        self._worker_thread = QtCore.QThread()
        # We are now modifying the thread affinity (https://doc.qt.io/qt-6/qobject.html#thread-affinity) of the worker object to the worker thread.
        # This will control in which thread the received events of the worker are processed. The worker instance is still available in the dialog
        # so the latter can still access member variables, etc. of the former.
        self._worker.moveToThread(self._worker_thread)
        # If the worker has completed a batch in its long running operation then it will emit a corresponding signal that should be processed by the dialog instance.
        # Since we have changed the thread affinity of the worker, the worker signal -> dialog slot connection needs to be marked as a queued connection so that the
        # signal of the worker will enqueue an entry into the event queue of the dialog and then continue its long running operation in the worker thread. Since the
        # worker and main thread do not share the same event queue, the slot called in the dialog is then executed in the main thread.
        #
        # At runtime Qt could decide at runtime whether a direct or queued connection is required based on the thread affinity between the connected signal and slot but
        # we try to mark this behaviour explicitly by defining the signal-slot connection as a queued connection.
        self._worker.batchCompleted.connect(
            self._handle_imported_sim_run_batch, QtCore.Qt.ConnectionType.QueuedConnection
        )
        # The worker thread executing the long running worker operation will still continue running after the 'finished' signal of the worker was received thus
        # we need to manually handle the correct cancellation of the worker thread
        self._worker.finished.connect(self._handle_import_completion, QtCore.Qt.ConnectionType.QueuedConnection)
        # Assuming that we are correctly catching all errors of the long running worker operation in the function execution said operation (executed in the worker thread)
        # the worker will emit a signal containing the caught error with the worker thread still running thus again we need to manually handle its cancellation
        self._worker.failed.connect(self._handle_importer_failure, QtCore.Qt.ConnectionType.QueuedConnection)

        # We initially tried to move the constructor parameters of the SimulationRunJsonImportWorker to the function executing the long running operation by using a lambda
        # that will trigger the latter but since python lambdas seemingly do not have thread affinity (https://stackoverflow.com/a/28626472) the lambda is executed in the main
        # thread and thus the long running operation would be executed in the main thread blocking the GUI and potentially causing thread starvation if generated batches need to
        # be acknowledged.
        self._worker_thread.started.connect(self._worker.start_import, QtCore.Qt.ConnectionType.QueuedConnection)
        # Since we are manually triggering the worker thread shutdown, after the worker thread has finished the associated QThread should be deleted
        self._worker_thread.finished.connect(self._worker_thread.deleteLater)
        # Additionally we 'clean' up the worker and worker thread instances (by setting them to None) after the worker thread has finished
        self._worker_thread.finished.connect(self._reset_workers)
        # Only this call will actually start a new thread
        self._worker_thread.start(QtCore.QThread.Priority.LowPriority)
        self._change_dialog_cancel_button_enable_state(True)

    # Pressing the ESC key will only close the dialog but not close it thus no closeEvent will be triggered.
    @override
    def reject(self) -> None:
        if self._handle_import_from_file_cancel_button_click():
            super().reject()

    @override
    def closeEvent(self, event: QtGui.QCloseEvent) -> None:
        # Ask for confirmation before closing
        if self._handle_import_from_file_cancel_button_click():
            if not self._error_text_lbl.text():
                self.accept()
            else:
                # Avoid requiring duplicate confirmation of close operation by calling reject() function of super class instead of overridden reject function.
                super().reject()
        else:
            event.ignore()

    @QtCore.pyqtSlot(Exception)  # type: ignore[untyped-decorator]
    def _handle_importer_failure(self, err: Exception) -> None:
        self._handle_non_recoverable_error(err)

    @QtCore.pyqtSlot(float)  # type: ignore[untyped-decorator]
    def _handle_imported_sim_run_batch(self, batch_generation_duration_in_seconds: float) -> None:
        if self._stop_processing_recv_batches:
            return

        n_dequeued_batch_elems: int = 0
        try:
            for _ in range(self._worker_send_queue_batch_size):
                self._shared_simulation_runs_model.add_simulation_run_model(self._worker_send_queue.get_nowait())
                n_dequeued_batch_elems += 1
        except queue.Empty:
            # The last batch generated by the worker could contain less than the expected batch size elements thus an empty queue should not be treated as an error
            pass
        except Exception as sim_run_model_addition_err:
            self._handle_non_recoverable_error(sim_run_model_addition_err)
            return

        self._update_progress_text_with_batch_info(n_dequeued_batch_elems, batch_generation_duration_in_seconds)
        self._accumulate_and_update_total_runtime(batch_generation_duration_in_seconds)
        self._num_imported_simulation_runs += n_dequeued_batch_elems
        self._num_imported_simulation_runs_info_lbl.setText(
            f"Num. imported simulation runs: {self._num_imported_simulation_runs}"
        )

        if self._progress_bar is not None:
            self._progress_bar.setValue(self.num_generated_input_states)
        self._progress_info_text_lbl.setText("")
        QtCore.QTimer.singleShot(DEFAULT_WORKER_CONTINUE_DELAY_IN_MS, self._allow_worker_to_continue)

    @QtCore.pyqtSlot(bool)  # type: ignore[untyped-decorator]
    def _handle_import_completion(self, was_cancellation_requested: bool) -> None:
        self._progress_info_text_lbl.setText("Simulation run import finished!")
        log_info_to_console("Simulation run import finished!")

        # Cancelling the long running operation through a click on the cancel button of the dialog will already request a shutdown of the worker
        # and its associated thread but the same operation also needs to be execute when the worker completes successfully. However, when cancellation
        # was already requested, skip this operation.
        if not was_cancellation_requested:
            self._request_worker_cancellation()
            self._shutdown_worker_thread_and_await_completion()

        self._change_dialog_cancel_button_enable_state(False)
        self._change_dialog_ok_button_enable_state(True)

    @QtCore.pyqtSlot()  # type: ignore[untyped-decorator]
    def _handle_import_from_file_cancel_button_click(self) -> bool:
        if self._worker is None:
            return True

        if show_and_request_ok_in_optionally_cancellable_notification(
            message_box_type=MessageBoxType.QUESTION,
            message_box_parent=self,
            message_box_title="Cancellation of import from json file!",
            message_box_content="Are you sure that you want to stop the import of simulation runs from the file? This will cause the deletion of all already generated simulation runs.",
            is_cancellable=True,
            log_contents=False,
        ):
            log_info_to_console("Cancellation of simulation run import requested!")
            self._handle_non_recoverable_error(None)
            return True
        return False

    def _handle_non_recoverable_error(self, err: Exception | str | None) -> None:
        self._progress_info_text_lbl.setText("")
        if err is not None:
            self._update_displayed_error_text(err, num_additionally_skipped_stack_frames_starting_from_this_function=2)

        try:
            self._shared_simulation_runs_model.delete_all_simulation_run_models()
        except Exception:
            show_and_request_ok_in_optionally_cancellable_notification(
                message_box_type=MessageBoxType.ERROR,
                message_box_parent=self,
                message_box_title="Internal error!",
                message_box_content="Failed to delete all simulation run models during handling of non-recoverable error!\nThis should not happen, cancelling long running operation!",
                is_cancellable=False,
            )

        self._request_worker_cancellation()
        self._shutdown_worker_thread_and_await_completion()
