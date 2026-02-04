# Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
# Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

"""
Python interface for the SyReC programming language for the synthesis of reversible circuits
"""

import enum
from typing import overload

import mqt.core.ir

class QubitInliningStackEntry:
    def __init__(self) -> None:
        """Constructs an empty qubit inlining stack entry"""

    @property
    def line_number_of_call_of_target_module(self) -> int | None:
        """
        Returns the line number in the source file in which the call statement variant was defined
        """

    @property
    def is_target_module_accessed_via_call_stmt(self) -> bool | None:
        """Returns whether the target module was called using a CallStatement"""

    @property
    def stringified_signature_of_called_module(self) -> str | None:
        """Returns the stringified target module signature"""

class QubitInliningStack:
    def __init__(self) -> None:
        """Constructs an empty qubit inlining stack"""

    def size(self) -> int:
        """Get the number of stack entries"""

    def __getitem__(self, idx: int) -> QubitInliningStackEntry: ...

class InlinedQubitInformation:
    def __init__(self) -> None:
        """Constructs an empty inlined qubit information container"""

    @property
    def user_declared_qubit_label(self) -> str | None:
        """Get the label of the qubit as defined by the user in the SyReC program"""

    @property
    def inline_stack(self) -> QubitInliningStack | None:
        """Get the inline stack associated with the qubit"""

class QubitLabelType(enum.Enum):
    internal = 0
    """
    Generate the qubit label using the internal qubit identifier (only available for ancillary qubits and local SyReC module variables)
    """

    user_declared = 1
    """
    Generate the qubit label using the user declared variable identifier (only available for the qubits of the variables of a SyReC program [ancillary qubits are not associated with a variable and thus have no user declared label])
    """

class AnnotatableQuantumComputation(mqt.core.ir.QuantumComputation):
    @overload
    def __init__(self) -> None:
        """Constructs an annotatable quantum computation"""

    @overload
    def __init__(self, generate_quantum_operation_annotations: bool) -> None:
        """
        Constructs an annotatable quantum computation while also specifying whether quantum operation annotations can be generated
        """

    def get_qubit_label(self, qubit: int, qubit_label_type: QubitLabelType) -> str | None:
        """
        Get either the internal or user-declared label of a qubit as a stringified SyReC variable access based on its location in the quantum register storing the qubit and, optionally, the layout of the SyReC variable stored in the register.
        """

    def get_quantum_cost_for_synthesis(self) -> int:
        """Get the quantum cost to synthesis the quantum computation"""

    def get_transistor_cost_for_synthesis(self) -> int:
        """Get the transistor cost to synthesis the quantum computation"""

    def get_annotations_of_quantum_operation(self, quantum_operation_index_in_quantum_operation: int) -> dict[str, str]:
        """
        Get the annotations of a specific quantum operation in the quantum computation
        """

    def get_inlined_qubit_information(self, qubit: int) -> InlinedQubitInformation | None:
        """Get the inlined information of a qubit"""

class NBitValuesContainer:
    @overload
    def __init__(self) -> None:
        """Constructs an empty container of size zero."""

    @overload
    def __init__(self, n: int) -> None:
        """Constructs a zero-initialized container of size n."""

    @overload
    def __init__(self, n: int, initial_line_values: int) -> None:
        """Constructs a container of size n from an integer initial_line_values"""

    def __getitem__(self, arg: int, /) -> bool: ...
    def test(self, n: int) -> bool | None:
        """Determine the value of the bit at position n"""

    @overload
    def set(self, n: int) -> bool:
        """Set the value of the bit at position n to TRUE"""

    @overload
    def set(self, n: int, value: bool) -> bool:
        """Set the bit at position n to a specific value"""

    def reset(self, n: int) -> bool:
        """Set the value of the bit at position n to FALSE"""

    def resize(self, n: int) -> None:
        """Changes the number of bits stored in the container"""

    def size(self) -> int:
        """Get the number of values stored in the container"""

    def flip(self, n: int) -> bool:
        """Flip the value of the bit at position n"""

