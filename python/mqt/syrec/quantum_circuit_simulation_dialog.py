# Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
# Copyright (c) 2025 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

from __future__ import annotations

from PyQt6 import QtCore, QtGui, QtWidgets

from mqt import syrec


def does_qubit_label_start_with_internal_qubit_label_prefix(qubit_label: str) -> bool:
    return qubit_label.startswith("__q")


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
        initial_input_state: syrec.n_bit_values_container,
        optional_initial_output_state: syrec.n_bit_values_container | None,
    ) -> None:
        # parent: QtWidgets.QWidget) -> None:
        super().__init__()

        self.annotatable_quantum_computation = annotatable_quantum_computation

        # TODO: Validation that input and output state have same size (validate all input parameters)
        # TODO: Define validator for input and output state inputs
        # TODO: Update input/output state value when qubit value is changed
        # TODO: How to render n-dimensional variables

        self.input_state_qubit_checkbox_name_format = "q_{relative_qubit_idx:d}_in_checkB"
        self.output_state_qubit_checkbox_name_format = "q_{relative_qubit_idx:d}_out_checkB"
        self.stringified_qubit_value_format = "(Value: {stringified_qubit_value:s})"
        self.qreg_qubit_values_groupbox_format = "qreg_{qreg_name}_qubit_values_groupbox"
        self.qreg_label_name_format = "qreg_{qreg_name}_label"
        self.qreg_input_state_input_field_name_format = "qreg_{qreg_name}_inputState"
        self.qreg_output_state_input_field_name_format = "qreg_{qreg_name}_outputState"
        self.qreg_qubit_values_toggle_button_name_format = "qreg_{qreg_name}_qubit_values_toggle"

        main_layout = QtWidgets.QVBoxLayout()
        self.setLayout(main_layout)
        self.simulation_run_wrapper_box = QtWidgets.QGroupBox("Simulation run #" + str(simulation_run_number))

        # TODO: How can we determine whether qubits are readonly
        self.are_qubits_values_readonly: bool = initial_input_state.size() == 0
        self.edit_of_qubit_values_enabled: bool = False

        self.input_state: syrec.n_bit_values_container = initial_input_state
        self.output_state: syrec.n_bit_values_container | None = optional_initial_output_state

        # TODO: Add validators
        self.quantum_register_controls_grid_layout = QtWidgets.QGridLayout()
        self.simulation_run_wrapper_box.setLayout(self.quantum_register_controls_grid_layout)

        quantum_register_search_controls_layout = QtWidgets.QHBoxLayout()
        quantum_register_search_label = QtWidgets.QLabel("Quantum register:")
        self.quantum_register_search_input_field = QtWidgets.QLineEdit()
        self.quantum_register_search_input_field.setPlaceholderText("<QUANTUM_REGISTER_NAME>")

        quantum_register_name_regular_expr = QtCore.QRegularExpression(R"(^([_A-Za-z]\w*)?$)")
        quantum_register_name_validator = QtGui.QRegularExpressionValidator(quantum_register_name_regular_expr, self)
        self.quantum_register_search_input_field.setValidator(quantum_register_name_validator)

        self.quantum_register_search_trigger_button = QtWidgets.QPushButton("Search")
        self.quantum_register_search_trigger_button.clicked.connect(self.handle_quantum_register_name_search)

        quantum_register_search_controls_layout.addWidget(quantum_register_search_label)
        quantum_register_search_controls_layout.addWidget(self.quantum_register_search_input_field)
        quantum_register_search_controls_layout.addWidget(self.quantum_register_search_trigger_button)
        self.quantum_register_controls_grid_layout.addLayout(
            quantum_register_search_controls_layout, 0, 0, alignment=QtCore.Qt.AlignmentFlag.AlignCenter
        )

        simulation_run_delete_button = QtWidgets.QPushButton("Delete simulation run")
        self.quantum_register_controls_grid_layout.addWidget(simulation_run_delete_button, 0, 5)

        # Grid position component order is row followed by column
        input_column_label = QtWidgets.QLabel("Input")
        output_column_label = QtWidgets.QLabel("Output")

        self.quantum_register_controls_grid_layout.addWidget(
            input_column_label, 1, 1, alignment=QtCore.Qt.AlignmentFlag.AlignCenter
        )
        self.quantum_register_controls_grid_layout.addWidget(
            output_column_label, 1, 2, alignment=QtCore.Qt.AlignmentFlag.AlignCenter
        )

        quantum_register_controls_grid_row: int = 2
        for qreg in annotatable_quantum_computation.qregs.values():
            first_qubit_of_qreg: int = qreg.start
            n_qubits_of_qreg: int = qreg.size

            # Skip ancillary quantum registers (we assume that ancillary quantum registers only store ancillary qubits thus only checking the first qubit of the quantum register is sufficient)
            # It is not sufficient to simply check via annotatable_quantum_computation.is_circuit_qubit_ancillary since this does not cover garbage qubits generated for local SyReC module variables.
            if n_qubits_of_qreg == 0 or does_qubit_label_start_with_internal_qubit_label_prefix(
                annotatable_quantum_computation.get_qubit_label(first_qubit_of_qreg, syrec.qubit_label_type.internal)
            ):
                continue

            quantum_register_label = QtWidgets.QLabel(
                "Quantum register: " + qreg.name, objectName=self.qreg_label_name_format.format(qreg_name=qreg.name)
            )

            input_state_edit_field = QtWidgets.QLineEdit(
                objectName=self.qreg_input_state_input_field_name_format.format(qreg_name=qreg.name)
            )
            input_state_edit_field.setText(str(initial_input_state))
            input_state_edit_field.setReadOnly(not self.are_qubits_values_readonly)

            output_state_edit_field = QtWidgets.QLineEdit(
                objectName=self.qreg_output_state_input_field_name_format.format(qreg_name=qreg.name)
            )
            if optional_initial_output_state is not None:
                output_state_edit_field.setText(str(optional_initial_output_state))
                input_state_edit_field.setReadOnly(not self.are_qubits_values_readonly)
            else:
                output_state_edit_field.setEnabled(False)
                output_state_edit_field.setPlaceholderText("-")

            edit_qubit_values_toggle_button = QtWidgets.QPushButton(
                "Edit qubit values",
                objectName=self.qreg_qubit_values_toggle_button_name_format.format(qreg_name=qreg.name),
            )
            # We need to ignore the checked parameter that is passed to the clicked slot of the QPushButton
            edit_qubit_values_toggle_button.clicked.connect(
                lambda _, associated_qreg_name=qreg.name: self.handle_qreg_qubit_values_edit_toggle_button_click(
                    associated_qreg_name
                )
            )

            self.quantum_register_controls_grid_layout.addWidget(
                quantum_register_label,
                quantum_register_controls_grid_row,
                0,
                alignment=QtCore.Qt.AlignmentFlag.AlignLeft,
            )
            self.quantum_register_controls_grid_layout.addWidget(
                input_state_edit_field,
                quantum_register_controls_grid_row,
                1,
                alignment=QtCore.Qt.AlignmentFlag.AlignRight,
            )
            self.quantum_register_controls_grid_layout.addWidget(
                output_state_edit_field,
                quantum_register_controls_grid_row,
                2,
                alignment=QtCore.Qt.AlignmentFlag.AlignRight,
            )
            self.quantum_register_controls_grid_layout.addWidget(
                edit_qubit_values_toggle_button, quantum_register_controls_grid_row, 3
            )
            n_cols_in_quantum_register_controls_grid_layout: int = 4

            # TODO: Scroll area
            input_output_qubits_value_controls_groupbox = QtWidgets.QGroupBox(
                "Qubit values", objectName=self.qreg_qubit_values_groupbox_format.format(qreg_name=qreg.name)
            )
            input_output_qubits_value_controls_groupbox_layout = QtWidgets.QGridLayout()
            input_output_qubits_value_controls_groupbox.setLayout(input_output_qubits_value_controls_groupbox_layout)

            for qubit in range(first_qubit_of_qreg, first_qubit_of_qreg + n_qubits_of_qreg):
                relative_qubit_idx_in_qreg: int = qubit - first_qubit_of_qreg
                fetched_internal_qubit_label: str | None = annotatable_quantum_computation.get_qubit_label(
                    qubit, syrec.qubit_label_type.internal
                )
                qubit_label = QtWidgets.QLabel(
                    "Qubit: " + fetched_internal_qubit_label
                    if fetched_internal_qubit_label is not None
                    else "<UNKNOWN>"
                )
                input_output_qubits_value_controls_groupbox_layout.addWidget(qubit_label, relative_qubit_idx_in_qreg, 0)

                input_state_qubit_value_checkbox = QtWidgets.QCheckBox(
                    objectName=self.input_state_qubit_checkbox_name_format.format(
                        relative_qubit_idx=relative_qubit_idx_in_qreg
                    )
                )
                input_state_qubit_value_checkbox.setText(
                    self.stringified_qubit_value_format.format(
                        stringified_qubit_value=self.stringify_qubit_value(self.input_state.test(qubit))
                    )
                )
                input_state_qubit_value_checkbox.stateChanged.connect(
                    lambda relative_qubit_idx=relative_qubit_idx_in_qreg: self.handle_input_state_qubit_value_checkbox_state_change(
                        relative_qubit_idx, self.state_changed == QtCore.Qt.CheckState.Checked
                    )
                )
                input_output_qubits_value_controls_groupbox_layout.addWidget(
                    input_state_qubit_value_checkbox,
                    relative_qubit_idx_in_qreg,
                    1,
                    alignment=QtCore.Qt.AlignmentFlag.AlignRight,
                )

                output_state_qubit_value_checkbox = QtWidgets.QCheckBox(
                    objectName=self.output_state_qubit_checkbox_name_format.format(
                        relative_qubit_idx=relative_qubit_idx_in_qreg
                    )
                )
                output_state_qubit_value_checkbox.setText(
                    self.stringified_qubit_value_format.format(
                        stringified_qubit_value=self.stringify_qubit_value(
                            None if self.output_state is None else self.output_state.test(qubit)
                        )
                    )
                )
                output_state_qubit_value_checkbox.stateChanged.connect(
                    lambda relative_qubit_idx=relative_qubit_idx_in_qreg: self.handle_output_state_qubit_value_checkbox_state_change(
                        relative_qubit_idx, self.state_changed == QtCore.Qt.CheckState.Checked
                    )
                )
                output_state_qubit_value_checkbox.setEnabled(False)
                input_output_qubits_value_controls_groupbox_layout.addWidget(
                    output_state_qubit_value_checkbox,
                    relative_qubit_idx_in_qreg,
                    2,
                    alignment=QtCore.Qt.AlignmentFlag.AlignRight,
                )

            quantum_register_controls_grid_row += 1
            input_output_qubits_value_controls_groupbox.setVisible(False)
            self.quantum_register_controls_grid_layout.addWidget(
                input_output_qubits_value_controls_groupbox,
                quantum_register_controls_grid_row,
                0,
                1,
                n_cols_in_quantum_register_controls_grid_layout,
            )

            # Add a spacer item that will take the remaining horizontal space in the grid layout for each quantum register while vertical resizing should only take the minimum required spacing.
            quantum_register_controls_grid_spacer_widget = QtWidgets.QSpacerItem(
                2, 2, QtWidgets.QSizePolicy.Policy.Expanding, QtWidgets.QSizePolicy.Policy.Minimum
            )
            self.quantum_register_controls_grid_layout.addItem(
                quantum_register_controls_grid_spacer_widget, quantum_register_controls_grid_row, 4
            )
            quantum_register_controls_grid_row += 1

        self.quantum_register_controls_grid_layout.setColumnStretch(0, 0)
        self.quantum_register_controls_grid_layout.setColumnStretch(1, 0)
        self.quantum_register_controls_grid_layout.setColumnStretch(2, 0)

        simulation_run_scroll_area = QtWidgets.QScrollArea()
        simulation_run_scroll_area.setWidget(self.simulation_run_wrapper_box)
        simulation_run_scroll_area.setWidgetResizable(True)
        main_layout.addWidget(simulation_run_scroll_area)

    def handle_quantum_register_name_search(self) -> None:
        for qreg in self.annotatable_quantum_computation.qregs.values():
            if qreg.size == 0 or does_qubit_label_start_with_internal_qubit_label_prefix(
                self.annotatable_quantum_computation.get_qubit_label(qreg.start, syrec.qubit_label_type.internal)
            ):
                continue

            qreg_name_label: QtWidgets.QLabel | None = self.simulation_run_wrapper_box.findChild(
                QtWidgets.QLabel, self.qreg_label_name_format.format(qreg_name=qreg.name)
            )
            qreg_input_state_input_field: QtWidgets.QLineEdit | None = self.simulation_run_wrapper_box.findChild(
                QtWidgets.QLineEdit, self.qreg_input_state_input_field_name_format.format(qreg_name=qreg.name)
            )
            qreg_output_state_input_field: QtWidgets.QLineEdit | None = self.simulation_run_wrapper_box.findChild(
                QtWidgets.QLineEdit, self.qreg_output_state_input_field_name_format.format(qreg_name=qreg.name)
            )
            qreg_edit_qubit_values_toggle_button: QtWidgets.QPushButton | None = (
                self.simulation_run_wrapper_box.findChild(
                    QtWidgets.QPushButton, self.qreg_qubit_values_toggle_button_name_format.format(qreg_name=qreg.name)
                )
            )

            if (
                qreg_name_label is None
                or qreg_input_state_input_field is None
                or qreg_output_state_input_field is None
                or qreg_edit_qubit_values_toggle_button is None
            ):
                # TODO: This should not happen
                continue

            should_control_be_visible: bool = (
                self.quantum_register_search_input_field.text() is None
                or qreg.name.startswith(self.quantum_register_search_input_field.text())
            )
            qreg_name_label.setVisible(should_control_be_visible)
            qreg_input_state_input_field.setVisible(should_control_be_visible)
            qreg_output_state_input_field.setVisible(should_control_be_visible)
            qreg_edit_qubit_values_toggle_button.setVisible(should_control_be_visible)

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

    def handle_qreg_qubit_values_edit_toggle_button_click(self, associated_qreg_name: str) -> None:
        for qreg in self.annotatable_quantum_computation.qregs.values():
            if qreg.size == 0 or does_qubit_label_start_with_internal_qubit_label_prefix(
                self.annotatable_quantum_computation.get_qubit_label(qreg.start, syrec.qubit_label_type.internal)
            ):
                continue

            # TODO: QtCore.Qt.FindDirectChildrenOnly
            qreg_input_state_input_field: QtWidgets.QtWidget | None = self.simulation_run_wrapper_box.findChild(
                QtWidgets.QLineEdit, self.qreg_input_state_input_field_name_format.format(qreg_name=qreg.name)
            )
            qreg_output_state_input_field: QtWidgets.QtWidget | None = self.simulation_run_wrapper_box.findChild(
                QtWidgets.QLineEdit, self.qreg_output_state_input_field_name_format.format(qreg_name=qreg.name)
            )
            qubit_values_groupbox: QtWidgets.QtWidget | None = self.simulation_run_wrapper_box.findChild(
                QtWidgets.QGroupBox, self.qreg_qubit_values_groupbox_format.format(qreg_name=qreg.name)
            )
            qubit_values_toggle_button: QtWidgets.QtWidget | None = self.simulation_run_wrapper_box.findChild(
                QtWidgets.QPushButton, self.qreg_qubit_values_toggle_button_name_format.format(qreg_name=qreg.name)
            )

            if (
                qreg_input_state_input_field is None
                or qreg_output_state_input_field is None
                or qubit_values_groupbox is None
                or qubit_values_toggle_button is None
            ):
                # TODO: This should not happen
                continue

            if qreg.name == associated_qreg_name and not qubit_values_groupbox.isVisible():
                qubit_values_groupbox.setVisible(True)
                qubit_values_toggle_button.setText("Toggle qubit values edit")
                qreg_input_state_input_field.setEnabled(False)
                qreg_output_state_input_field.setEnabled(False)
                self.quantum_register_search_input_field.setEnabled(False)
                self.quantum_register_search_trigger_button.setEnabled(False)
            else:
                qubit_values_groupbox.setVisible(False)
                qubit_values_toggle_button.setText("Edit qubit values")
                qreg_input_state_input_field.setEnabled(True)
                qreg_output_state_input_field.setEnabled(not qreg_output_state_input_field.text().empty())
                self.quantum_register_search_input_field.setEnabled(True)
                self.quantum_register_search_trigger_button.setEnabled(True)

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
        self, annotatable_quantum_computation: syrec.annotatable_quantum_computation, parent: QtWidgets.QWidget
    ) -> None:
        super().__init__()
        self.parent = parent
        self.annotatable_quantum_computation = annotatable_quantum_computation

        self.title = "Define simulation runs for quantum computation"
        self.setWindowTitle(self.title)

        self.left = 0
        self.top = 0
        self.width = 1200
        self.height = 800
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

        in_state = syrec.n_bit_values_container(self.annotatable_quantum_computation.num_qubits)
        out_state = syrec.n_bit_values_container(self.annotatable_quantum_computation.num_qubits)
        simulation_run_one = InputOutputStateMappingDefinitionWidget(
            0, self.annotatable_quantum_computation, in_state, None
        )
        simulation_run_two = InputOutputStateMappingDefinitionWidget(
            1, self.annotatable_quantum_computation, in_state, None
        )
        simulation_run_three = InputOutputStateMappingDefinitionWidget(
            2, self.annotatable_quantum_computation, in_state, out_state
        )
        simulation_runs_list_layout.addWidget(simulation_run_one)
        simulation_runs_list_layout.addWidget(simulation_run_two)
        simulation_runs_list_layout.addWidget(simulation_run_three)

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
