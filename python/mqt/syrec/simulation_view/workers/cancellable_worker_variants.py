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
    """
    Defines a worker executing a long running operation producing batches of elements.

    While executing the long running operation the worker is able to emit the following signals:
        - finished(bool): Worker has finished its long running operation without errors, the boolean argument defines whether cancellation of the worker was requested before it complete normally.
        - failed(Exception): An exception occurred during the long running operation.
        - batchCompleted(float): The worker has produced a new batch of items, the runtime (in seconds) to produce a new batch is passed as the argument of the signal.

    Inputs:
        SendQueueElemType: The workers send queue element type.

    Attributes:
        send_queue: A thread-safe FIFO queue storing the elements produced by the worker. The queue is expected to be unbounded.
    """

    finished = QtCore.pyqtSignal(bool)
    failed = QtCore.pyqtSignal(Exception)
    batchCompleted = QtCore.pyqtSignal(float)  # noqa: N815

    def __init__(self, worker_send_queue_config: QueueConfig[SendQueueElemType]) -> None:
        super().__init__()
        # The requirement to cancel the long running operation performed by the worker "forces" us to use the non-blocking get_nowait(...) and put_nowait(...) functions of the unbounded
        # queue.SimpleQueue container. Correctly handling the expected batch sizes is the responsibility of the user of this unbounded queue.
        self.send_queue: queue.SimpleQueue[SendQueueElemType] = worker_send_queue_config.queue_instance
        # A thread-safe boolean flag to enable cooperative cancellation of the long running operation.
        self._cancellation_requested_flag: threading.Event = threading.Event()
        # Defines after how many elements the worker will emit the batchCompleted signal.
        self._send_queue_batch_size: int = worker_send_queue_config.queue_batch_size
        # A condition variable usable to control the production of new elements the production of new elements in the worker by notifying.
        self._cancelled_or_continue_processing_condition: threading.Condition = threading.Condition()

    def notify_to_continue_processing(self) -> None:
        """Notify the producer to continue producing new elements. This can also be used to rate-limit the producer to only emit new batches when the consumer is ready."""
        with self._cancelled_or_continue_processing_condition:
            self._cancelled_or_continue_processing_condition.notify()

    def request_cancellation(self) -> None:
        """Request a cancellation of the long running producer operation in a thread-safe manner."""
        self._cancellation_requested_flag.set()
        self.notify_to_continue_processing()

    def is_cancellation_requested(self) -> bool:
        """Check whether cancellation of the long running operation is requested in a thread-safe manner."""
        return self._cancellation_requested_flag.is_set()

    def _can_continue_processing_or_is_cancellation_requested(self) -> bool:
        """Check whether the consumer has dequeued all elements from the producer queue (i.e. this worker) or if cancellation was requested."""
        return self.send_queue.empty() or self.is_cancellation_requested()

    def _wait_on_cancellation_or_input_data(self) -> None:
        """Blocks until either cancellation is requested or when the send queue is empty."""
        with self._cancelled_or_continue_processing_condition:
            self._cancelled_or_continue_processing_condition.wait_for(
                self._can_continue_processing_or_is_cancellation_requested
            )

    def _assert_valid_user_provided_parameter_values(self) -> None:
        """
        Validate the user-provided worker configuration parameters.

        Raises:
            ValueError: An invalid value for the send queue batch size was passed.
        """
        if self._send_queue_batch_size < 1:
            msg = f"Send queue batch size must be larger than 0 but was actually {self._send_queue_batch_size}!"
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
    """
    Defines a worker executing a long running operation that both consumes and produces elements.

    While executing the long running operation the worker is able to emit the following signals in addition to the signals inherited from its base class:
        - requestingData: Emitted when the worker is about to run out of elements to consume.

    Inputs:
        RecvQueueElemType: The workers receive queue element type.
        SendQueueElemType: The workers send queue element type.

    Attributes:
        recv_queue: A thread-safe FIFO queue used by the worker to fetch new elements to consume from. The queue is expected to be unbounded.
    """

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
        # Defines how many elements the worker will consumer per batch.
        self._recv_queue_batch_size: int = worker_recv_queue_config.queue_batch_size

    @override
    def _can_continue_processing_or_is_cancellation_requested(self) -> bool:
        """Check whether elements in the receive queue exist or if cancellation was requested"""
        return not self.recv_queue.empty() or self.is_cancellation_requested()

    @override
    def _wait_on_cancellation_or_input_data(self) -> None:
        """Blocks until either new elements in the receive queue exist or cancelation was requested"""
        with self._cancelled_or_continue_processing_condition:
            self._cancelled_or_continue_processing_condition.wait_for(
                self._can_continue_processing_or_is_cancellation_requested
            )

    @override
    def _assert_valid_user_provided_parameter_values(self) -> None:
        """
        Validate the user-provided worker configuration parameters

        Raises:
            ValueError: An invalid value for the receive queue batch size was passed or validation of the base class parameters failed.
        """
        super()._assert_valid_user_provided_parameter_values()
        if self._recv_queue_batch_size < 1:
            msg = f"Receive queue batch size must be larger than 0 but was actually {self._recv_queue_batch_size}!"
            raise ValueError(msg)
