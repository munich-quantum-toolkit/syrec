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

# from dataclasses import dataclass
# from .qt_simulation_run_model import InputOutputStateMapping, QSimulationRunModel
from .simulation_view.qt_simulation_run_model import (
    InputOutputStateMapping,
    QtSimulationRunModel,
    SimulationRunModelStyledItemDelegate,
)

# @dataclass
# class InputOutputStateMapping:
#     input_state: syrec.n_bit_values_container
#     output_state: syrec.n_bit_values_container | None

#     def initialize_output_state_as_copy_of_input_state(self) -> bool:
#         if self.output_state is not None:
#             return False

#         self.output_state = syrec.n_bit_values_container(self.input_state.size())
#         for i in range(self.output_state.size()):
#             self.output_state.set(self.input_state.test(i))
#         return True

#     def update_input_state_qubit_value(self, qubit: int, qubit_value: bool) -> bool:
#         if qubit < 0 or qubit >= self.input_state.size():
#             return False

#         self.input_state.set(qubit, qubit_value)
#         return True

#     def update_output_state_qubit_value(self, qubit: int, qubit_value: bool) -> bool:
#         if self.output_state is None or qubit < 0 or qubit >= self.output_state.size():
#             return False

#         self.output_state.set(qubit, qubit_value)
#         return True


def does_qubit_label_start_with_internal_qubit_label_prefix(qubit_label: str) -> bool:
    return qubit_label.startswith("__q")


def stringify_some_qubits_of_n_bit_values_container(
    n_bit_values_container: syrec.n_bit_values_container, first_qubit: int, n_qubits: int
) -> str:
    if first_qubit >= n_bit_values_container.size() or first_qubit + n_qubits >= n_bit_values_container.size():
        return ""

    return "".join(["1" if n_bit_values_container.test(i) else "0" for i in range(first_qubit, first_qubit + n_qubits)])


