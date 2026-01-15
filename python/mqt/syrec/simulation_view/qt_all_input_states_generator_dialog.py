# Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
# Copyright (c) 2025 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

from __future__ import annotations

from typing import TYPE_CHECKING, Final

from PyQt6 import QtCore, QtWidgets

if TYPE_CHECKING:
    from PyQt6 import QtGui

    from .qt_simulation_run_model import QtSimulationRunModel, SimulationRunModel

from .all_input_states_generator_worker import AllInputStatesGeneratorWorker

TOTAL_RUNTIME_TEXT_FORMAT: Final[str] = (
    "Total runtime for input states generation [in seconds]: {total_runtime_in_sec:f}"
)


class AllInputStatesGeneratorDialog(QtWidgets.QDialog):  # type: ignore[misc]
    # input_state_batch_ack = QtCore.pyqtSignal(name="inputStateBatchAck")
    input_state_batch_ack = QtCore.pyqtSignal(name="inputStateBatchAck")
    request_worker_cancellation = QtCore.pyqtSignal(name="requestWorkerCancellation")

    def __init__(self, parent: QtWidgets.QWidget):
        super().__init__(parent)

        self.shared_simulation_runs_model: QtSimulationRunModel | None = None
        self.worker_thread: QtCore.QThread | None = None
        self.worker: AllInputStatesGeneratorWorker | None = None

        self.num_generated_input_states: int = 0
        self.stop_processing_recv_input_state_batches: bool = False
        self.total_input_state_generation_runtime_in_seconds: float = 0

        self.setModal(True)
        self.setSizeGripEnabled(True)
        self.setWindowTitle("Generating simulation runs...")
        left = 0
        top = 0
        width = 400
        height = 200
        self.setGeometry(left, top, width, height)

        main_layout = QtWidgets.QVBoxLayout()
        self.progress_bar = QtWidgets.QProgressBar()
        # For placeholder values see: https://doc.qt.io/qtforpython-6/PySide6/QtWidgets/QProgressBar.html#PySide6.QtWidgets.QProgressBar.format
        self.progress_bar.setFormat("Generated %v out of %m input states")
        self.progress_bar.setAlignment(QtCore.Qt.AlignmentFlag.AlignCenter)

        self.err_text_lbl = QtWidgets.QLabel("")
        self.err_text_lbl.setStyleSheet("QLabel { color : red; }")
        self.err_text_lbl.setAlignment(QtCore.Qt.AlignmentFlag.AlignCenter)

        self.progress_text_lbl = QtWidgets.QLabel("")
        self.progress_text_lbl.setAlignment(QtCore.Qt.AlignmentFlag.AlignCenter)

        self.dialog_button_box = QtWidgets.QDialogButtonBox(
            QtWidgets.QDialogButtonBox.StandardButton.Ok | QtWidgets.QDialogButtonBox.StandardButton.Cancel
        )
        self.dialog_button_box.setCenterButtons(True)
        self.dialog_button_box.rejected.connect(self._handle_input_state_generation_cancel_button_click)
        self.dialog_button_box.accepted.connect(self.accept)

        self._change_dialog_ok_button_enable_state(False)
        self._change_dialog_cancellation_button_enable_state(False)

        main_layout.addWidget(self.progress_bar)
        main_layout.addWidget(self.progress_text_lbl)
        main_layout.addWidget(self.err_text_lbl)

        main_layout.addStretch()
        self.total_runtime_text_lbl = QtWidgets.QLabel("")
        self.total_runtime_text_lbl.setAlignment(QtCore.Qt.AlignmentFlag.AlignCenter)
        main_layout.addWidget(self.total_runtime_text_lbl)
        main_layout.addWidget(self.dialog_button_box)
        self.setLayout(main_layout)

    def start_generation(
        self, shared_simulation_runs_model: QtSimulationRunModel, expected_input_state_size: int, batch_size: int = 500
    ) -> None:
        self.shared_simulation_runs_model = shared_simulation_runs_model
        self.stop_processing_recv_input_state_batches = False
        self.num_generated_input_states = 0
        self.total_input_state_generation_runtime_in_seconds = 0
        self.progress_text_lbl.setText("")
        self.total_runtime_text_lbl.setText("")

        try:
            self.worker = AllInputStatesGeneratorWorker(expected_input_state_size, batch_size)
        except ValueError as err:
            self.err_text_lbl.setText(f"Error {err=}, {type(err)=} during initialization of input states generator!")
            return

        # TODO: Validation that maximum value can actually be stored in progress bar maximum (should validation be performed in dialog or by caller?)
        self.progress_bar.setMinimum(0)
        self.progress_bar.setMaximum(2**expected_input_state_size)
        self.progress_bar.setValue(0)

        self.inputStateBatchAck.connect(self.worker.ack_batch_processed, QtCore.Qt.ConnectionType.QueuedConnection)
        self.requestWorkerCancellation.connect(
            self.worker.request_cancellation, QtCore.Qt.ConnectionType.QueuedConnection
        )

        self.worker_thread = QtCore.QThread()
        self.worker.moveToThread(self.worker_thread)
        # TODO: It is recommended in the official documentation to mark slots explicitly via the QtCore.pyqtSlot() decorator:
        # see https://doc.qt.io/qtforpython-6/tutorials/basictutorial/signals_and_slots.html#the-slot-class

        # Do not block the UI thread by the potentially long running operations of the worker a new thread is started (which also has its own event loop)
        # and the worker operation moved to the latter. We also do not want to block the UI thread by executing the slots of said worker in the UI thread but
        # instead want to simply send the events to the event queue of its thread thus the QueuedConnection between the signal (here the UI thread) and the receiver (worker thread)
        # needs to be defined as a QueuedConnection (QtCore.Qt.ConnectionType.QueuedConnection).
        self.worker_thread.started.connect(self.worker.start_generation, QtCore.Qt.ConnectionType.QueuedConnection)
        self.worker.batchGenerated.connect(
            self._handle_generated_input_state_batch, QtCore.Qt.ConnectionType.QueuedConnection
        )
        self.worker.generationFinished.connect(
            self._handle_input_state_generator_finished, QtCore.Qt.ConnectionType.QueuedConnection
        )
        self.worker.generationFailed.connect(
            self._handle_input_state_generator_failure, QtCore.Qt.ConnectionType.QueuedConnection
        )

        self.worker_thread.finished.connect(self.worker_thread.deleteLater)
        self.worker_thread.finished.connect(self._reset_workers)
        self.worker_thread.start(QtCore.QThread.Priority.LowPriority)
        self._change_dialog_cancellation_button_enable_state(True)

    def closeEvent(self, event: QtGui.QCloseEvent) -> None:  # noqa: N802
        # Ask for confirmation before closing
        if self.worker is None or self._handle_input_state_generation_cancel_button_click():
            event.accept()
        else:
            event.ignore()

    def test(self) -> None:
        pass

    @QtCore.pyqtSlot(Exception)  # type: ignore[untyped-decorator]
    def _handle_input_state_generator_failure(self, err: Exception) -> None:
        self.progress_text_lbl.setText("")
        self.err_text_lbl.setText(f"Unexpected {err=}, {type(err)=} during generation of input states")
        if self.shared_simulation_runs_model is not None:
            self.shared_simulation_runs_model.delete_all_simulation_run_models()
        else:
            QtWidgets.QMessageBox.critical(
                self,
                "Internal state error!",
                "Shared simulation runs model was not initialized during handling of input state generator failure!\nThis should not happen.",
                buttons=QtWidgets.QMessageBox.StandardButton.Ok,
                defaultButton=QtWidgets.QMessageBox.StandardButton.Ok,
            )
        self._request_worker_cancellation()

    @QtCore.pyqtSlot(tuple)  # type: ignore[untyped-decorator]
    def _handle_generated_input_state_batch(self, batch_data: tuple[float, list[SimulationRunModel]]) -> None:
        if self.stop_processing_recv_input_state_batches:
            return

        self.progress_text_lbl.setText("Generated input state batch!")
        batch_generation_duration_in_seconds: float = batch_data[0]
        generated_simulation_run_models: list[SimulationRunModel] = batch_data[1]

        if self.shared_simulation_runs_model is None:
            QtWidgets.QMessageBox.critical(
                self,
                "Internal state error!",
                "Shared simulation runs model was not initialized during handling of generated input state batch!\nThis should not happen.",
                buttons=QtWidgets.QMessageBox.StandardButton.Ok,
                defaultButton=QtWidgets.QMessageBox.StandardButton.Ok,
            )
            self._request_worker_cancellation()
            return

        # TODO: Error handling
        # TODO: Use delayed processing to reduce "laggy"/almost frozen GUI
        self.shared_simulation_runs_model.add_simulation_run_models(generated_simulation_run_models)
        self.inputStateBatchAck.emit()

        self.total_input_state_generation_runtime_in_seconds += batch_generation_duration_in_seconds
        self.total_runtime_text_lbl.setText(
            TOTAL_RUNTIME_TEXT_FORMAT.format(total_runtime_in_sec=self.total_input_state_generation_runtime_in_seconds)
        )

        self.num_generated_input_states += len(generated_simulation_run_models)
        self.progress_bar.setValue(self.num_generated_input_states)
        self.progress_text_lbl.setText("")

    @QtCore.pyqtSlot()  # type: ignore[untyped-decorator]
    def _handle_input_state_generator_finished(self) -> None:
        self.progress_text_lbl.setText("Input state generator finished!")
        self.progress_bar.setVisible(False)
        self._request_worker_cancellation()

        if self.worker_thread is not None:
            self.worker_thread.quit()
            self.worker_thread.wait()
            self.progress_text_lbl.setText("Input state generator thread finished!")

        self._change_dialog_ok_button_enable_state(True)
        self._change_dialog_cancellation_button_enable_state(False)

    def _request_worker_cancellation(self) -> None:
        self.stop_processing_recv_input_state_batches = True
        self.progress_text_lbl.setText("Requesting cancellation of input state generator!")
        if self.worker is not None:
            self.requestWorkerCancellation.emit()
            self._change_dialog_cancellation_button_enable_state(False)

    @QtCore.pyqtSlot()  # type: ignore[untyped-decorator]
    def _handle_input_state_generation_cancel_button_click(self) -> bool:
        clicked_button_in_confirmation_dialog: QtWidgets.QMessageBox.StandardButton = QtWidgets.QMessageBox.warning(
            self,
            "Cancellation of generation of input states requested!",
            "Are you sure that you want to stop the generation of the input states? This will cause the deletion of all already generated input states.",
            buttons=QtWidgets.QMessageBox.StandardButton.Ok | QtWidgets.QMessageBox.StandardButton.Cancel,
            defaultButton=QtWidgets.QMessageBox.StandardButton.Ok,
        )

        if clicked_button_in_confirmation_dialog == QtWidgets.QMessageBox.StandardButton.Ok:
            self._request_worker_cancellation()
            if self.shared_simulation_runs_model is not None:
                self.shared_simulation_runs_model.delete_all_simulation_run_models()
            return True
        return False

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
