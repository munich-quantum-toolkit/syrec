# Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
# Copyright (c) 2025 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

from __future__ import annotations

from PyQt6 import QtCore, QtWidgets

from mqt import syrec


class InputOutputStateMappingDefinitionWidget(QtWidgets.QWidget):  # type: ignore[misc]
    input_state_qubit_value_checkbox_clicked = QtCore.pyqtSignal(
        int, bool, arguments=["relative_qubit_idx", "new_qubit_value"], name="inputStateQubitValueCheckboxClicked"
    )
    output_state_qubit_value_checkbox_clicked = QtCore.pyqtSignal(
        int, bool, arguments=["relative_qubit_idx", "new_qubit_value"], name="outputStateQubitValueCheckboxClicked"
    )

    def __init__(
        self,
        simulation_run_number: int,
        annotatable_quantum_computation: syrec.annotatable_quantum_computation,
        associated_quantum_register_label: str,
        first_qubit_of_quantum_register: int,
        initial_input_state: syrec.n_bit_values_container,
        optional_initial_output_state: syrec.n_bit_values_container | None,
    ) -> None:
        # parent: QtWidgets.QWidget) -> None:
        super().__init__()

        # TODO: Validation that input and output state have same size (validate all input parameters)
        # TODO: Define validator for input and output state inputs
        # TODO: Update input/output state value when qubit value is changed
        # TODO: How to render n-dimensional variables

        self.input_state_qubit_checkbox_name_format = "q_{relative_qubit_idx:d}_in_checkB"
        self.output_state_qubit_checkbox_name_format = "q_{relative_qubit_idx:d}_out_checkB"
        self.stringified_qubit_value_format = "(Value: {stringified_qubit_value:s})"

        main_layout = QtWidgets.QVBoxLayout()
        self.setLayout(main_layout)

        simulation_run_wrapper_box = QtWidgets.QGroupBox("Simulation run #" + str(simulation_run_number))
        # main_layout.addWidget(simulation_run_wrapper_box)

        # TODO: How can we determine whether qubits are readonly
        self.are_qubits_values_readonly: bool = initial_input_state.size() == 0
        self.edit_of_qubit_values_enabled: bool = False

        self.input_state: syrec.n_bit_values_container = initial_input_state
        self.output_state: syrec.n_bit_values_container | None = optional_initial_output_state

        input_column_label = QtWidgets.QLabel("Input")
        output_column_label = QtWidgets.QLabel("Output")
        quantum_register_label = QtWidgets.QLabel("Quantum register: " + associated_quantum_register_label)

        # TODO: Add validators
        self.input_state_edit_field = QtWidgets.QLineEdit(str(initial_input_state))
        self.input_state_edit_field.setReadOnly(not self.are_qubits_values_readonly)

        self.output_state_edit_field = QtWidgets.QLineEdit()
        if optional_initial_output_state is not None:
            self.output_state_edit_field.setText(str(optional_initial_output_state))
            self.input_state_edit_field.setReadOnly(not self.are_qubits_values_readonly)
        else:
            self.output_state_edit_field.setEnabled(False)
            self.output_state_edit_field.setPlaceholderText("-")

        self.view_qubit_values_toggle_button = QtWidgets.QPushButton("Edit qubit values")

        group_box_layout = QtWidgets.QVBoxLayout()
        simulation_run_wrapper_box.setLayout(group_box_layout)

        quantum_register_controls_grid_layout = QtWidgets.QGridLayout()
        # Grid position component order is row followed by column
        quantum_register_controls_grid_layout.addWidget(
            input_column_label, 0, 1, alignment=QtCore.Qt.AlignmentFlag.AlignCenter
        )
        quantum_register_controls_grid_layout.addWidget(
            output_column_label, 0, 2, alignment=QtCore.Qt.AlignmentFlag.AlignCenter
        )
        quantum_register_controls_grid_layout.addWidget(
            quantum_register_label, 1, 0, alignment=QtCore.Qt.AlignmentFlag.AlignRight
        )
        quantum_register_controls_grid_layout.addWidget(
            self.input_state_edit_field, 1, 1, alignment=QtCore.Qt.AlignmentFlag.AlignRight
        )
        quantum_register_controls_grid_layout.addWidget(
            self.output_state_edit_field, 1, 2, alignment=QtCore.Qt.AlignmentFlag.AlignRight
        )
        quantum_register_controls_grid_layout.addWidget(self.view_qubit_values_toggle_button, 1, 3)
        group_box_layout.addLayout(quantum_register_controls_grid_layout)

        self.n_qubits_of_quantum_register = initial_input_state.size()
        if self.n_qubits_of_quantum_register > 0 and not self.are_qubits_values_readonly:
            self.qubit_values_grid_layout = QtWidgets.QGridLayout()

            for qubit in range(first_qubit_of_quantum_register, self.n_qubits_of_quantum_register):
                fetched_internal_qubit_label: str | None = annotatable_quantum_computation.get_qubit_label(
                    qubit, syrec.qubit_label_type.internal
                )
                qubit_label = QtWidgets.QLabel(
                    "Qubit: " + fetched_internal_qubit_label
                    if fetched_internal_qubit_label is not None
                    else "<UNKNOWN>"
                )
                self.qubit_values_grid_layout.addWidget(qubit_label, qubit, 0)

                relative_qubit_idx_in_n_bit_container: int = qubit - first_qubit_of_quantum_register
                input_state_qubit_value_checkbox = QtWidgets.QCheckBox(
                    objectName=self.input_state_qubit_checkbox_name_format.format(
                        relative_qubit_idx=relative_qubit_idx_in_n_bit_container
                    )
                )
                input_state_qubit_value_checkbox.setText(
                    self.stringified_qubit_value_format.format(
                        stringified_qubit_value=self.stringify_qubit_value(
                            self.input_state.test(relative_qubit_idx_in_n_bit_container)
                        )
                    )
                )
                input_state_qubit_value_checkbox.stateChanged.connect(
                    lambda relative_qubit_idx=relative_qubit_idx_in_n_bit_container: self.handle_input_state_qubit_value_checkbox_state_change(
                        relative_qubit_idx, self.state_changed == QtCore.Qt.CheckState.Checked
                    )
                )
                self.qubit_values_grid_layout.addWidget(
                    input_state_qubit_value_checkbox, qubit, 1, alignment=QtCore.Qt.AlignmentFlag.AlignRight
                )

                output_state_qubit_value_checkbox = QtWidgets.QCheckBox(
                    objectName=self.output_state_qubit_checkbox_name_format.format(
                        relative_qubit_idx=relative_qubit_idx_in_n_bit_container
                    )
                )
                output_state_qubit_value_checkbox.setText(
                    self.stringified_qubit_value_format.format(
                        stringified_qubit_value=self.stringify_qubit_value(
                            None
                            if self.output_state is None
                            else self.output_state.test(relative_qubit_idx_in_n_bit_container)
                        )
                    )
                )
                output_state_qubit_value_checkbox.stateChanged.connect(
                    lambda relative_qubit_idx=relative_qubit_idx_in_n_bit_container: self.handle_output_state_qubit_value_checkbox_state_change(
                        relative_qubit_idx, self.state_changed == QtCore.Qt.CheckState.Checked
                    )
                )
                output_state_qubit_value_checkbox.setEnabled(False)
                self.qubit_values_grid_layout.addWidget(
                    output_state_qubit_value_checkbox, qubit, 2, alignment=QtCore.Qt.AlignmentFlag.AlignRight
                )

            group_box_layout.addLayout(self.qubit_values_grid_layout)

        simulation_run_scroll_area = QtWidgets.QScrollArea()
        simulation_run_scroll_area.setWidget(simulation_run_wrapper_box)
        main_layout.addWidget(simulation_run_scroll_area)

    # TODO: Update n_bit_values_container and parent textfield
    def handle_input_state_qubit_value_checkbox_state_change(
        self, relative_qubit_index_in_n_bit_values_container: int, qubit_value: bool
    ) -> None:
        associated_qubit_value_checkbox: QtWidgets.QCheckBox | None = self.qubit_values_grid_layout.findChild(
            QtWidgets.QCheckBox,
            self.input_state_qubit_checkbox_name_format.format(
                relative_qubit_idx=relative_qubit_index_in_n_bit_values_container
            ),
        )
        if associated_qubit_value_checkbox is None:
            return

        associated_qubit_value_checkbox.setText(
            self.stringified_qubit_value_format.format(stringified_qubit_value=self.stringify_qubit_value(qubit_value))
        )

    # TODO: Update n_bit_values_container and parent textfield
    def handle_output_state_qubit_value_checkbox_state_change(
        self, relative_qubit_index_in_n_bit_values_container: int, qubit_value: bool
    ) -> None:
        associated_qubit_value_checkbox: QtWidgets.QCheckBox | None = self.qubit_values_grid_layout.findChild(
            QtWidgets.QCheckBox,
            self.output_state_qubit_checkbox_name_format.format(
                relative_qubit_idx=relative_qubit_index_in_n_bit_values_container
            ),
        )
        if associated_qubit_value_checkbox is None:
            return

        associated_qubit_value_checkbox.setText(
            self.stringified_qubit_value_format.format(stringified_qubit_value=self.stringify_qubit_value(qubit_value))
        )

    @staticmethod
    def stringify_qubit_value(qubit_value: bool | None) -> str:
        if qubit_value is None:
            return "UNKNOWN"
        return "HIGH" if qubit_value else "LOW"

    def handle_edit_qubit_values_toggle_button_click(self) -> None:
        if self.edit_of_qubit_values_enabled:
            return

        if self.output_state is None:
            self.output_state = syrec.n_bit_values_container(self.input_state.size())
            for qubit in range(self.output_state.size()):
                if self.input_state.test(qubit):
                    self.output_state.set(qubit)


