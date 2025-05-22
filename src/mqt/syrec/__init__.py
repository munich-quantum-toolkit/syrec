# Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
# Copyright (c) 2025 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

"""MQT SyReC library.

This file is part of the MQT SyReC library released under the MIT license.
See README.md or go to https://github.com/cda-tum/syrec for more information.
"""

from __future__ import annotations

from ._version import version as __version__
from .pysyrec import (
    annotatable_quantum_computation,
    cost_aware_synthesis,
    line_aware_synthesis,
    program,
    properties,
    quantum_computation_simulation_for_state,
    read_program_settings,
    simple_simulation,
)

__all__ = [
    "__version__",
    "annotatable_quantum_computation",
    "cost_aware_synthesis",
    "line_aware_synthesis",
    "program",
    "properties",
    "quantum_computation_simulation_for_state",
    "read_program_settings",
    "simple_simulation",
]
