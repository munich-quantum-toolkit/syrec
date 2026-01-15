# Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
# Copyright (c) 2025 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

from __future__ import annotations

import time
from typing import Final

from PyQt6 import QtCore

from mqt import syrec

from .qt_simulation_run_model import SimulationRunModel


class AllInputStatesGeneratorWorker(QtCore.QObject):  # type: ignore[misc]
    batch_generated = QtCore.pyqtSignal(tuple, name="batchGenerated")
    generation_failed = QtCore.pyqtSignal(Exception, name="generationFailed")
    generation_cancelled = QtCore.pyqtSignal(name="generationCancelled")
    generation_finished = QtCore.pyqtSignal(name="generationFinished")

    def __init__(self, expected_input_state_size: int, batch_size: int):
        super().__init__()

        if expected_input_state_size < 0:
            msg = f"Expected input state size must be a positive integer but was actually {expected_input_state_size}!"
            raise ValueError(msg)

        if batch_size < 1:
            msg = f"Batch size must be larger than 0 but was actually {batch_size}"
            raise ValueError(msg)

        self.expected_input_state_size: Final[int] = expected_input_state_size
        self.batch_size: Final[int] = batch_size
        self.cancellation_requested = False
        self.cancellation_flag_mutex = QtCore.QReadWriteLock()
        self.wait_on_batch_processed_acknowledgement_condition = QtCore.QWaitCondition()

    @QtCore.pyqtSlot()  # type: ignore[untyped-decorator]
    def start_generation(self) -> None:
        n_states_to_generate: int = 2**self.expected_input_state_size
        n_batches: int = n_states_to_generate // self.batch_size

        batch_generation_start_time: float = time.perf_counter()
        batch_generation_end_time: float = 0
        batch_generation_duration_in_seconds: float = 0
        try:
            first_integer_encoding_first_state_of_batch: int = 0
            batch_data: list[SimulationRunModel | None] = [None for _ in range(self.batch_size)]
            for _ in range(n_batches):
                if self._thread_safe_check_whether_cancellation_is_requested():
                    break

                for i in range(self.batch_size):
                    batch_data[i] = AllInputStatesGeneratorWorker._generate_sim_run_model_for_input_state(
                        self.expected_input_state_size, first_integer_encoding_first_state_of_batch + i
                    )

                batch_generation_end_time = time.perf_counter()
                batch_generation_duration_in_seconds = batch_generation_end_time - batch_generation_start_time
                batch_generation_start_time = batch_generation_end_time

                self.batch_generated.emit((batch_generation_duration_in_seconds, batch_data.copy()))
                try:
                    self.cancellation_flag_mutex.lockForRead()
                    # Lock needs to be already held for wait condition to not return immediately
                    self.wait_on_batch_processed_acknowledgement_condition.wait(self.cancellation_flag_mutex)
                finally:
                    self.cancellation_flag_mutex.unlock()
                # An artificial delay improves the responsiveness of the UI but does not seem like the best solution. However, using
                # a delayed acknowledgement in the UI thread would increase the complexity of the implementation of the UI.
                time.sleep(0.1)

                first_integer_encoding_first_state_of_batch += self.batch_size
                for i in range(self.batch_size):
                    batch_data[i] = None

            n_elems_in_last_batch: int = n_states_to_generate % self.batch_size
            if n_elems_in_last_batch != 0 and not self._thread_safe_check_whether_cancellation_is_requested():
                last_batch_data: list[SimulationRunModel | None] = [None for _ in range(n_elems_in_last_batch)]
                for i in range(n_elems_in_last_batch):
                    last_batch_data[i] = AllInputStatesGeneratorWorker._generate_sim_run_model_for_input_state(
                        self.expected_input_state_size, first_integer_encoding_first_state_of_batch + i
                    )

                batch_generation_end_time = time.perf_counter()
                batch_generation_duration_in_seconds = batch_generation_end_time - batch_generation_start_time
                batch_generation_start_time = batch_generation_end_time
                self.batch_generated.emit((batch_generation_duration_in_seconds, last_batch_data))
        except Exception as err:
            self.generation_failed.emit(err)
        self.generation_finished.emit()

    # In the cross thread communication between the main thread (rendering the GUI) and the worker thread we had the issue that if we define the slot function with a QtCore.pyqtSlot() decorator
    # the main thread will not invoke said slot in the worker thread but we do not know exactly we since other signal->slot connections between the two threads function when being defined with
    # corresponding decorators. Thus for now we define the slot without a decorator.
    #
    # One explanation, generated by an AI agent, was:
    # The decisive moment is when the decorator runs, not when you later do moveToThread.
    # At import time (define time) the worker instance already exists and lives in the GUI thread; the decorator therefore registers the slot in the GUI thread's meta-object.
    # Afterwards you move the object to the worker thread, but the meta-object data that Qt uses to locate the slot stays where it was created - in the GUI thread.
    # When the GUI thread later emits the signal, Qt again looks in the GUI thread's meta-object, finds the entry that was created by the decorator, and tries to invoke it.
    # Because the slot entry is marked as "belonging to another thread" (the worker thread), Qt posts a queued meta-call to that thread … but the worker thread has no corresponding meta-object entry, so nothing is executed.
    # If you remove the decorator the connection is handled purely in Python (Qt simply stores the callable); the queued connection then works, because Python callables are independent of the meta-object system.
    # In short:
    #    - pyqtSlot must be executed after the object has been moved to the target thread, or
    #    - drop the decorator and rely on the automatic queued connection that PyQt already provides.
    def request_cancellation(self) -> None:
        self._thread_safe_set_cancellation_requested_flag(True)
        self.wait_on_batch_processed_acknowledgement_condition.wakeAll()

    # Again we define the slot without the corresponding decorator, for further information we refer to the request_cancellation function.
    def ack_batch_processed(self) -> None:
        self.wait_on_batch_processed_acknowledgement_condition.wakeAll()

    @staticmethod
    def _generate_sim_run_model_for_input_state(
        expected_input_state_size: int, integer_defining_input_state: int
    ) -> SimulationRunModel:
        input_state = syrec.n_bit_values_container(expected_input_state_size)
        for qubit in range(expected_input_state_size):
            input_state.set(qubit, bool((integer_defining_input_state >> qubit) & 1))
        return SimulationRunModel(input_state, expected_output_state=None)

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
