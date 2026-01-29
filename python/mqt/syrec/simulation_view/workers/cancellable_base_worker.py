# Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
# Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

from __future__ import annotations

import time
from dataclasses import dataclass
from typing import TYPE_CHECKING, Any, Final, TypeVar

from PyQt6 import QtCore

from ...message_box_utils import MessageBoxType, show_and_request_ok_in_optionally_cancellable_notification

if TYPE_CHECKING:
    from PyQt6 import QtWidgets

T = TypeVar("T")


@dataclass(frozen=True)
class BatchTimestamps:
    start: float
    end: float
    duration: float


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
    def is_batch_data_list_of_expected_type(
        batch_data: Any, expected_batch_element_type: type[T], parent_widget_for_error_notification: QtWidgets.QWidget
    ) -> bool:
        if not isinstance(batch_data, list):
            show_and_request_ok_in_optionally_cancellable_notification(
                message_box_type=MessageBoxType.WARNING,
                message_box_parent=parent_widget_for_error_notification,
                message_box_title="Cannot handle batch data",
                message_box_content=f"Expected batch data to be a list of {expected_batch_element_type} but was actually {type(batch_data)}! This should not happen.",
                is_cancellable=False,
            )
            return False

        mismatched_elem_type: type | None = next(
            (type(elem) for elem in batch_data if not isinstance(elem, expected_batch_element_type)),
            None,
        )
        if mismatched_elem_type is None:
            # All elements of list match expected element type or list was empty
            return True

        show_and_request_ok_in_optionally_cancellable_notification(
            message_box_type=MessageBoxType.WARNING,
            message_box_parent=parent_widget_for_error_notification,
            message_box_title="Cannot handle batch data",
            message_box_content=f"Expected batch data to be a list of {expected_batch_element_type} but was actually a list that contained an element of type {mismatched_elem_type}! This should not happen.",
            is_cancellable=False,
        )
        return False

    @staticmethod
    def is_batch_data_of_type(
        batch_data: Any, expected_batch_type: type[T], parent_widget_for_error_notification: QtWidgets.QWidget
    ) -> bool:
        if isinstance(batch_data, expected_batch_type):
            return True

        show_and_request_ok_in_optionally_cancellable_notification(
            message_box_type=MessageBoxType.WARNING,
            message_box_parent=parent_widget_for_error_notification,
            message_box_title="Cannot handle batch data",
            message_box_content=f"Expected batch data to be of type {expected_batch_type} but was actually of type {type(batch_data)}! This should not happen.",
            is_cancellable=False,
        )
        return False

    @staticmethod
    def _get_timestamp() -> float:
        return time.perf_counter()

    @staticmethod
    def _calc_batch_duration_and_return_end_timestamp_in_seconds(batch_start_timestamp: float) -> BatchTimestamps:
        batch_end_timestamp: Final[float] = CancellableBaseWorker._get_timestamp()
        batch_duration = batch_end_timestamp - batch_start_timestamp
        return BatchTimestamps(batch_start_timestamp, batch_end_timestamp, batch_duration)
