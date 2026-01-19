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

    from .qt_simulation_run_model import QtSimulationRunModel, SimulationRunModel

from .qt_simulation_run_model import SimulationRunModel
from .simulation_run_json_import_worker import SimulationRunJsonImportWorker

AGGREGATE_IMPORT_DATA_TEXT_FORMAT: Final[str] = (
    "Imported simulation runs: {num_imported_simulation_runs:d} | Total runtime for simulation run import [in seconds]: {total_runtime_in_sec:f}"
)
IMPORT_ORIGIN_INFO_TEXT_FORMAT: Final[str] = "Importing simulation runs from file {path_to_json_file:s}"
IMPORTED_BATCH_PROGRESS_INFO_TEXT_FORMAT: Final[str] = (
    "Finished import of simulation runs batch from file (runtime [in sec]: {batch_generation_duration_in_seconds:f}!"
)


class SimulationRunJsonImportDialog(QtWidgets.QDialog):  # type: ignore[misc]
    def __init__(self, parent: QtWidgets.QWidget):
        super().__init__(parent)

        self.shared_simulation_runs_model: QtSimulationRunModel | None = None
        self.worker_thread: QtCore.QThread | None = None
        self.worker: SimulationRunJsonImportWorker | None = None

        self.num_imported_simulation_runs: int = 0
        self.stop_processing_imported_sim_run_batches: bool = False
        self.total_simulation_run_import_runtime_in_seconds: float = 0

        self.setModal(True)
        self.setSizeGripEnabled(True)
        self.setWindowTitle("Importing simulation runs...")
        left = 0
        top = 0
        width = 400
        height = 200
        self.setGeometry(left, top, width, height)

        main_layout = QtWidgets.QVBoxLayout()
        self.import_origin_info_lbl = QtWidgets.QLabel("")
        self.import_origin_info_lbl.setAlignment(QtCore.Qt.AlignmentFlag.AlignCenter)

        self.progress_text_lbl = QtWidgets.QLabel("")
        self.progress_text_lbl.setAlignment(QtCore.Qt.AlignmentFlag.AlignCenter)
        self.progress_text_lbl.setStyleSheet("QLabel { color : gray; }")

        self.err_text_lbl = QtWidgets.QLabel("")
        self.err_text_lbl.setAlignment(QtCore.Qt.AlignmentFlag.AlignCenter)
        self.err_text_lbl.setStyleSheet("QLabel { color : red; }")

        main_layout.addWidget(self.import_origin_info_lbl)
        main_layout.addWidget(self.progress_text_lbl)
        main_layout.addWidget(self.err_text_lbl)
        main_layout.addStretch()

        self.total_runtime_text_lbl = QtWidgets.QLabel("")
        self.total_runtime_text_lbl.setAlignment(QtCore.Qt.AlignmentFlag.AlignCenter)
        main_layout.addWidget(self.total_runtime_text_lbl)

        self.dialog_button_box = QtWidgets.QDialogButtonBox(
            QtWidgets.QDialogButtonBox.StandardButton.Ok | QtWidgets.QDialogButtonBox.StandardButton.Cancel
        )
        self.dialog_button_box.setCenterButtons(True)
        self.dialog_button_box.rejected.connect(self._handle_import_from_file_cancel_button_click)
        self.dialog_button_box.accepted.connect(self.accept)

        self._change_dialog_ok_button_enable_state(False)
        self._change_dialog_cancellation_button_enable_state(False)

        main_layout.addWidget(self.dialog_button_box)
        self.setLayout(main_layout)

    def start_generation(
        self,
        path_to_json_file: Path,
        shared_simulation_runs_model: QtSimulationRunModel,
        expected_input_state_size: int,
        batch_size: int = 1000,
    ) -> None:
        self.shared_simulation_runs_model = shared_simulation_runs_model
        self.import_origin_info_lbl.setText(
            IMPORT_ORIGIN_INFO_TEXT_FORMAT.format(path_to_json_file=str(path_to_json_file))
        )

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
        self._change_dialog_cancellation_button_enable_state(True)

    def closeEvent(self, event: QtGui.QCloseEvent) -> None:  # noqa: N802
        # Ask for confirmation before closing
        if self.worker is None or self._handle_import_from_file_cancel_button_click():
            if not self.err_text_lbl.text():
                self.accept()
            else:
                self.reject()
        else:
            event.ignore()

    @QtCore.pyqtSlot(Exception)  # type: ignore[untyped-decorator]
    def _handle_importer_failure(self, err: Exception) -> None:
        self.progress_text_lbl.setText("")
        self.err_text_lbl.setText(f"Unexpected {err=}, {type(err)=} during import of simulation runs")
        if self.shared_simulation_runs_model is not None:
            self.shared_simulation_runs_model.delete_all_simulation_run_models()
        else:
            QtWidgets.QMessageBox.critical(
                self,
                "Internal state error!",
                "Shared simulation runs model was not initialized during handling of importer failure!\nThis should not happen.",
                buttons=QtWidgets.QMessageBox.StandardButton.Ok,
                defaultButton=QtWidgets.QMessageBox.StandardButton.Ok,
            )
        self._request_worker_cancellation()
        self._await_worker_thread_completion()
        self._change_dialog_cancellation_button_enable_state(False)

    @QtCore.pyqtSlot(float, object)  # type: ignore[untyped-decorator]
    def _handle_imported_sim_run_batch(self, batch_generation_duration_in_seconds: float, batch_data: object) -> None:
        if self.stop_processing_imported_sim_run_batches:
            return

        if not SimulationRunJsonImportWorker.are_list_of_batch_items_of_type(batch_data, SimulationRunModel):
            # TODO: Error logging?
            # TODO: Cancel worker?
            if self.worker is not None:
                self.worker.ack_batch_processed()
            return

        self.progress_text_lbl.setText(
            IMPORTED_BATCH_PROGRESS_INFO_TEXT_FORMAT.format(
                batch_generation_duration_in_seconds=batch_generation_duration_in_seconds
            )
        )
        generated_simulation_run_models: Final[list[SimulationRunModel]] = batch_data  # type: ignore[assignment]
        if self.shared_simulation_runs_model is None:
            QtWidgets.QMessageBox.critical(
                self,
                "Internal state error!",
                "Shared simulation runs model was not initialized during handling of generated input state batch!\nThis should not happen.",
                buttons=QtWidgets.QMessageBox.StandardButton.Ok,
                defaultButton=QtWidgets.QMessageBox.StandardButton.Ok,
            )
            self._request_worker_cancellation()
            self._await_worker_thread_completion()
            return

        # TODO: Error handling
        # TODO: Use delayed processing to reduce "laggy"/almost frozen GUI
        self.shared_simulation_runs_model.add_simulation_run_models(generated_simulation_run_models)
        if self.worker is not None:
            self.worker.ack_batch_processed()

        self.total_simulation_run_import_runtime_in_seconds += batch_generation_duration_in_seconds
        self.num_imported_simulation_runs += len(generated_simulation_run_models)
        self.total_runtime_text_lbl.setText(
            AGGREGATE_IMPORT_DATA_TEXT_FORMAT.format(
                num_imported_simulation_runs=self.num_imported_simulation_runs,
                total_runtime_in_sec=self.total_simulation_run_import_runtime_in_seconds,
            )
        )

    @QtCore.pyqtSlot()  # type: ignore[untyped-decorator]
    def _handle_import_completion(self) -> None:
        self._request_worker_cancellation()
        self._await_worker_thread_completion()
        self._change_dialog_cancellation_button_enable_state(False)
        self._change_dialog_ok_button_enable_state(True)

    def _request_worker_cancellation(self) -> None:
        self.stop_processing_imported_sim_run_batches = True
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
            self.progress_text_lbl.setText("Simulation run importer thread finished!")

    @QtCore.pyqtSlot()  # type: ignore[untyped-decorator]
    def _handle_import_from_file_cancel_button_click(self) -> bool:
        clicked_button_in_confirmation_dialog: QtWidgets.QMessageBox.StandardButton = QtWidgets.QMessageBox.warning(
            self,
            "Cancellation of import from json file!",
            "Are you sure that you want to stop the import of simulation runs from the file? This will cause the deletion of all already generated simulation runs.",
            buttons=QtWidgets.QMessageBox.StandardButton.Ok | QtWidgets.QMessageBox.StandardButton.Cancel,
            defaultButton=QtWidgets.QMessageBox.StandardButton.Ok,
        )

        if clicked_button_in_confirmation_dialog == QtWidgets.QMessageBox.StandardButton.Ok:
            self._request_worker_cancellation()
            if self.shared_simulation_runs_model is not None:
                self.shared_simulation_runs_model.delete_all_simulation_run_models()
            return True
        return False