class Statistics:
    def __init__(self) -> None:
        """Constructs an object to record collected statistics."""

    @property
    def runtime_in_milliseconds(self) -> float:
        """The recorded runtime in milliseconds"""

    @runtime_in_milliseconds.setter
    def runtime_in_milliseconds(self, arg: float, /) -> None: ...

class IntegerConstantTruncationOperation(enum.Enum):
    modulo = 0
    """Use the modulo operation for the truncation of constant values"""

    bitwise_and = 1
    """Use the bitwise AND operation for the truncation of constant values"""

class ConfigurableOptions:
    def __init__(self) -> None:
        """Constructs a configurable options object."""

    @property
    def default_bitwidth(self) -> int:
        """
        Defines the default variable bitwidth used by the SyReC parser for variables whose bitwidth specification was omitted
        """

    @default_bitwidth.setter
    def default_bitwidth(self, arg: int, /) -> None: ...
    @property
    def integer_constant_truncation_operation(self) -> IntegerConstantTruncationOperation:
        """
        Defines the operation used by the SyReC parser for the truncation of integer constant values. For further details we refer to the semantics of the SyReC language
        """

    @integer_constant_truncation_operation.setter
    def integer_constant_truncation_operation(self, arg: IntegerConstantTruncationOperation, /) -> None: ...
    @property
    def allow_access_on_assigned_to_variable_parts_in_dimension_access_of_variable_access(self) -> bool:
        """
        Defines whether an access on the assigned to signal parts of an assigned is allowed in variable accesses defined in any operand of the assignment. For further details we refer to the semantics of the SyReC language.
        """

    @allow_access_on_assigned_to_variable_parts_in_dimension_access_of_variable_access.setter
    def allow_access_on_assigned_to_variable_parts_in_dimension_access_of_variable_access(
        self, arg: bool, /
    ) -> None: ...
    @property
    def main_module_identifier(self) -> str | None:
        """
        Define the identifier of the module serving as the entry-point of the to be processed SyReC program
        """

    @main_module_identifier.setter
    def main_module_identifier(self, arg: str) -> None: ...
    @property
    def generate_inlined_qubit_debug_information(self) -> bool:
        """
        Should debug information for the qubits associated with the local variables of a SyReC module be generated
        """

    @generate_inlined_qubit_debug_information.setter
    def generate_inlined_qubit_debug_information(self, arg: bool, /) -> None: ...
    @property
    def generate_quantum_operation_annotations(self) -> bool:
        """
        Should the optional quantum operation annotations be generated during the synthesis of a SyReC program, disabled by default
        """

    @generate_quantum_operation_annotations.setter
    def generate_quantum_operation_annotations(self, arg: bool, /) -> None: ...

class Program:
    def __init__(self) -> None:
        """Constructs SyReC program object."""

    def read(self, filename: str, configurable_options: ConfigurableOptions = ...) -> str:
        """Read and process a SyReC program from a file."""

    def read_from_string(self, stringified_program: str, configurable_options: ConfigurableOptions = ...) -> str:
        """Process an already stringified SyReC program."""

def cost_aware_synthesis(
    annotated_quantum_computation: AnnotatableQuantumComputation,
    program: Program,
    configurable_options: ConfigurableOptions = ...,
    optional_recorded_statistics: Statistics | None = None,
) -> bool:
    """Cost-aware synthesis of the SyReC program."""

def line_aware_synthesis(
    annotated_quantum_computation: AnnotatableQuantumComputation,
    program: Program,
    configurable_options: ConfigurableOptions = ...,
    optional_recorded_statistics: Statistics | None = None,
) -> bool:
    """Line-aware synthesis of the SyReC program."""

def simple_simulation(
    output: NBitValuesContainer,
    quantum_computation: mqt.core.ir.QuantumComputation,
    input_: NBitValuesContainer,
    optional_recorded_statistics: Statistics | None = None,
) -> None:
    """Simulation of a synthesized SyReC program"""
