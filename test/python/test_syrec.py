# Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
# Copyright (c) 2025 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

from __future__ import annotations

import json
from pathlib import Path
from typing import Any

import os
import pytest

from mqt import syrec

test_dir = Path(__file__).resolve().parent.parent
configs_dir = test_dir / "configs"
circuit_dir = test_dir / "circuits"


@pytest.fixture
def data_line_aware_synthesis():
    with (configs_dir / "circuits_line_aware_synthesis.json").open() as f:
        return json.load(f)


@pytest.fixture
def data_cost_aware_synthesis():
    with (configs_dir / "circuits_cost_aware_synthesis.json").open() as f:
        return json.load(f)


@pytest.fixture
def data_line_aware_simulation():
    with (configs_dir / "circuits_line_aware_simulation.json").open() as f:
        return json.load(f)


@pytest.fixture
def data_cost_aware_simulation():
    with (configs_dir / "circuits_cost_aware_simulation.json").open() as f:
        return json.load(f)


def test_parser(data_line_aware_synthesis: dict[str, Any]) -> None:
    for file_name in data_line_aware_synthesis:
        prog = syrec.program()
        error = prog.read(str(circuit_dir / (file_name + ".src")))

        assert not error


def test_synthesis_no_lines(data_line_aware_synthesis: dict[str, Any]) -> None:
    for file_name in data_line_aware_synthesis:
        annotatable_quantum_computation = syrec.annotatable_quantum_computation()
        prog = syrec.program()
        error = prog.read(str(circuit_dir / (file_name + ".src")))

        assert not error
        assert syrec.line_aware_synthesis(annotatable_quantum_computation, prog)
        assert data_line_aware_synthesis[file_name]["num_gates"] == circ.num_ops
        assert data_line_aware_synthesis[file_name]["lines"] == circ.num_qubits
        assert data_line_aware_synthesis[file_name]["quantum_costs"] == circ.get_quantum_cost_for_synthesis()
        assert data_line_aware_synthesis[file_name]["transistor_costs"] == circ.get_transistor_cost_for_synthesis()


def test_synthesis_add_lines(data_cost_aware_synthesis: dict[str, Any]) -> None:
    for file_name in data_cost_aware_synthesis:
        annotatable_quantum_computation = syrec.annotatable_quantum_computation()
        prog = syrec.program()
        error = prog.read(str(circuit_dir / (file_name + ".src")))

        assert not error
        assert syrec.cost_aware_synthesis(circ, prog)
        assert data_cost_aware_synthesis[file_name]["num_gates"] == circ.num_ops
        assert data_cost_aware_synthesis[file_name]["lines"] == circ.num_qubits
        assert data_cost_aware_synthesis[file_name]["quantum_costs"] == circ.get_quantum_cost_for_synthesis()
        assert data_cost_aware_synthesis[file_name]["transistor_costs"] == circ.get_transistor_cost_for_synthesis()


def test_simulation_no_lines(data_line_aware_simulation: dict[str, Any]) -> None:
    for file_name in data_line_aware_simulation:
        annotatable_quantum_computation = syrec.annotatable_quantum_computation()
        prog = syrec.program()
        error = prog.read(str(circuit_dir / (file_name + ".src")))

        assert not error
        assert syrec.line_aware_synthesis(annotatable_quantum_computation, prog)

        input_qubit_values = [False] * annotatable_quantum_computation.num_data_qubits
        output_qubit_values = [False] * annotatable_quantum_computation.num_data_qubits
        set_list = data_line_aware_simulation[file_name]["set_lines"]

        for set_index in set_list:
            input_qubit_values[set_index] = True

        output_qubit_values = syrec.quantum_computation_simulation_for_state(annotatable_quantum_computation, input_qubit_values)
        assert output_qubit_values is not None
        assert len(output_qubit_values) == len(input_qubit_values)
        assert data_line_aware_simulation[file_name]["sim_out"] == str(output_qubit_values)


def test_simulation_add_lines(data_cost_aware_simulation: dict[str, Any]) -> None:
    for file_name in data_cost_aware_simulation:
        annotatable_quantum_computation = syrec.annotatable_quantum_computation()
        prog = syrec.program()
        error = prog.read(str(circuit_dir / (file_name + ".src")))

        assert not error
        assert syrec.cost_aware_synthesis(annotatable_quantum_computation, prog)

        input_qubit_values = [False] * annotatable_quantum_computation.num_data_qubits
        output_qubit_values = [False] * annotatable_quantum_computation.num_data_qubits
        set_list = data_cost_aware_simulation[file_name]["set_lines"]

        for set_index in set_list:
            input_qubit_values[set_index] = True

        output_qubit_values = syrec.quantum_computation_simulation_for_state(annotatable_quantum_computation, input_qubit_values)
        assert output_qubit_values is not None
        assert len(output_qubit_values) == len(input_qubit_values)
        assert data_cost_aware_simulation[file_name]["sim_out"] == str(output_qubit_values)


def test_no_lines_to_qasm(data_line_aware_synthesis: dict[str, Any]) -> None:
    for file_name in data_line_aware_synthesis:
        annotatable_quantum_computation = syrec.annotatable_quantum_computation()
        prog = syrec.program()
        prog.read(str(circuit_dir / (file_name + ".src")))

        expected_qasm_file_path = str(circuit_dir / (file_name + ".qasm"))
        annotatable_quantum_computation.qasm3(existed_qasm_file_path)
        assert os.path.isfile(expected_qasm_file_path) == True