class InputOutputStateMappingDefinitionWidget(QtWidgets.QWidget):  # type: ignore[misc]
    input_state_qubit_value_change = QtCore.pyqtSignal(
        int,
        int,
        bool,
        arguments=["simulation_run_number", "qubit", "new_qubit_value"],
        name="inputStateQubitValueChanged",
    )
    output_state_qubit_value_change = QtCore.pyqtSignal(
        int,
        int,
        bool,
        arguments=["simulation_run_number", "qubit", "new_qubit_value"],
        name="outputStateQubitValueChanged",
    )
    requested_simulation_run_deletion = QtCore.pyqtSignal(
        int, arguments=["simulation_run_number"], name="simulationRunDeleted"
    )
    request_output_state_initialization = QtCore.pyqtSignal(name="requestedOutputStateInitialization")
    request_output_state_reset = QtCore.pyqtSignal(name="requestedOutputStateReset")

    def __init__(
        self,
        simulation_run_number: int,
        annotatable_quantum_computation: syrec.annotatable_quantum_computation,
        input_output_state_mapping: InputOutputStateMapping,
        is_input_state_readonly: bool = False,
    ) -> None:
        # parent: QtWidgets.QWidget) -> None:
        super().__init__()

        self.simulation_run_number = simulation_run_number
        self.annotatable_quantum_computation = annotatable_quantum_computation
        self.is_input_state_readonly = is_input_state_readonly

        # TODO: Validation that input and output state have same size (validate all input parameters)
        # TODO: Define validator for input and output state inputs
        # TODO: Update input/output state value when qubit value is changed
        # TODO: How to render n-dimensional variables

        self.qubit_label_name_format = "q_{qubit:d}_lbl"
        self.input_state_qubit_checkbox_name_format = "q_{qubit:d}_in_checkB"
        self.output_state_qubit_checkbox_name_format = "q_{qubit:d}_out_checkB"
        self.stringified_qubit_value_format = "(Value: {stringified_qubit_value:s})"
        self.qreg_qubit_values_groupbox_format = "qreg_{qreg_name:s}_qubit_values_groupbox"
        self.qreg_label_name_format = "qreg_{qreg_name:s}_lbl"
        self.qreg_input_state_input_field_name_format = "qreg_{qreg_name:s}_inputState"
        self.qreg_output_state_input_field_name_format = "qreg_{qreg_name:s}_outputState"
        self.qreg_qubit_values_toggle_button_name_format = "qreg_{qreg_name:s}_qubit_values_toggle"
        self.qreg_qubit_search_input_field_name_format = "qreg_{qreg_name:s}_qubit_search_input"

        main_layout = QtWidgets.QVBoxLayout()
        self.setLayout(main_layout)
        self.simulation_run_wrapper_box = QtWidgets.QGroupBox("Simulation run #" + str(self.simulation_run_number))

        # TODO: How can we determine whether qubits are readonly
        self.are_qubits_values_readonly: bool = input_output_state_mapping.input_state.size() == 0
        self.edit_of_qubit_values_enabled: bool = False

        # TODO: Add validators
        quantum_register_controls_grid_layout = QtWidgets.QGridLayout()
        self.simulation_run_wrapper_box.setLayout(quantum_register_controls_grid_layout)

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
        quantum_register_controls_grid_layout.addLayout(
            quantum_register_search_controls_layout, 0, 0, alignment=QtCore.Qt.AlignmentFlag.AlignCenter
        )

        if not self.is_input_state_readonly:
            simulation_run_delete_button = QtWidgets.QPushButton("Delete simulation run")
            simulation_run_delete_button.clicked.connect(self.handle_simulation_run_deletion_button_click)
            quantum_register_controls_grid_layout.addWidget(simulation_run_delete_button, 0, 5)

        # Grid position component order is row followed by column
        input_column_label = QtWidgets.QLabel("Input")
        output_column_label = QtWidgets.QLabel("Output")

        quantum_register_controls_grid_layout.addWidget(
            input_column_label, 1, 1, alignment=QtCore.Qt.AlignmentFlag.AlignCenter
        )
        quantum_register_controls_grid_layout.addWidget(
            output_column_label, 1, 2, alignment=QtCore.Qt.AlignmentFlag.AlignCenter
        )

        n_bit_values_container_contents_validator_regular_expr = QtCore.QRegularExpression(R"^(\b)?$")
        n_bit_values_container_contents_validator = QtGui.QRegularExpressionValidator(
            n_bit_values_container_contents_validator_regular_expr, self
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
            input_state_edit_field.setText(
                stringify_some_qubits_of_n_bit_values_container(
                    input_output_state_mapping.input_state, first_qubit_of_qreg, n_qubits_of_qreg
                )
            )
            input_state_edit_field.setEnabled(not self.are_qubits_values_readonly and not self.is_input_state_readonly)
            input_state_edit_field.setValidator(n_bit_values_container_contents_validator)
            input_state_edit_field.setMaxLength(n_qubits_of_qreg)

            output_state_edit_field = QtWidgets.QLineEdit(
                objectName=self.qreg_output_state_input_field_name_format.format(qreg_name=qreg.name)
            )
            if input_output_state_mapping.output_state is not None:
                output_state_edit_field.setText(
                    stringify_some_qubits_of_n_bit_values_container(
                        input_output_state_mapping.output_state, first_qubit_of_qreg, n_qubits_of_qreg
                    )
                )
                output_state_edit_field.setEnabled(not self.are_qubits_values_readonly)
            else:
                output_state_edit_field.setEnabled(False)
                output_state_edit_field.setPlaceholderText("-")

            output_state_edit_field.setValidator(n_bit_values_container_contents_validator)
            output_state_edit_field.setMaxLength(n_qubits_of_qreg)

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

            quantum_register_controls_grid_layout.addWidget(
                quantum_register_label,
                quantum_register_controls_grid_row,
                0,
                alignment=QtCore.Qt.AlignmentFlag.AlignLeft,
            )
            quantum_register_controls_grid_layout.addWidget(
                input_state_edit_field,
                quantum_register_controls_grid_row,
                1,
                alignment=QtCore.Qt.AlignmentFlag.AlignCenter,
            )
            quantum_register_controls_grid_layout.addWidget(
                output_state_edit_field,
                quantum_register_controls_grid_row,
                2,
                alignment=QtCore.Qt.AlignmentFlag.AlignCenter,
            )
            quantum_register_controls_grid_layout.addWidget(
                edit_qubit_values_toggle_button, quantum_register_controls_grid_row, 3
            )
            n_cols_in_quantum_register_controls_grid_layout: int = 3

            # TODO: Scroll area
            input_output_qubits_value_controls_groupbox = QtWidgets.QGroupBox(
                "Qubit values", objectName=self.qreg_qubit_values_groupbox_format.format(qreg_name=qreg.name)
            )
            input_output_qubits_value_controls_groupbox_layout = QtWidgets.QGridLayout()
            input_output_qubits_value_controls_groupbox.setLayout(input_output_qubits_value_controls_groupbox_layout)

            qubit_search_layout = QtWidgets.QHBoxLayout()

            qubit_search_label = QtWidgets.QLabel("Qubit")
            qubit_search_layout.addWidget(qubit_search_label)

            qubit_search_input_field = QtWidgets.QLineEdit(
                objectName=self.qreg_qubit_search_input_field_name_format.format(qreg_name=qreg.name)
            )
            qubit_search_input_field.setPlaceholderText("<QUBIT_LABEL>")
            qubit_search_layout.addWidget(qubit_search_input_field)

            qubit_search_trigger_button = QtWidgets.QPushButton("Search")
            qubit_search_trigger_button.clicked.connect(
                lambda _, associated_qreg_name=qreg.name: self.handle_qubit_search_trigger_button_click(
                    associated_qreg_name
                )
            )
            qubit_search_layout.addWidget(qubit_search_trigger_button)

            input_output_qubits_value_controls_groupbox_layout.addLayout(
                qubit_search_layout, 0, 0, 1, 1, QtCore.Qt.AlignmentFlag.AlignCenter
            )

            for qubit in range(first_qubit_of_qreg, first_qubit_of_qreg + n_qubits_of_qreg):
                one_based_relative_qubit_idx_in_qreg: int = (qubit - first_qubit_of_qreg) + 1
                fetched_internal_qubit_label: str | None = annotatable_quantum_computation.get_qubit_label(
                    qubit, syrec.qubit_label_type.internal
                )
                qubit_label = QtWidgets.QLabel(
                    "Qubit: " + fetched_internal_qubit_label
                    if fetched_internal_qubit_label is not None
                    else "<UNKNOWN>",
                    objectName=self.qubit_label_name_format.format(qubit=qubit),
                )
                input_output_qubits_value_controls_groupbox_layout.addWidget(
                    qubit_label, one_based_relative_qubit_idx_in_qreg, 0
                )

                input_state_qubit_value_checkbox = QtWidgets.QCheckBox(
                    objectName=self.input_state_qubit_checkbox_name_format.format(qubit=qubit)
                )
                input_state_qubit_value_checkbox.setText(
                    self.stringified_qubit_value_format.format(
                        stringified_qubit_value=self.stringify_qubit_value(
                            input_output_state_mapping.input_state.test(qubit)
                        )
                    )
                )

                if not self.is_input_state_readonly:
                    input_state_qubit_value_checkbox.checkStateChanged.connect(
                        lambda state,
                        associated_qreg_name=qreg.name,
                        associated_qubit=qubit,
                        relative_qubit_index_in_quantum_register=one_based_relative_qubit_idx_in_qreg
                        - 1: self.handle_input_state_qubit_value_checkbox_state_change(
                            associated_qreg_name,
                            associated_qubit,
                            relative_qubit_index_in_quantum_register,
                            state == QtCore.Qt.CheckState.Checked,
                        )
                    )
                else:
                    input_state_qubit_value_checkbox.setEnabled(False)

                input_output_qubits_value_controls_groupbox_layout.addWidget(
                    input_state_qubit_value_checkbox,
                    one_based_relative_qubit_idx_in_qreg,
                    1,
                    alignment=QtCore.Qt.AlignmentFlag.AlignCenter,
                )

                output_state_qubit_value_checkbox = QtWidgets.QCheckBox(
                    objectName=self.output_state_qubit_checkbox_name_format.format(qubit=qubit)
                )
                output_state_qubit_value_checkbox.setText(
                    self.stringified_qubit_value_format.format(
                        stringified_qubit_value=self.stringify_qubit_value(
                            None
                            if input_output_state_mapping.output_state is None
                            else input_output_state_mapping.output_state.test(qubit)
                        )
                    )
                )
                output_state_qubit_value_checkbox.checkStateChanged.connect(
                    lambda state,
                    associated_qreg_name=qreg.name,
                    associated_qubit=qubit,
                    relative_qubit_index_in_quantum_register=one_based_relative_qubit_idx_in_qreg
                    - 1: self.handle_output_state_qubit_value_checkbox_state_change(
                        associated_qreg_name,
                        associated_qubit,
                        relative_qubit_index_in_quantum_register,
                        state == QtCore.Qt.CheckState.Checked,
                    )
                )
                output_state_qubit_value_checkbox.setEnabled(input_output_state_mapping.output_state is not None)
                input_output_qubits_value_controls_groupbox_layout.addWidget(
                    output_state_qubit_value_checkbox,
                    one_based_relative_qubit_idx_in_qreg,
                    2,
                    alignment=QtCore.Qt.AlignmentFlag.AlignCenter,
                )

            input_output_qubits_value_controls_groupbox_layout.setColumnStretch(0, 0)
            input_output_qubits_value_controls_groupbox_layout.setColumnStretch(1, 1)
            input_output_qubits_value_controls_groupbox_layout.setColumnStretch(2, 1)

            quantum_register_controls_grid_row += 1
            input_output_qubits_value_controls_groupbox.setVisible(False)
            quantum_register_controls_grid_layout.addWidget(
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
            quantum_register_controls_grid_layout.addItem(
                quantum_register_controls_grid_spacer_widget, quantum_register_controls_grid_row, 4
            )
            quantum_register_controls_grid_row += 1

        quantum_register_controls_grid_layout.setColumnStretch(0, 0)
        quantum_register_controls_grid_layout.setColumnStretch(1, 1)
        quantum_register_controls_grid_layout.setColumnStretch(2, 1)
        quantum_register_controls_grid_layout.setColumnStretch(3, 0)
        quantum_register_controls_grid_layout.setColumnStretch(4, 2)
        quantum_register_controls_grid_layout.setColumnStretch(5, 0)

        # simulation_run_scroll_area = QtWidgets.QScrollArea()
        # simulation_run_scroll_area.setWidget(self.simulation_run_wrapper_box)
        # simulation_run_scroll_area.setWidgetResizable(True)
        # main_layout.addWidget(simulation_run_scroll_area)
        main_layout.addWidget(self.simulation_run_wrapper_box)

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
        self,
        associated_qreg_name: str,
        associated_qubit: int,
        relative_qubit_index_in_quantum_register: int,
        qubit_value: bool,
    ) -> None:
        associated_qubit_value_checkbox: QtWidgets.QCheckBox | None = self.simulation_run_wrapper_box.findChild(
            QtWidgets.QCheckBox,
            self.input_state_qubit_checkbox_name_format.format(qubit=associated_qubit),
        )

        qreg_input_state_input_field: QtWidgets.QLineEdit | None = self.simulation_run_wrapper_box.findChild(
            QtWidgets.QLineEdit, self.qreg_input_state_input_field_name_format.format(qreg_name=associated_qreg_name)
        )

        if associated_qubit_value_checkbox is None or qreg_input_state_input_field is None:
            # TODO: This should not happen
            return

        associated_qubit_value_checkbox.setText(
            self.stringified_qubit_value_format.format(stringified_qubit_value=self.stringify_qubit_value(qubit_value))
        )

        curr_stringified_input_state: str = qreg_input_state_input_field.text()
        qreg_input_state_input_field.setText(
            curr_stringified_input_state[:relative_qubit_index_in_quantum_register]
            + ("1" if associated_qubit_value_checkbox.checkState() == QtCore.Qt.CheckState.Checked else "0")
            + curr_stringified_input_state[relative_qubit_index_in_quantum_register + 1 :]
        )

    # TODO: Update n_bit_values_container and parent textfield
    def handle_output_state_qubit_value_checkbox_state_change(
        self,
        associated_qreg_name: str,
        associated_qubit: int,
        relative_qubit_index_in_quantum_register: int,
        qubit_value: bool,
    ) -> None:
        associated_qubit_value_checkbox: QtWidgets.QCheckBox | None = self.simulation_run_wrapper_box.findChild(
            QtWidgets.QCheckBox,
            self.output_state_qubit_checkbox_name_format.format(qubit=associated_qubit),
        )

        qreg_output_state_input_field: QtWidgets.QLineEdit | None = self.simulation_run_wrapper_box.findChild(
            QtWidgets.QLineEdit, self.qreg_output_state_input_field_name_format.format(qreg_name=associated_qreg_name)
        )

        if associated_qubit_value_checkbox is None or qreg_output_state_input_field is None:
            # TODO: This should not happen
            return

        associated_qubit_value_checkbox.setText(
            self.stringified_qubit_value_format.format(stringified_qubit_value=self.stringify_qubit_value(qubit_value))
        )

        curr_stringified_output_state: str = qreg_output_state_input_field.text()
        qreg_output_state_input_field.setText(
            curr_stringified_output_state[:relative_qubit_index_in_quantum_register]
            + ("1" if associated_qubit_value_checkbox.checkState() == QtCore.Qt.CheckState.Checked else "0")
            + curr_stringified_output_state[relative_qubit_index_in_quantum_register + 1 :]
        )

    @staticmethod
    def stringify_qubit_value(qubit_value: bool | None) -> str:
        if qubit_value is None:
            return "UNKNOWN"
        return "HIGH" if qubit_value else "LOW"

    def handle_qubit_search_trigger_button_click(self, associated_quantum_register_name: str) -> None:
        for qreg in self.annotatable_quantum_computation.qregs.values():
            if (
                qreg.size == 0
                or does_qubit_label_start_with_internal_qubit_label_prefix(
                    self.annotatable_quantum_computation.get_qubit_label(qreg.start, syrec.qubit_label_type.internal)
                )
                or qreg.name != associated_quantum_register_name
            ):
                continue

            qreg_qubits_groupbox: QtWidgets.QtWidget | None = self.simulation_run_wrapper_box.findChild(
                QtWidgets.QGroupBox,
                self.qreg_qubit_values_groupbox_format.format(qreg_name=associated_quantum_register_name),
            )
            if qreg_qubits_groupbox is None:
                # TODO: This should not happen
                continue

            qubit_search_input_field: QtWidgets.QtWidget | None = qreg_qubits_groupbox.findChild(
                QtWidgets.QLineEdit,
                self.qreg_qubit_search_input_field_name_format.format(qreg_name=associated_quantum_register_name),
            )
            if qubit_search_input_field is None:
                # TODO: This should not happen
                continue

            for qubit in range(qreg.start, qreg.start + qreg.size):
                qubit_value_label: QtWidgets.QtWidget | None = qreg_qubits_groupbox.findChild(
                    QtWidgets.QLabel, self.qubit_label_name_format.format(qubit=qubit)
                )
                input_state_qubit_checkbox: QtWidgets.QCheckBox | None = qreg_qubits_groupbox.findChild(
                    QtWidgets.QCheckBox, self.input_state_qubit_checkbox_name_format.format(qubit=qubit)
                )
                output_state_qubit_checkbox: QtWidgets.QCheckBox | None = qreg_qubits_groupbox.findChild(
                    QtWidgets.QCheckBox, self.output_state_qubit_checkbox_name_format.format(qubit=qubit)
                )
                if (
                    qubit_value_label is None
                    or input_state_qubit_checkbox is None
                    or output_state_qubit_checkbox is None
                ):
                    # TODO: This should not happen
                    continue

                does_qubit_label_match_search_text: bool = self.annotatable_quantum_computation.get_qubit_label(
                    qubit, syrec.qubit_label_type.internal
                ).startswith(qubit_search_input_field.text())
                qubit_value_label.setVisible(does_qubit_label_match_search_text)
                input_state_qubit_checkbox.setVisible(does_qubit_label_match_search_text)
                output_state_qubit_checkbox.setVisible(does_qubit_label_match_search_text)

    def handle_qreg_qubit_values_edit_toggle_button_click(self, associated_qreg_name: str) -> None:
        is_qubit_values_edit_enabled_for_any_qreg: bool = False
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
                is_qubit_values_edit_enabled_for_any_qreg = True
                qubit_values_groupbox.setVisible(True)
                qubit_values_toggle_button.setText("Toggle qubit values edit")
                qreg_input_state_input_field.setEnabled(False)
                qreg_output_state_input_field.setEnabled(False)
            else:
                qubit_values_groupbox.setVisible(False)
                qubit_values_toggle_button.setText("Edit qubit values")
                qreg_input_state_input_field.setEnabled(not self.is_input_state_readonly)
                qreg_output_state_input_field.setEnabled(qreg_output_state_input_field.text() != "")  # noqa: PLC1901

        self.quantum_register_search_input_field.setEnabled(not is_qubit_values_edit_enabled_for_any_qreg)
        self.quantum_register_search_trigger_button.setEnabled(not is_qubit_values_edit_enabled_for_any_qreg)

    def handle_simulation_run_deletion_button_click(self) -> None:
        self.requested_simulation_run_deletion.emit(self.simulation_run_number)

    def handle_simulation_run_number_update(self, new_simulation_run_number: int) -> None:
        self.simulation_run_number = new_simulation_run_number
        self.simulation_run_wrapper_box.setText("Simulation run #" + str(self.simulation_run_number))


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

        self.simulation_runs_model: QtSimulationRunModel = QtSimulationRunModel(annotatable_quantum_computation, self)
        self.simulation_runs_list_view: QtWidgets.QListView = QtWidgets.QListView()
        self.simulation_runs_list_view.setModel(self.simulation_runs_model)
        self.simulation_runs_list_view.setItemDelegate(SimulationRunModelStyledItemDelegate())  # type: ignore[no-untyped-call]
        self.simulation_runs_list_view.setUniformItemSizes(True)
        self.simulation_runs_list_view.setFlow(QtWidgets.QListView.Flow.TopToBottom)
        self.simulation_runs_list_view.setSelectionMode(QtWidgets.QAbstractItemView.SelectionMode.SingleSelection)
        # self.simulation_runs_list_view.setSpacing(10)

        # TODO: Default background of tabwidget is white on windows (https://forum.qt.io/topic/82262/default-background-color-of-qtabwidget-and-qwidget-qgroupbox/4)
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
        self.layout.addWidget(self.simulation_runs_tab_widget)
        # self.layout.addStretch()
        self.setLayout(self.layout)

    def initialize_some_simulation_runs_tab(self) -> QtWidgets.QWidget:
        for i in range(10):
            in_state = syrec.n_bit_values_container(self.annotatable_quantum_computation.num_qubits)
            out_state = syrec.n_bit_values_container(self.annotatable_quantum_computation.num_qubits)

            in_out_state_mapping: InputOutputStateMapping | None = None
            if i < 2:
                in_out_state_mapping = InputOutputStateMapping(in_state, None)
            else:
                in_out_state_mapping = InputOutputStateMapping(in_state, out_state)

            self.simulation_runs_model.add_simulation_run(in_out_state_mapping)

        simulation_runs_list_scrollarea = QtWidgets.QScrollArea()
        simulation_runs_list_scrollarea.setWidget(self.simulation_runs_list_view)
        simulation_runs_list_scrollarea.setWidgetResizable(True)
        return simulation_runs_list_scrollarea

    # Since this function will render many items one should use a QListView with a custom styled delegate to improve rendering performance
    # (see: https://forum.qt.io/topic/98733/how-can-i-make-my-listview-that-uses-custom-widgets-more-efficient)
    # How would one then edit the simulation run that is not rendered as a widget?
    # (regarding performance issues when rendering a lot of items in a list, tree or table view: https://forum.qt.io/topic/159449/qtreeview-with-lots-of-items-is-really-slow-can-it-be-optimised-or-is-something-buggy/31)
    @staticmethod
    def initialize_all_simulation_runs_tab() -> QtWidgets.QTabWidget:
        return QtWidgets.QTabWidget()

    @staticmethod
    def initialize_simulation_runs_from_file_tab() -> QtWidgets.QTabWidget:
        return QtWidgets.QTabWidget()

    def handle_simulation_runs_tab_widget_tab_bar_clicked(self, clicked_on_tab_index: int) -> None:
        self.simulation_runs_tab_widget.setCurrentIndex(clicked_on_tab_index)

    def handle_simulation_run_input_state_qubit_value_change(
        self, simulation_run_number: int, qubit: int, new_qubit_value: bool
    ) -> None:
        pass

    def handle_simulation_run_output_state_qubit_value_change(
        self, simulation_run_number: int, qubit: int, new_qubit_value: bool
    ) -> None:
        pass

    def handle_simulation_run_deletion_request(self, simulation_run_number: int) -> None:
        if simulation_run_number < 0 or simulation_run_number >= len(self.defined_simulation_runs):
            # TODO: Log error?
            return

        current_tab_widget: QtWidgets.QWidget | None = self.simulation_runs_tab_widget.currentWidget()
        if current_tab_widget is None:
            # TODO: This should not happen
            return
        # self.defined_simulation_runs.pop(simulation_run_number)
