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
    finished = QtCore.pyqtSignal(name="finished")
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
        self.set_cancellation_requested_flag(True)
        self.ack_batch_processed()

    def ack_batch_processed(self) -> None:
        if self.batch_ack_mutex is not None and self.wait_on_batch_processed_acknowledgement_condition is not None:
            with QtCore.QMutexLocker(self.batch_ack_mutex):
                self.wait_on_batch_processed_acknowledgement_condition.wakeOne()

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

    @staticmethod
    def _calc_batch_duration_and_return_end_timestamp_in_seconds(batch_start_timestamp: float) -> float:
        batch_end_timestamp: Final[float] = CancellableBaseWorker._get_timestamp()
        batch_duration = batch_end_timestamp - batch_start_timestamp
        batch_start_timestamp = batch_end_timestamp
        return batch_duration