class SimulationRunDefinitionWidget(QtWidgets.QWidget):  # type: ignore[misc]
    def __init__(
        self,
        simulation_run_idx: int,  # noqa: ARG002
        annotatable_quantum_computation: syrec.annotatable_quantum_computation,  # noqa: ARG002
        is_delete_action_enabled: bool,  # noqa: ARG002
        parent: QtWidgets.QWidget,  # noqa: ARG002
    ) -> None:
        super().__init__()

        self.layout = QtWidgets.QVBoxLayout()
        self.setLayout(self.layout)


class QuantumCircuitSimulationDialog(QtWidgets.QDialog):  # type: ignore[misc]
    def __init__(
        self, annotatable_quantum_computation: syrec.annotatable_quantum_computation | None, parent: QtWidgets.QWidget
    ) -> None:
        super().__init__()
        self.parent = parent
        self.annotatable_quantum_computation = annotatable_quantum_computation

        self.title = "Define simulation runs for quantum computation"
        self.setWindowTitle(self.title)

        self.left = 0
        self.top = 0
        self.width = 600
        self.height = 400
        self.setGeometry(self.left, self.top, self.width, self.height)

        self.simulation_runs_tab_widget = QtWidgets.QTabWidget(self)
        self.simulation_runs_tab_widget.addTab(
            self.initialize_some_simulation_runs_tab(), "Check some input-output mapping combinations"
        )
        self.simulation_runs_tab_widget.addTab(
            self.initialize_all_simulation_runs_tab(), "Check all input-output mapping combinations"
        )
        self.simulation_runs_tab_widget.addTab(
            self.initialize_simulation_runs_from_file_tab(), "Check input-output mapping combinations from file"
        )
        self.simulation_runs_tab_widget.tabBarClicked.connect(self.handle_simulation_runs_tab_widget_tab_bar_clicked)

        self.layout = QtWidgets.QVBoxLayout()
        self.layout.addStretch()
        self.layout.addWidget(self.simulation_runs_tab_widget)
        self.setLayout(self.layout)

    def initialize_some_simulation_runs_tab(self) -> QtWidgets.QTabWidget:
        simulation_runs_list_layout = QtWidgets.QVBoxLayout()

        in_state = syrec.n_bit_values_container(10)
        out_state = syrec.n_bit_values_container(10)
        simulation_run_one = InputOutputStateMappingDefinitionWidget(
            0, self.annotatable_quantum_computation, "a", 0, in_state, None
        )
        simulation_run_two = InputOutputStateMappingDefinitionWidget(
            1, self.annotatable_quantum_computation, "a", 2, in_state, None
        )
        simulation_run_three = InputOutputStateMappingDefinitionWidget(
            2, self.annotatable_quantum_computation, "a", 4, in_state, out_state
        )
        simulation_runs_list_layout.addWidget(simulation_run_one)
        simulation_runs_list_layout.addWidget(simulation_run_two)
        simulation_runs_list_layout.addWidget(simulation_run_three)
        simulation_runs_list_layout.addStretch()

        tab_widget = QtWidgets.QTabWidget()
        tab_widget.setLayout(simulation_runs_list_layout)
        return tab_widget

    @staticmethod
    def initialize_all_simulation_runs_tab() -> QtWidgets.QTabWidget:
        return QtWidgets.QTabWidget()

    @staticmethod
    def initialize_simulation_runs_from_file_tab() -> QtWidgets.QTabWidget:
        return QtWidgets.QTabWidget()

    def handle_simulation_runs_tab_widget_tab_bar_clicked(self, clicked_on_tab_index: int) -> None:
        self.simulation_runs_tab_widget.setCurrentIndex(clicked_on_tab_index)
