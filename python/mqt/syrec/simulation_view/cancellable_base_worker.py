# Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
# Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

from __future__ import annotations

import time
from typing import Any, Final, TypeVar

from PyQt6 import QtCore

T = TypeVar("T")


class CancellableBaseWorker(QtCore.QObject):  # type: ignore[misc]
    batch_completed = QtCore.pyqtSignal(float, object, name="batchCompleted")
    # While the cancellation operation is assumed to request the cancellation of the internal worker
    # as well as the worker_thread, due to the Qt event loop the QThread.finished signal is received
    # after the finished signal of the worker, i.e. the order of events in case of a cancellation or error will be:
    # [Optionally error thrown in worker] -> Cancellation requested -> Finished -> QThread.finished
    #
    # This could lead to the worker attempting to perform a double shutdown of the worker/worker_thread,
    # assuming that the cancel/error handler will request the worker shutdown, when the finished slot of the worker
    # perform the shutdown of the worker. Thus we introduce an additional flag in  the finished signal to perform a
    # conditional shutdown of the worker in the slot that is connected to the finished signal.
    finished = QtCore.pyqtSignal(bool, name="finished")
    failed = QtCore.pyqtSignal(Exception, name="failed")

    def __init__(self, do_batches_require_ack: bool):
        super().__init__()

        self.cancellation_requested = False
        self.cancellation_flag_mutex = QtCore.QReadWriteLock()
        self.batch_ack_mutex: QtCore.QMutex | None = QtCore.QMutex() if do_batches_require_ack else None
        self.wait_on_batch_processed_acknowledgement_condition: QtCore.QWaitCondition | None = (
            QtCore.QWaitCondition() if do_batches_require_ack else None
        )

    def request_cancellation(self) -> None:
        # Since the wait of the QWaitCondition can only be 'cancelled' by either a wakeX call or by providing a timeout value with the
        # latter probably leading to a while-loop construct repeatedly performing temporary waits (until the timer elapses), the programmer
        # needs to make sure that the cancellation operation will both set the cancellation flag as well as waking the QWaitCondition in a single
        # operation (i.e. while locking the batch_ack_mutex)
        if self.batch_ack_mutex is not None and self.wait_on_batch_processed_acknowledgement_condition:
            with QtCore.QMutexLocker(self.batch_ack_mutex):
                self.set_cancellation_requested_flag(True)
                self.wait_on_batch_processed_acknowledgement_condition.wakeAll()
        else:
            self.set_cancellation_requested_flag(True)

    def ack_batch_processed(self) -> None:
        if self.batch_ack_mutex is not None and self.wait_on_batch_processed_acknowledgement_condition is not None:
            with QtCore.QMutexLocker(self.batch_ack_mutex):
                self.wait_on_batch_processed_acknowledgement_condition.wakeAll()

    def is_cancellation_requested(self) -> bool:
        cancellation_requested: bool = False
        self.cancellation_flag_mutex.lockForRead()
        cancellation_requested = self.cancellation_requested
        self.cancellation_flag_mutex.unlock()
        return cancellation_requested

    def set_cancellation_requested_flag(self, flag_value: bool) -> None:
        self.cancellation_flag_mutex.lockForWrite()
        self.cancellation_requested = flag_value
        self.cancellation_flag_mutex.unlock()

    @staticmethod
    def are_list_of_batch_items_of_type(batch_data: Any, expected_batch_element_type: type[T]) -> bool:
        return isinstance(batch_data, list) and all(
            isinstance(batch_item, expected_batch_element_type) for batch_item in batch_data
        )

    @staticmethod
    def _get_timestamp() -> float:
        return time.perf_counter()

    # TODO: Is this correct
    @staticmethod
    def _calc_batch_duration_and_return_end_timestamp_in_seconds(batch_start_timestamp: float) -> float:
        batch_end_timestamp: Final[float] = CancellableBaseWorker._get_timestamp()
        batch_duration = batch_end_timestamp - batch_start_timestamp
        batch_start_timestamp = batch_end_timestamp
        return batch_duration
