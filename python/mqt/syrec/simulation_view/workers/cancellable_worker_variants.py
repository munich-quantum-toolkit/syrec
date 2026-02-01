# Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
# Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

from __future__ import annotations

import sys
import threading
import time
from dataclasses import dataclass
from typing import TYPE_CHECKING, Final, Generic, TypeVar

if sys.version_info >= (3, 12):
    from typing import override
else:
    from typing_extensions import override

from PyQt6 import QtCore

if TYPE_CHECKING:
    import queue

RecvQueueElemType = TypeVar("RecvQueueElemType")
SendQueueElemType = TypeVar("SendQueueElemType")
QueueElemType = TypeVar("QueueElemType")


@dataclass(frozen=True)
class BatchTimestamps:
    start: float = 0
    end: float = 0
    duration: float = 0


@dataclass(frozen=True)
class QueueConfig(Generic[QueueElemType]):
    queue_instance: queue.SimpleQueue[QueueElemType]
    queue_batch_size: int


class CancellableProducerWorker(QtCore.QObject, Generic[SendQueueElemType]):  # type: ignore[misc]
    finished = QtCore.pyqtSignal(bool)
    failed = QtCore.pyqtSignal(Exception)
    batchCompleted = QtCore.pyqtSignal(float)  # noqa: N815

    def __init__(self, worker_send_queue_config: QueueConfig[SendQueueElemType]) -> None:
        super().__init__()
        self.cancellation_requested_flag: threading.Event = threading.Event()
        # The requirement to cancel the long running operation performed by the worker "forces" us to use the non-blocking get_nowait(...) and put_nowait(...) functions of the unbounded
        # queue.SimpleQueue container. Correctly handling the expected batch sizes is the responsibility of the user of this unbounded queue.
        self.send_queue: queue.SimpleQueue[SendQueueElemType] = worker_send_queue_config.queue_instance
        self.send_queue_batch_size: int = worker_send_queue_config.queue_batch_size
        self.cancelled_or_continue_processing_condition: threading.Condition = threading.Condition()

    def notify_to_continue_processing(self) -> None:
        """Notify the producer to continue producing new elements. This can also be used to rate-limit the producer to only emit new batches when the consumer is ready."""
        with self.cancelled_or_continue_processing_condition:
            self.cancelled_or_continue_processing_condition.notify()

    def request_cancellation(self) -> None:
        """Request a cancellation of the long running producer operation in a thread-safe manner."""
        self.cancellation_requested_flag.set()
        self.notify_to_continue_processing()

    def is_cancellation_requested(self) -> bool:
        """Check whether cancellation of the long running operation is requested in a thread-safe manner."""
        return self.cancellation_requested_flag.is_set()

    def _can_continue_processing_or_is_cancellation_requested(self) -> bool:
        """Check whether the consumer has dequeued all elements from the producer queue (i.e. this worker) or if cancellation was requested"""
        return self.send_queue.empty() or self.is_cancellation_requested()

    def _wait_on_cancellation_or_input_data(self) -> None:
        """Block the caller of this function until either cancellation is requested or when the consumer has dequeued all items from the producer queue (i.e. this worker)"""
        with self.cancelled_or_continue_processing_condition:
            self.cancelled_or_continue_processing_condition.wait_for(
                self._can_continue_processing_or_is_cancellation_requested
            )

    def _assert_valid_user_provided_parameter_values(self) -> None:
        if self.send_queue_batch_size < 1:
            msg = f"Send queue batch size must be larger than 0 but was actually {self.send_queue_batch_size}!"
            raise ValueError(msg)

    @staticmethod
    def get_timestamp() -> float:
        return time.perf_counter()

    @staticmethod
    def calc_batch_duration_and_return_end_timestamp_in_seconds(batch_start_timestamp: float) -> BatchTimestamps:
        batch_end_timestamp: Final[float] = CancellableProducerWorker.get_timestamp()
        batch_duration = batch_end_timestamp - batch_start_timestamp
        return BatchTimestamps(batch_start_timestamp, batch_end_timestamp, batch_duration)


class CancellableProducerConsumerWorker(
    CancellableProducerWorker[SendQueueElemType], Generic[RecvQueueElemType, SendQueueElemType]
):
    requestingData = QtCore.pyqtSignal()  # noqa: N815

    def __init__(
        self,
        worker_send_queue_config: QueueConfig[SendQueueElemType],
        worker_recv_queue_config: QueueConfig[RecvQueueElemType | None],
    ) -> None:
        super().__init__(worker_send_queue_config)
        # The requirement to cancel the long running operation performed by the worker "forces" us to use the non-blocking get_nowait(...) and put_nowait(...) functions of the unbounded
        # queue.SimpleQueue container. Correctly handling the expected batch sizes is the responsibility of the user of this unbounded queue.
        self.recv_queue: queue.SimpleQueue[RecvQueueElemType | None] = worker_recv_queue_config.queue_instance
        self.recv_queue_batch_size: int = worker_recv_queue_config.queue_batch_size

    @override
    def _can_continue_processing_or_is_cancellation_requested(self) -> bool:
        """Check whether elements in the receive queue exist or if cancellation was requested"""
        return not self.recv_queue.empty() or self.is_cancellation_requested()

    @override
    def _wait_on_cancellation_or_input_data(self) -> None:
        """Block the caller of this function until either new elements in the receive queue exist or cancelation was requested"""
        with self.cancelled_or_continue_processing_condition:
            self.cancelled_or_continue_processing_condition.wait_for(
                self._can_continue_processing_or_is_cancellation_requested
            )

    @override
    def _assert_valid_user_provided_parameter_values(self) -> None:
        super()._assert_valid_user_provided_parameter_values()
        if self.recv_queue_batch_size < 1:
            msg = f"Receive queue batch size must be larger than 0 but was actually {self.recv_queue_batch_size}!"
            raise ValueError(msg)
