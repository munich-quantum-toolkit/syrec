# Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
# Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

from __future__ import annotations

import json
from pathlib import Path
from typing import Any

import pytest

from mqt import syrec

test_dir = Path(__file__).resolve().parent.parent
circuit_dir = test_dir / "circuits"
configs_dir = test_dir / "configs"
simulation_test_data_dir = test_dir / "unittests" / "simulation" / "data"


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
    with (simulation_test_data_dir / "test_line_aware_synthesis_of_full_circuits.json").open() as f:
        return json.load(f)


@pytest.fixture
def data_cost_aware_simulation():
    with (simulation_test_data_dir / "test_cost_aware_synthesis_of_full_circuits.json").open() as f:
        return json.load(f)


def init_n_bit_values_container_with_expected_state(
    state_container: syrec.n_bit_values_container, stringified_state: str
) -> None:
    assert len(stringified_state) <= state_container.size()
    for idx, stringified_qubit_value in enumerate(stringified_state):
        assert stringified_qubit_value in {"0", "1"}
        if stringified_qubit_value == "1":
            state_container.set(idx)


def compare_some_values_of_n_bit_values_container(
    n: int, expected: syrec.n_bit_values_container, actual: syrec.n_bit_values_container
) -> None:
    assert n > 0
    assert n <= expected.size()
    assert expected.size() == actual.size()

    for i in range(n):
        assert expected[i] == actual[i]


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
        assert data_line_aware_synthesis[file_name]["num_gates"] == annotatable_quantum_computation.num_ops
        assert data_line_aware_synthesis[file_name]["lines"] == annotatable_quantum_computation.num_qubits
        assert (
            data_line_aware_synthesis[file_name]["quantum_costs"]
            == annotatable_quantum_computation.get_quantum_cost_for_synthesis()
        )
        assert (
            data_line_aware_synthesis[file_name]["transistor_costs"]
            == annotatable_quantum_computation.get_transistor_cost_for_synthesis()
        )


def test_synthesis_add_lines(data_cost_aware_synthesis: dict[str, Any]) -> None:
    for file_name in data_cost_aware_synthesis:
        annotatable_quantum_computation = syrec.annotatable_quantum_computation()
        prog = syrec.program()
        error = prog.read(str(circuit_dir / (file_name + ".src")))

        assert not error
        assert syrec.cost_aware_synthesis(annotatable_quantum_computation, prog)
        assert data_cost_aware_synthesis[file_name]["num_gates"] == annotatable_quantum_computation.num_ops
        assert data_cost_aware_synthesis[file_name]["lines"] == annotatable_quantum_computation.num_qubits
        assert (
            data_cost_aware_synthesis[file_name]["quantum_costs"]
            == annotatable_quantum_computation.get_quantum_cost_for_synthesis()
        )
        assert (
            data_cost_aware_synthesis[file_name]["transistor_costs"]
            == annotatable_quantum_computation.get_transistor_cost_for_synthesis()
        )


def test_simulation_no_lines(data_line_aware_simulation: dict[str, Any]) -> None:
    for test_case_name in data_line_aware_simulation:
        annotatable_quantum_computation = syrec.annotatable_quantum_computation()
        prog = syrec.program()
        errors = prog.read_from_string(data_line_aware_simulation[test_case_name]["inputCircuit"])

        assert not errors
        assert syrec.line_aware_synthesis(annotatable_quantum_computation, prog)

        for simulation_run_data in data_line_aware_simulation[test_case_name]["simulationRuns"]:
            input_state = syrec.n_bit_values_container(annotatable_quantum_computation.num_qubits)
            expected_output_state = syrec.n_bit_values_container(input_state.size())
            actual_output_state = syrec.n_bit_values_container(input_state.size())

            init_n_bit_values_container_with_expected_state(input_state, simulation_run_data["in"])
            init_n_bit_values_container_with_expected_state(expected_output_state, simulation_run_data["out"])

            syrec.simple_simulation(actual_output_state, annotatable_quantum_computation, input_state)
            compare_some_values_of_n_bit_values_container(
                annotatable_quantum_computation.num_data_qubits, expected_output_state, actual_output_state
            )


def test_simulation_add_lines(data_cost_aware_simulation: dict[str, Any]) -> None:
    for test_case_name in data_cost_aware_simulation:
        annotatable_quantum_computation = syrec.annotatable_quantum_computation()
        prog = syrec.program()
        errors = prog.read_from_string(data_cost_aware_simulation[test_case_name]["inputCircuit"])

        assert not errors
        assert syrec.cost_aware_synthesis(annotatable_quantum_computation, prog)

        for simulation_run_data in data_cost_aware_simulation[test_case_name]["simulationRuns"]:
            input_state = syrec.n_bit_values_container(annotatable_quantum_computation.num_qubits)
            expected_output_state = syrec.n_bit_values_container(input_state.size())
            actual_output_state = syrec.n_bit_values_container(input_state.size())

            init_n_bit_values_container_with_expected_state(input_state, simulation_run_data["in"])
            init_n_bit_values_container_with_expected_state(expected_output_state, simulation_run_data["out"])

            syrec.simple_simulation(actual_output_state, annotatable_quantum_computation, input_state)
            compare_some_values_of_n_bit_values_container(
                annotatable_quantum_computation.num_data_qubits, expected_output_state, actual_output_state
            )


def test_no_lines_to_qasm(data_line_aware_synthesis: dict[str, Any]) -> None:
    for file_name in data_line_aware_synthesis:
        expected_qasm_file_path = Path(str(circuit_dir / (file_name + ".qasm")))
        # Remove .qasm file generated by a previous test run while ignoring file not found exceptions
        expected_qasm_file_path.unlink(missing_ok=True)

        annotatable_quantum_computation = syrec.annotatable_quantum_computation()
        prog = syrec.program()
        prog.read(str(circuit_dir / (file_name + ".src")))

        annotatable_quantum_computation.qasm3(str(expected_qasm_file_path))
        assert Path.is_file(expected_qasm_file_path)
