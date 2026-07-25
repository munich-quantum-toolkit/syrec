# Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
# Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

"""MQT SyReC library.

This file is part of the MQT SyReC library released under the MIT license.
See README.md or go to https://github.com/munich-quantum-toolkit/syrec for more information.
"""

from __future__ import annotations

import sys

# under Windows, make sure to add the appropriate DLL directory to the PATH
if sys.platform == "win32":  # ruff:ignore[non-empty-init-module] This is actually required on Windows

    def _dll_patch() -> None:
        """Add the DLL directory to the PATH."""
        import os  # ruff:ignore[import-outside-top-level] because only needed on Windows
        import sysconfig  # ruff:ignore[import-outside-top-level] because only needed on Windows
        from pathlib import Path  # ruff:ignore[import-outside-top-level] because only needed on Windows

        bin_dir = Path(sysconfig.get_paths()["purelib"]) / "mqt" / "core" / "bin"
        os.add_dll_directory(str(bin_dir))

    _dll_patch()
    del _dll_patch

from ._version import version as __version__
from .pysyrec import (
    AnnotatableQuantumComputation,
    ConfigurableOptions,
    InlinedQubitInformation,
    IntegerConstantTruncationOperation,
    NBitValuesContainer,
    Program,
    QubitInliningStack,
    QubitInliningStackEntry,
    QubitLabelType,
    Statistics,
    cost_aware_synthesis,
    line_aware_synthesis,
    simple_simulation,
)

__all__ = [
    "AnnotatableQuantumComputation",
    "ConfigurableOptions",
    "InlinedQubitInformation",
    "IntegerConstantTruncationOperation",
    "NBitValuesContainer",
    "Program",
    "QubitInliningStack",
    "QubitInliningStackEntry",
    "QubitLabelType",
    "Statistics",
    "__version__",
    "cost_aware_synthesis",
    "line_aware_synthesis",
    "simple_simulation",
]
