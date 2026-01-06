# Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
# Copyright (c) 2025 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

from __future__ import annotations

from typing import TYPE_CHECKING

from PyQt6 import QtCore, QtGui, QtWidgets

from mqt import syrec

if TYPE_CHECKING:
    from .qt_simulation_run_model import QuantumRegisterLayout, SimulationRunModel

from .qt_simulation_run_model import (
    ANNOTATABLE_QUANTUM_COMPUTATION_QT_ROLE,
    QUANTUM_REGISTER_LAYOUT_QT_ROLE,
    SIMULATION_RUN_IO_STATE_QT_ROLE,
)


def stringify_some_qubits_of_n_bit_values_container(
    n_bit_values_container: syrec.n_bit_values_container, first_qubit: int, n_qubits: int
) -> str:
    if first_qubit >= n_bit_values_container.size() or first_qubit + n_qubits >= n_bit_values_container.size():
        return ""

    return "".join(["1" if n_bit_values_container.test(i) else "0" for i in range(first_qubit, first_qubit + n_qubits)])


class SimulationRunEditorDialog(QtWidgets.QDialog):  # type: ignore[misc]
    def __init__(self, simulation_run_model_index: QtCore.QModelIndex, parent: QtWidgets.QWidget):
        super().__init__(parent)
        self.simulation_run_model_index: QtCore.QModelIndex = simulation_run_model_index
        self.edited_simulation_run_model: SimulationRunModel = simulation_run_model_index.data(
            SIMULATION_RUN_IO_STATE_QT_ROLE
        )

        self.qreg_layouts: list[QuantumRegisterLayout] = simulation_run_model_index.data(
            QUANTUM_REGISTER_LAYOUT_QT_ROLE
        )
        self.annotatable_quantum_computation: syrec.annotatable_quantum_computation = simulation_run_model_index.data(
            ANNOTATABLE_QUANTUM_COMPUTATION_QT_ROLE
        )

        initial_input_state: syrec.n_bit_values_container = self.edited_simulation_run_model.input_state
        initial_expected_output_state: syrec.n_bit_values_container | None = (
            self.edited_simulation_run_model.expected_output_state
        )

        self.setModal(True)
        self.setSizeGripEnabled(True)
        self.setWindowTitle("Edit qubit values of quantum registers for simulation run")
        main_layout = QtWidgets.QVBoxLayout()
        self.setLayout(main_layout)

        self.simulation_run_wrapper_box = QtWidgets.QGroupBox(
            "Simulation run #" + str(simulation_run_model_index.row())
        )
        main_layout.addWidget(self.simulation_run_wrapper_box)

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

        # TODO: How can we determine whether qubits are readonly
        self.are_qubits_values_readonly: bool = initial_input_state.size() == 0
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

        qreg_name_search_completer = QtWidgets.QCompleter([qreg_layout.qreg_name for qreg_layout in self.qreg_layouts])
        qreg_name_search_completer.setCaseSensitivity(QtCore.Qt.CaseSensitivity.CaseSensitive)
        self.quantum_register_search_input_field.setCompleter(qreg_name_search_completer)

        self.quantum_register_search_trigger_button = QtWidgets.QPushButton("Search")
        self.quantum_register_search_trigger_button.clicked.connect(self.handle_quantum_register_name_search)

        quantum_register_search_controls_layout.addWidget(quantum_register_search_label)
        quantum_register_search_controls_layout.addWidget(self.quantum_register_search_input_field)
        quantum_register_search_controls_layout.addWidget(self.quantum_register_search_trigger_button)
        quantum_register_controls_grid_layout.addLayout(
            quantum_register_search_controls_layout, 0, 0, alignment=QtCore.Qt.AlignmentFlag.AlignCenter
        )

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
        for qreg_layout in self.qreg_layouts:
            first_qubit_of_qreg: int = qreg_layout.first_qubit_of_qreg
            n_qubits_of_qreg: int = qreg_layout.qreg_size
            qreg_name: str = qreg_layout.qreg_name

            quantum_register_label = QtWidgets.QLabel(
                "Quantum register: " + qreg_name, objectName=self.qreg_label_name_format.format(qreg_name=qreg_name)
            )

            input_state_edit_field = QtWidgets.QLineEdit(
                objectName=self.qreg_input_state_input_field_name_format.format(qreg_name=qreg_name)
            )
            input_state_edit_field.setText(
                stringify_some_qubits_of_n_bit_values_container(
                    initial_input_state, first_qubit_of_qreg, n_qubits_of_qreg
                )
            )
            # input_state_edit_field.setEnabled(not self.are_qubits_values_readonly and not self.is_input_state_readonly)
            input_state_edit_field.setValidator(n_bit_values_container_contents_validator)
            input_state_edit_field.setMaxLength(n_qubits_of_qreg)

            output_state_edit_field = QtWidgets.QLineEdit(
                objectName=self.qreg_output_state_input_field_name_format.format(qreg_name=qreg_name)
            )
            if initial_expected_output_state is not None:
                output_state_edit_field.setText(
                    stringify_some_qubits_of_n_bit_values_container(
                        initial_expected_output_state, first_qubit_of_qreg, n_qubits_of_qreg
                    )
                )
                # output_state_edit_field.setEnabled(not self.are_qubits_values_readonly)
            else:
                output_state_edit_field.setEnabled(False)
                output_state_edit_field.setPlaceholderText("-")

            output_state_edit_field.setValidator(n_bit_values_container_contents_validator)
            output_state_edit_field.setMaxLength(n_qubits_of_qreg)

            edit_qubit_values_toggle_button = QtWidgets.QPushButton(
                "Edit qubit values",
                objectName=self.qreg_qubit_values_toggle_button_name_format.format(qreg_name=qreg_name),
            )
            # We need to ignore the checked parameter that is passed to the clicked slot of the QPushButton
            edit_qubit_values_toggle_button.clicked.connect(
                lambda _, associated_qreg_name=qreg_name: self.handle_qreg_qubit_values_edit_toggle_button_click(
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
                "Qubit values", objectName=self.qreg_qubit_values_groupbox_format.format(qreg_name=qreg_name)
            )
            input_output_qubits_value_controls_groupbox_layout = QtWidgets.QGridLayout()
            input_output_qubits_value_controls_groupbox.setLayout(input_output_qubits_value_controls_groupbox_layout)

            qubit_search_layout = QtWidgets.QHBoxLayout()

            qubit_search_label = QtWidgets.QLabel("Qubit")
            qubit_search_layout.addWidget(qubit_search_label)

            qubit_search_input_field = QtWidgets.QLineEdit(
                objectName=self.qreg_qubit_search_input_field_name_format.format(qreg_name=qreg_name)
            )
            qubit_search_input_field.setPlaceholderText("<QUBIT_LABEL>")

            qubit_search_completer = QtWidgets.QCompleter(
                SimulationRunEditorDialog.get_internal_qubit_labels_for_qreg(
                    self.annotatable_quantum_computation, first_qubit_of_qreg, first_qubit_of_qreg + n_qubits_of_qreg
                )
            )
            qubit_search_completer.setCaseSensitivity(QtCore.Qt.CaseSensitivity.CaseSensitive)
            qubit_search_input_field.setCompleter(qubit_search_completer)

            qubit_search_layout.addWidget(qubit_search_input_field)

            qubit_search_trigger_button = QtWidgets.QPushButton("Search")
            qubit_search_trigger_button.clicked.connect(
                lambda _, associated_qreg_name=qreg_name: self.handle_qubit_search_trigger_button_click(
                    associated_qreg_name
                )
            )
            qubit_search_layout.addWidget(qubit_search_trigger_button)

            input_output_qubits_value_controls_groupbox_layout.addLayout(
                qubit_search_layout, 0, 0, 1, 1, QtCore.Qt.AlignmentFlag.AlignCenter
            )

            for qubit in range(first_qubit_of_qreg, first_qubit_of_qreg + n_qubits_of_qreg):
                one_based_relative_qubit_idx_in_qreg: int = (qubit - first_qubit_of_qreg) + 1
                fetched_internal_qubit_label: str | None = self.annotatable_quantum_computation.get_qubit_label(
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
                        stringified_qubit_value=SimulationRunEditorDialog.stringify_qubit_value(
                            initial_input_state.test(qubit), return_as_high_low_state=True
                        )
                    )
                )

                input_state_qubit_value_checkbox.checkStateChanged.connect(
                    lambda _,
                    associated_qreg_name=qreg_name,
                    associated_qubit=qubit,
                    relative_qubit_index_in_quantum_register=one_based_relative_qubit_idx_in_qreg
                    - 1: self.handle_input_state_qubit_value_checkbox_state_change(
                        associated_qreg_name, associated_qubit, relative_qubit_index_in_quantum_register
                    )
                )

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
                        stringified_qubit_value=SimulationRunEditorDialog.stringify_qubit_value(
                            None
                            if initial_expected_output_state is None
                            else initial_expected_output_state.test(qubit),
                            return_as_high_low_state=True,
                        )
                    )
                )
                output_state_qubit_value_checkbox.checkStateChanged.connect(
                    lambda _,
                    associated_qreg_name=qreg_name,
                    associated_qubit=qubit,
                    relative_qubit_index_in_quantum_register=one_based_relative_qubit_idx_in_qreg
                    - 1: self.handle_output_state_qubit_value_checkbox_state_change(
                        associated_qreg_name, associated_qubit, relative_qubit_index_in_quantum_register
                    )
                )
                output_state_qubit_value_checkbox.setEnabled(initial_expected_output_state is not None)
                input_output_qubits_value_controls_groupbox_layout.addWidget(
                    output_state_qubit_value_checkbox,
                    one_based_relative_qubit_idx_in_qreg,
                    2,
                    alignment=QtCore.Qt.AlignmentFlag.AlignCenter,
                )

            # TODO: How can the column widths of the input fields and the checkbox columns be synced?
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

        # Add spacer item to take up remaining space between last quantum register elements and bottom of parent group box without stretching the spacing between the already added controls in the group box
        quantum_register_controls_grid_layout.addItem(
            QtWidgets.QSpacerItem(2, 2, QtWidgets.QSizePolicy.Policy.Minimum, QtWidgets.QSizePolicy.Policy.Expanding),
            quantum_register_controls_grid_row,
            0,
            1,
            5,
        )
        quantum_register_controls_grid_layout.setColumnStretch(0, 0)
        quantum_register_controls_grid_layout.setColumnStretch(1, 1)
        quantum_register_controls_grid_layout.setColumnStretch(2, 1)
        quantum_register_controls_grid_layout.setColumnStretch(3, 0)
        quantum_register_controls_grid_layout.setColumnStretch(4, 2)

        simulation_run_scroll_area = QtWidgets.QScrollArea()
        simulation_run_scroll_area.setWidget(self.simulation_run_wrapper_box)
        simulation_run_scroll_area.setWidgetResizable(True)
        main_layout.addWidget(simulation_run_scroll_area)

        # Add dialog control buttons and link signals to slots of dialog
        dialog_button_box = QtWidgets.QDialogButtonBox(
            QtWidgets.QDialogButtonBox.StandardButton.Save | QtWidgets.QDialogButtonBox.StandardButton.Cancel
        )
        dialog_button_box.setCenterButtons(True)
        dialog_button_box.accepted.connect(self.accept)
        dialog_button_box.rejected.connect(self.reject)

        main_layout.addWidget(dialog_button_box)

    def handle_quantum_register_name_search(self) -> None:
        for qreg_layout in self.qreg_layouts:
            qreg_name: str = qreg_layout.qreg_name
            qreg_name_label: QtWidgets.QLabel | None = self.simulation_run_wrapper_box.findChild(
                QtWidgets.QLabel, self.qreg_label_name_format.format(qreg_name=qreg_name)
            )
            qreg_input_state_input_field: QtWidgets.QLineEdit | None = self.simulation_run_wrapper_box.findChild(
                QtWidgets.QLineEdit, self.qreg_input_state_input_field_name_format.format(qreg_name=qreg_name)
            )
            qreg_output_state_input_field: QtWidgets.QLineEdit | None = self.simulation_run_wrapper_box.findChild(
                QtWidgets.QLineEdit, self.qreg_output_state_input_field_name_format.format(qreg_name=qreg_name)
            )
            qreg_edit_qubit_values_toggle_button: QtWidgets.QPushButton | None = (
                self.simulation_run_wrapper_box.findChild(
                    QtWidgets.QPushButton, self.qreg_qubit_values_toggle_button_name_format.format(qreg_name=qreg_name)
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
                or qreg_name.startswith(self.quantum_register_search_input_field.text())
            )
            qreg_name_label.setVisible(should_control_be_visible)
            qreg_input_state_input_field.setVisible(should_control_be_visible)
            qreg_output_state_input_field.setVisible(should_control_be_visible)
            qreg_edit_qubit_values_toggle_button.setVisible(should_control_be_visible)

    def handle_input_state_qubit_value_checkbox_state_change(
        self, associated_qreg_name: str, associated_qubit: int, relative_qubit_index_in_quantum_register: int
    ) -> None:
        associated_qubit_value_checkbox: QtWidgets.QCheckBox | None = self.simulation_run_wrapper_box.findChild(
            QtWidgets.QCheckBox,
            self.input_state_qubit_checkbox_name_format.format(qubit=associated_qubit),
        )

        qreg_input_state_input_field: QtWidgets.QLineEdit | None = self.simulation_run_wrapper_box.findChild(
            QtWidgets.QLineEdit, self.qreg_input_state_input_field_name_format.format(qreg_name=associated_qreg_name)
        )

        if associated_qubit_value_checkbox is None or qreg_input_state_input_field is None:
            self.show_error_msg_dialog(
                title="Failed to updated qubit value",
                error_msg=f"Failed to locate all required Qt widgets required to update value of qubit {relative_qubit_index_in_quantum_register} of quantum register {associated_qreg_name} in input state!",
            )
            return

        updated_qubit_value: bool = associated_qubit_value_checkbox.checkState() == QtCore.Qt.CheckState.Checked
        stringified_updated_qubit_value: str = SimulationRunEditorDialog.stringify_qubit_value(
            updated_qubit_value, return_as_high_low_state=True
        )

        if not self.edited_simulation_run_model.update_input_state_qubit_value(associated_qubit, updated_qubit_value):
            self.show_error_msg_dialog(
                title="Failed to updated qubit value",
                error_msg=f"Failed to update value of qubit {relative_qubit_index_in_quantum_register} of quantum register {associated_qreg_name} in input state to new value{stringified_updated_qubit_value}!",
            )
            return

        associated_qubit_value_checkbox.setText(
            self.stringified_qubit_value_format.format(stringified_qubit_value=stringified_updated_qubit_value)
        )

        curr_stringified_input_state: str = qreg_input_state_input_field.text()
        qreg_input_state_input_field.setText(
            curr_stringified_input_state[:relative_qubit_index_in_quantum_register]
            + SimulationRunEditorDialog.stringify_qubit_value(updated_qubit_value, return_as_high_low_state=False)
            + curr_stringified_input_state[relative_qubit_index_in_quantum_register + 1 :]
        )

    def handle_output_state_qubit_value_checkbox_state_change(
        self, associated_qreg_name: str, associated_qubit: int, relative_qubit_index_in_quantum_register: int
    ) -> None:
        associated_qubit_value_checkbox: QtWidgets.QCheckBox | None = self.simulation_run_wrapper_box.findChild(
            QtWidgets.QCheckBox,
            self.output_state_qubit_checkbox_name_format.format(qubit=associated_qubit),
        )

        qreg_output_state_input_field: QtWidgets.QLineEdit | None = self.simulation_run_wrapper_box.findChild(
            QtWidgets.QLineEdit, self.qreg_output_state_input_field_name_format.format(qreg_name=associated_qreg_name)
        )

        if associated_qubit_value_checkbox is None or qreg_output_state_input_field is None:
            self.show_error_msg_dialog(
                title="Failed to updated qubit value",
                error_msg=f"Failed to locate all required Qt widgets required to update value of qubit {relative_qubit_index_in_quantum_register} of quantum register {associated_qreg_name} in output state!",
            )
            return

        updated_qubit_value: bool = associated_qubit_value_checkbox.checkState() == QtCore.Qt.CheckState.Checked
        stringified_updated_qubit_value: str = SimulationRunEditorDialog.stringify_qubit_value(
            updated_qubit_value, return_as_high_low_state=True
        )

        if not self.edited_simulation_run_model.update_expected_output_state_qubit_value(
            associated_qubit, updated_qubit_value
        ):
            self.show_error_msg_dialog(
                title="Failed to updated qubit value",
                error_msg=f"Failed to update value of qubit {relative_qubit_index_in_quantum_register} of quantum register {associated_qreg_name} in output state to new value{stringified_updated_qubit_value}!",
            )
            return

        associated_qubit_value_checkbox.setText(
            self.stringified_qubit_value_format.format(stringified_qubit_value=stringified_updated_qubit_value)
        )

        curr_stringified_output_state: str = qreg_output_state_input_field.text()
        qreg_output_state_input_field.setText(
            curr_stringified_output_state[:relative_qubit_index_in_quantum_register]
            + SimulationRunEditorDialog.stringify_qubit_value(updated_qubit_value, return_as_high_low_state=False)
            + curr_stringified_output_state[relative_qubit_index_in_quantum_register + 1 :]
        )

    def get_internal_qubit_labels_for_qreg(
        self: syrec.annotatable_quantum_computation, first_qubit_of_qreg: int, n_qubits_in_qreg: int
    ) -> list[str]:
        internal_qubit_labels: list[str] = []
        for qubit in range(first_qubit_of_qreg, first_qubit_of_qreg + n_qubits_in_qreg):
            fetched_internal_qubit_label: str | None = self.get_qubit_label(qubit, syrec.qubit_label_type.internal)
            if fetched_internal_qubit_label is None:
                continue
            internal_qubit_labels.append(fetched_internal_qubit_label)
        return internal_qubit_labels

    def show_error_msg_dialog(self, title: str, error_msg: str) -> None:
        QtWidgets.QMessageBox.critical(self, title, error_msg, defaultButton=QtWidgets.QMessageBox.StandardButton.Ok)

    @staticmethod
    def stringify_qubit_value(qubit_value: bool | None, return_as_high_low_state: bool) -> str:
        if qubit_value is None:
            return "UNKNOWN" if return_as_high_low_state else "-"

        if qubit_value is True:
            return "HIGH" if return_as_high_low_state else "1"

        return "LOW" if return_as_high_low_state else "0"

    def handle_qubit_search_trigger_button_click(self, associated_quantum_register_name: str) -> None:
        for qreg_layout in self.qreg_layouts:
            if qreg_layout.qreg_name != associated_quantum_register_name:
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

            for qubit in range(
                qreg_layout.first_qubit_of_qreg, qreg_layout.first_qubit_of_qreg + qreg_layout.qreg_size
            ):
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
        for qreg_layout in self.qreg_layouts:
            qreg_name: str = qreg_layout.qreg_name
            # TODO: QtCore.Qt.FindDirectChildrenOnly
            qreg_input_state_input_field: QtWidgets.QtWidget | None = self.simulation_run_wrapper_box.findChild(
                QtWidgets.QLineEdit, self.qreg_input_state_input_field_name_format.format(qreg_name=qreg_name)
            )
            qreg_output_state_input_field: QtWidgets.QtWidget | None = self.simulation_run_wrapper_box.findChild(
                QtWidgets.QLineEdit, self.qreg_output_state_input_field_name_format.format(qreg_name=qreg_name)
            )
            qubit_values_groupbox: QtWidgets.QtWidget | None = self.simulation_run_wrapper_box.findChild(
                QtWidgets.QGroupBox, self.qreg_qubit_values_groupbox_format.format(qreg_name=qreg_name)
            )
            qubit_values_toggle_button: QtWidgets.QtWidget | None = self.simulation_run_wrapper_box.findChild(
                QtWidgets.QPushButton, self.qreg_qubit_values_toggle_button_name_format.format(qreg_name=qreg_name)
            )

            qubit_values_groupbox_qubit_search_field: QtWidgets.QtWidget | None = (
                qubit_values_groupbox.findChild(
                    QtWidgets.QLineEdit, self.qreg_qubit_search_input_field_name_format.format(qreg_name=qreg_name)
                )
                if qubit_values_groupbox is not None
                else None
            )

            if (
                qreg_input_state_input_field is None
                or qreg_output_state_input_field is None
                or qubit_values_groupbox is None
                or qubit_values_toggle_button is None
                or qubit_values_toggle_button is None
                or qubit_values_groupbox_qubit_search_field is None
            ):
                # TODO: This should not happen
                continue

            if qreg_name == associated_qreg_name and not qubit_values_groupbox.isVisible():
                is_qubit_values_edit_enabled_for_any_qreg = True
                qubit_values_groupbox.setVisible(True)
                qubit_values_toggle_button.setText("Toggle qubit values edit")
                qreg_input_state_input_field.setEnabled(False)
                qreg_output_state_input_field.setEnabled(False)
            else:
                qubit_values_groupbox.setVisible(False)
                qubit_values_toggle_button.setText("Edit qubit values")
                qreg_input_state_input_field.setEnabled(True)
                qreg_output_state_input_field.setEnabled(qreg_output_state_input_field.text() != "")  # noqa: PLC1901
                qubit_values_groupbox_qubit_search_field.setText("")
                self.handle_qubit_search_trigger_button_click(associated_qreg_name)

        self.quantum_register_search_input_field.setEnabled(not is_qubit_values_edit_enabled_for_any_qreg)
        self.quantum_register_search_trigger_button.setEnabled(not is_qubit_values_edit_enabled_for_any_qreg)
