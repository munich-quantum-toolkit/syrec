# Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
# Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

from __future__ import annotations

from typing import TYPE_CHECKING, Final

from PyQt6 import QtCore, QtGui, QtWidgets

from mqt import syrec

from .dialogs.base_progress_dialog import BaseProgressDialog
from .qt_simulation_run_model import (
    ANNOTATABLE_QUANTUM_COMPUTATION_QT_ROLE,
    QUANTUM_REGISTER_LAYOUT_QT_ROLE,
)

if TYPE_CHECKING:
    from .qt_simulation_run_model import (
        QuantumRegisterLayout,
        SimulationRunModel,
    )


class LineEditWithDynamicWidth(QtWidgets.QLineEdit):  # type: ignore[misc]
    focus_out = QtCore.pyqtSignal(name="focusOut")

    def __init__(self, expected_max_num_characters: int, parent: QtWidgets.QWidget = None):
        super().__init__(parent)
        self.expected_max_num_characters = expected_max_num_characters
        self.setMaxLength(expected_max_num_characters)
        self.setSizePolicy(QtWidgets.QSizePolicy.Policy.Expanding, QtWidgets.QSizePolicy.Policy.Fixed)

    # Make the widget greedy: whenever the layout offers more
    # than the nominal width, grab it.
    def sizeHint(self) -> QtCore.QSize:  # noqa: N802
        sh = super().sizeHint()
        fm = QtGui.QFontMetrics(self.font())
        nominal = fm.boundingRect("W" * self.expected_max_num_characters).width()
        # use the offered width
        preferred = min(nominal, self.width())
        return QtCore.QSize(preferred, sh.height())

    def focusOutEvent(self, ev: QtGui.QFocusEvent) -> None:  # noqa: N802
        super().focusOutEvent(ev)
        self.focus_out.emit()


QUBIT_LABEL_NAME_FORMAT: Final[str] = "q_{qubit:d}_lbl"
INPUT_STATE_QUBIT_CHECKBOX_NAME_FORMAT: Final[str] = "q_{qubit:d}_in_checkB"
OUTPUT_STATE_QUBIT_CHECKBOX_NAME_FORMAT: Final[str] = "q_{qubit:d}_out_checkB"

QREG_QUBIT_VALUES_GROUPBOX_NAME_FORMAT: Final[str] = "qreg_{qreg_name:s}_qubit_values_groupbox"
QREG_LABEL_NAME_FORMAT: Final[str] = "qreg_{qreg_name:s}_lbl"
QREG_LAYOUT_INFO_NAME_FORMAT: Final[str] = "qreg_{qreg_name:s}_layout_info_lbl"
QREG_INPUT_STATE_INPUT_FIELD_NAME_FORMAT: Final[str] = "qreg_{qreg_name:s}_input_state"
QREG_OUTPUT_STATE_INPUT_FIELD_NAME_FORMAT: Final[str] = "qreg_{qreg_name:s}_output_state"
QREG_QUBIT_VALUES_TOGGLE_BUTTON_NAME_FORMAT: Final[str] = "qreg_{qreg_name:s}_qubit_values_toggle"
QREG_QUBIT_SEARCH_INPUT_FIELD_NAME_FORMAT: Final[str] = "qreg_{qreg_name:s}_qubit_search_input"

QREG_EXPECTED_OUTPUT_STATE_VALUE_INIT_TOGGLE_NAME: Final[str] = "output_state_value_toggle"
QREG_SEARCH_INPUT_FIELD_NAME: Final[str] = "qreg_name_search_input_field"
QREG_VALUES_VALIDATION_ERROR_LABEL_NAME: Final[str] = "qreg_values_validation_err_lbl"

STRINGIFIED_QUBIT_VALUE_FORMAT: Final[str] = "(Value: {stringified_qubit_value:s})"
QREG_VALUES_VALIDATION_ERROR_FORMAT: Final[str] = (
    "Qubit values of quantum register '{qreg_name:s}' can only be defined as a combination of '0' or '1' literals. Additionally, the value of all qubits of the quantum register (n={expected_num_qubit_values:d}) must be specified but only {actual_num_qubit_values:d} were defined in the {input_or_output_state_ident:s} state!"
)


# TODO: Replace 'simple' returns with QDialog.reject("<ERR_MSG>") to indicate fatal errors and stop editing simulation run but also reject changes made in parent widget that opened dialog
class SimulationRunEditorDialog(QtWidgets.QDialog):  # type: ignore[misc]
    def __init__(
        self,
        simulation_run_model_index: QtCore.QModelIndex,
        copy_of_reference_edit_sim_run_model: SimulationRunModel,
        parent: QtWidgets.QWidget,
    ):
        super().__init__(parent)
        self.simulation_run_model_index: QtCore.QModelIndex = simulation_run_model_index
        self.edited_simulation_run_model: SimulationRunModel = copy_of_reference_edit_sim_run_model

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

        dialog_size: Final[QtCore.QSize] = BaseProgressDialog.get_default_big_dialog_size()
        center_dialog_pos_for_size: Final[QtCore.QPoint] = BaseProgressDialog.get_center_screen_position_for_size(
            dialog_size
        )
        self.setGeometry(
            center_dialog_pos_for_size.x(), center_dialog_pos_for_size.y(), dialog_size.width(), dialog_size.height()
        )

        main_layout = QtWidgets.QVBoxLayout()

        self.simulation_run_wrapper_box = QtWidgets.QGroupBox(
            "Simulation run #" + str(simulation_run_model_index.row())
        )
        # main_layout.addWidget(self.simulation_run_wrapper_box)

        # TODO: How to render n-dimensional variables
        # TODO: How can we determine whether qubits are readonly
        self.are_qubits_values_readonly: bool = initial_input_state.size() == 0
        self.edit_of_qubit_values_enabled: bool = False

        # TODO: Add validators
        quantum_register_controls_grid_layout = QtWidgets.QGridLayout()
        self.simulation_run_wrapper_box.setLayout(quantum_register_controls_grid_layout)
        quantum_register_controls_grid_layout.addLayout(
            self.create_qreg_search_controls(), 0, 0, alignment=QtCore.Qt.AlignmentFlag.AlignCenter
        )

        init_expected_output_state_button = QtWidgets.QPushButton(
            "Init output state"
            if self.edited_simulation_run_model.expected_output_state is None
            else "Clear output state",
            objectName=QREG_EXPECTED_OUTPUT_STATE_VALUE_INIT_TOGGLE_NAME,
        )
        init_expected_output_state_button.clicked.connect(self.handle_init_expected_output_state_button_click)

        output_state_value_toggle_controls_layout = QtWidgets.QHBoxLayout()
        output_state_value_toggle_controls_layout.addWidget(QtWidgets.QLabel("Modify output state value:"))
        output_state_value_toggle_controls_layout.addWidget(init_expected_output_state_button)
        quantum_register_controls_grid_layout.addLayout(
            output_state_value_toggle_controls_layout, 0, 1, alignment=QtCore.Qt.AlignmentFlag.AlignCenter
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

        n_bit_values_container_contents_validator_regular_expr = QtCore.QRegularExpression(R"^[0-1]*$")
        QtGui.QRegularExpressionValidator(n_bit_values_container_contents_validator_regular_expr, self)

        qreg_controls_grid_row: int = 2
        for qreg_layout in self.qreg_layouts:
            qreg_name: str = qreg_layout.qreg_name

            quantum_register_label = QtWidgets.QLabel(
                "Quantum register: " + qreg_name, objectName=QREG_LABEL_NAME_FORMAT.format(qreg_name=qreg_name)
            )
            quantum_register_controls_grid_layout.addWidget(
                quantum_register_label,
                qreg_controls_grid_row,
                0,
                alignment=QtCore.Qt.AlignmentFlag.AlignLeft,
            )

            input_state_edit_field = self.create_in_or_out_state_edit_field(
                qreg_layout, optional_qreg_qubit_values=initial_input_state, is_created_for_input_state=True
            )
            quantum_register_controls_grid_layout.addWidget(
                input_state_edit_field,
                qreg_controls_grid_row,
                1,
                alignment=QtCore.Qt.AlignmentFlag.AlignCenter,
            )

            output_state_edit_field = self.create_in_or_out_state_edit_field(
                qreg_layout, optional_qreg_qubit_values=initial_expected_output_state, is_created_for_input_state=False
            )
            quantum_register_controls_grid_layout.addWidget(
                output_state_edit_field,
                qreg_controls_grid_row,
                2,
                alignment=QtCore.Qt.AlignmentFlag.AlignCenter,
            )

            edit_qubit_values_toggle_button = QtWidgets.QPushButton(
                "Edit qubit values", objectName=QREG_QUBIT_VALUES_TOGGLE_BUTTON_NAME_FORMAT.format(qreg_name=qreg_name)
            )
            quantum_register_controls_grid_layout.addWidget(edit_qubit_values_toggle_button, qreg_controls_grid_row, 3)
            # We need to ignore the checked parameter that is passed to the clicked slot of the QPushButton
            edit_qubit_values_toggle_button.clicked.connect(
                lambda _, associated_qreg_name=qreg_name: self.handle_qreg_qubit_values_edit_toggle_button_click(
                    associated_qreg_name
                )
            )

            qreg_controls_grid_row += 1
            quantum_register_layout_info_label = QtWidgets.QLabel(
                f"(First qubit: {qreg_layout.first_qubit_of_qreg} - Num. qubits: {qreg_layout.qreg_size})",
                objectName=QREG_LAYOUT_INFO_NAME_FORMAT.format(qreg_name=qreg_name),
            )
            quantum_register_layout_info_label.setStyleSheet("QLabel { color : grey; }")
            quantum_register_controls_grid_layout.addWidget(
                quantum_register_layout_info_label, qreg_controls_grid_row, 0
            )

            n_cols_in_quantum_register_controls_grid_layout: int = 3
            qreg_controls_grid_row += 1
            quantum_register_controls_grid_layout.addWidget(
                self.create_qubit_controls_groupbox(qreg_layout, initial_input_state, initial_expected_output_state),
                qreg_controls_grid_row,
                0,
                1,
                n_cols_in_quantum_register_controls_grid_layout + 1,
            )

            # Add a spacer item that will take the remaining horizontal space in the grid layout for each quantum register while vertical resizing should only take the minimum required spacing.
            quantum_register_controls_grid_spacer_widget = QtWidgets.QSpacerItem(
                2, 2, QtWidgets.QSizePolicy.Policy.Expanding, QtWidgets.QSizePolicy.Policy.Minimum
            )
            quantum_register_controls_grid_layout.addItem(
                quantum_register_controls_grid_spacer_widget, qreg_controls_grid_row, 5
            )
            qreg_controls_grid_row += 1

        # Add spacer item to take up remaining space between last quantum register elements and bottom of parent group box without stretching the spacing between the already added controls in the group box
        quantum_register_controls_grid_layout.addItem(
            QtWidgets.QSpacerItem(2, 2, QtWidgets.QSizePolicy.Policy.Minimum, QtWidgets.QSizePolicy.Policy.Expanding),
            qreg_controls_grid_row,
            0,
            1,
            5,
        )
        quantum_register_controls_grid_layout.setColumnStretch(0, 0)
        quantum_register_controls_grid_layout.setColumnStretch(1, 1)
        quantum_register_controls_grid_layout.setColumnStretch(2, 1)
        quantum_register_controls_grid_layout.setColumnStretch(3, 0)
        quantum_register_controls_grid_layout.setColumnStretch(4, 1)

        simulation_run_scroll_area = QtWidgets.QScrollArea()
        simulation_run_scroll_area.setWidget(self.simulation_run_wrapper_box)
        simulation_run_scroll_area.setWidgetResizable(True)
        simulation_run_scroll_area.setFrameShape(QtWidgets.QFrame.Shape.NoFrame)
        main_layout.addWidget(simulation_run_scroll_area)

        qreg_values_validation_error_label = QtWidgets.QLabel(objectName=QREG_VALUES_VALIDATION_ERROR_LABEL_NAME)
        qreg_values_validation_error_label.setStyleSheet("QLabel { color : red; }")
        main_layout.addWidget(qreg_values_validation_error_label)

        # Add dialog control buttons and link signals to slots of dialog
        self.dialog_button_box = QtWidgets.QDialogButtonBox(
            QtWidgets.QDialogButtonBox.StandardButton.Save | QtWidgets.QDialogButtonBox.StandardButton.Cancel
        )
        self.dialog_button_box.setCenterButtons(True)
        self.dialog_button_box.accepted.connect(self.accept)
        # TODO: Require confirmation to discard changes
        self.dialog_button_box.rejected.connect(self.reject)
        main_layout.addWidget(self.dialog_button_box)
        self.setLayout(main_layout)

    def handle_quantum_register_name_search(self) -> None:
        for qreg_layout in self.qreg_layouts:
            qreg_name: str = qreg_layout.qreg_name
            qreg_name_search_input_field: QtWidgets.QLineEdit | None = self.simulation_run_wrapper_box.findChild(
                QtWidgets.QLineEdit, QREG_SEARCH_INPUT_FIELD_NAME
            )
            qreg_name_label: QtWidgets.QLabel | None = self.simulation_run_wrapper_box.findChild(
                QtWidgets.QLabel, QREG_LABEL_NAME_FORMAT.format(qreg_name=qreg_name)
            )
            qreg_layout_info_label: QtWidgets.QLabel | None = self.simulation_run_wrapper_box.findChild(
                QtWidgets.QLabel, QREG_LAYOUT_INFO_NAME_FORMAT.format(qreg_name=qreg_name)
            )
            qreg_input_state_input_field: QtWidgets.QLineEdit | None = self.simulation_run_wrapper_box.findChild(
                QtWidgets.QLineEdit, QREG_INPUT_STATE_INPUT_FIELD_NAME_FORMAT.format(qreg_name=qreg_name)
            )
            qreg_output_state_input_field: QtWidgets.QLineEdit | None = self.simulation_run_wrapper_box.findChild(
                QtWidgets.QLineEdit, QREG_OUTPUT_STATE_INPUT_FIELD_NAME_FORMAT.format(qreg_name=qreg_name)
            )
            qreg_edit_qubit_values_toggle_button: QtWidgets.QPushButton | None = (
                self.simulation_run_wrapper_box.findChild(
                    QtWidgets.QPushButton, QREG_QUBIT_VALUES_TOGGLE_BUTTON_NAME_FORMAT.format(qreg_name=qreg_name)
                )
            )

            if (
                qreg_name_search_input_field is None
                or qreg_name_label is None
                or qreg_layout_info_label is None
                or qreg_input_state_input_field is None
                or qreg_output_state_input_field is None
                or qreg_edit_qubit_values_toggle_button is None
            ):
                # TODO: This should not happen
                continue

            should_control_be_visible: bool = qreg_name_search_input_field.text() is None or qreg_name.startswith(
                qreg_name_search_input_field.text()
            )
            qreg_name_label.setVisible(should_control_be_visible)
            qreg_layout_info_label.setVisible(should_control_be_visible)
            qreg_input_state_input_field.setVisible(should_control_be_visible)
            qreg_output_state_input_field.setVisible(should_control_be_visible)
            qreg_edit_qubit_values_toggle_button.setVisible(should_control_be_visible)

    def handle_input_state_qubit_value_checkbox_state_change(
        self, associated_qreg_name: str, associated_qubit: int, relative_qubit_index_in_quantum_register: int
    ) -> None:
        associated_qubit_value_checkbox: QtWidgets.QCheckBox | None = self.simulation_run_wrapper_box.findChild(
            QtWidgets.QCheckBox,
            INPUT_STATE_QUBIT_CHECKBOX_NAME_FORMAT.format(qubit=associated_qubit),
        )

        qreg_input_state_input_field: QtWidgets.QLineEdit | None = self.simulation_run_wrapper_box.findChild(
            QtWidgets.QLineEdit, QREG_INPUT_STATE_INPUT_FIELD_NAME_FORMAT.format(qreg_name=associated_qreg_name)
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
            STRINGIFIED_QUBIT_VALUE_FORMAT.format(stringified_qubit_value=stringified_updated_qubit_value)
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
            OUTPUT_STATE_QUBIT_CHECKBOX_NAME_FORMAT.format(qubit=associated_qubit),
        )

        qreg_output_state_input_field: QtWidgets.QLineEdit | None = self.simulation_run_wrapper_box.findChild(
            QtWidgets.QLineEdit, QREG_OUTPUT_STATE_INPUT_FIELD_NAME_FORMAT.format(qreg_name=associated_qreg_name)
        )

        if associated_qubit_value_checkbox is None or qreg_output_state_input_field is None:
            self.show_error_msg_dialog(
                title="Failed to updated qubit value",
                error_msg=f"Failed to locate all required Qt widgets required to update value of qubit {relative_qubit_index_in_quantum_register} of quantum register '{associated_qreg_name}' in output state!",
            )
            return

        updated_qubit_value: bool = associated_qubit_value_checkbox.checkState() == QtCore.Qt.CheckState.Checked
        stringified_updated_qubit_value: str = SimulationRunEditorDialog.stringify_qubit_value(
            updated_qubit_value, return_as_high_low_state=True
        )

        if self.edited_simulation_run_model.expected_output_state is None:
            stringified_updated_qubit_value = SimulationRunEditorDialog.stringify_qubit_value(
                None, return_as_high_low_state=True
            )
            associated_qubit_value_checkbox.setText(
                STRINGIFIED_QUBIT_VALUE_FORMAT.format(stringified_qubit_value=stringified_updated_qubit_value)
            )
            return

        if not self.edited_simulation_run_model.update_expected_output_state_qubit_value(
            associated_qubit, updated_qubit_value
        ):
            self.show_error_msg_dialog(
                title="Failed to updated qubit value",
                error_msg=f"Failed to update value of qubit {relative_qubit_index_in_quantum_register} of quantum register '{associated_qreg_name}' in output state to new value{stringified_updated_qubit_value}!",
            )
            return

        associated_qubit_value_checkbox.setText(
            STRINGIFIED_QUBIT_VALUE_FORMAT.format(stringified_qubit_value=stringified_updated_qubit_value)
        )
        curr_stringified_output_state: str = qreg_output_state_input_field.text()
        qreg_output_state_input_field.setText(
            curr_stringified_output_state[:relative_qubit_index_in_quantum_register]
            + SimulationRunEditorDialog.stringify_qubit_value(updated_qubit_value, return_as_high_low_state=False)
            + curr_stringified_output_state[relative_qubit_index_in_quantum_register + 1 :]
        )

    def show_error_msg_dialog(self, title: str, error_msg: str) -> None:
        QtWidgets.QMessageBox.critical(self, title, error_msg, defaultButton=QtWidgets.QMessageBox.StandardButton.Ok)

    def create_qreg_search_controls(self) -> QtWidgets.QLayout:
        qreg_search_controls_layout = QtWidgets.QHBoxLayout()
        qreg_search_label = QtWidgets.QLabel("Quantum register:")
        qreg_search_input_field = QtWidgets.QLineEdit(objectName=QREG_SEARCH_INPUT_FIELD_NAME)
        qreg_search_input_field.setPlaceholderText("<QUANTUM_REGISTER_NAME>")

        qreg_name_regular_expr = QtCore.QRegularExpression(R"(^([_A-Za-z]\w*)?$)")
        qreg_name_validator = QtGui.QRegularExpressionValidator(qreg_name_regular_expr, self)
        qreg_search_input_field.setValidator(qreg_name_validator)

        qreg_name_search_completer = QtWidgets.QCompleter([qreg_layout.qreg_name for qreg_layout in self.qreg_layouts])
        qreg_name_search_completer.setCaseSensitivity(QtCore.Qt.CaseSensitivity.CaseSensitive)
        qreg_search_input_field.setCompleter(qreg_name_search_completer)

        qreg_search_trigger_button = QtWidgets.QPushButton("Search")
        qreg_search_trigger_button.clicked.connect(self.handle_quantum_register_name_search)

        qreg_search_controls_layout.addWidget(qreg_search_label)
        qreg_search_controls_layout.addWidget(qreg_search_input_field)
        qreg_search_controls_layout.addWidget(qreg_search_trigger_button)
        return qreg_search_controls_layout

    def create_in_or_out_state_edit_field(
        self,
        qreg_layout: QuantumRegisterLayout,
        optional_qreg_qubit_values: syrec.n_bit_values_container | None,
        is_created_for_input_state: bool,
    ) -> LineEditWithDynamicWidth:
        in_or_out_state_edit_field = LineEditWithDynamicWidth(qreg_layout.qreg_size)
        in_or_out_state_edit_field.setObjectName(
            QREG_INPUT_STATE_INPUT_FIELD_NAME_FORMAT.format(qreg_name=qreg_layout.qreg_name)
            if is_created_for_input_state
            else QREG_OUTPUT_STATE_INPUT_FIELD_NAME_FORMAT.format(qreg_name=qreg_layout.qreg_name)
        )

        if optional_qreg_qubit_values is not None:
            in_or_out_state_edit_field.setText(
                SimulationRunEditorDialog.stringify_some_qubits_of_n_bit_values_container(
                    optional_qreg_qubit_values, qreg_layout.first_qubit_of_qreg, qreg_layout.qreg_size
                )
            )
            # output_state_edit_field.setEnabled(not self.are_qubits_values_readonly)
        else:
            in_or_out_state_edit_field.setEnabled(False)
            in_or_out_state_edit_field.setPlaceholderText("-")

        in_or_out_state_edit_field.setCursorPosition(0)
        in_or_out_state_edit_field.setAlignment(QtCore.Qt.AlignmentFlag.AlignJustify)
        # input_state_edit_field.setEnabled(not self.are_qubits_values_readonly and not self.is_input_state_readonly)
        in_or_out_state_edit_field.setValidator(
            QtGui.QRegularExpressionValidator(QtCore.QRegularExpression(R"^[0-1]*$"), self)
        )
        # The QLineEdit editingFinished signal is only triggered when its input satisfies the set inputmask/validator or when return/enter is pressed or if the QLineEdit loses focus.
        # However, during testing entering an invalid quantum register state in the input/output state validation seems to not trigger after entering an invalid value into an QLineEdit
        # and moving focus to another item. Only after entering a second invalid state the validation is triggered. Only when confirming the changes with enter/return is the validation triggered immediately.
        # One could in a custom overwrite of the QLineEdit class use the focusOutEvent to emit a custom signal when the QLineEdit element looses focus or use the application-level focusChanged signal to determine
        # whether the QLineEdit widget lost focus.
        in_or_out_state_edit_field.editingFinished.connect(
            lambda associated_qreg_name=qreg_layout.qreg_name, expected_text_length=qreg_layout.qreg_size, is_editing_input_state=is_created_for_input_state: (
                self.handle_input_or_output_state_text_change(
                    associated_qreg_name, expected_text_length, is_editing_input_state
                )
            )
        )
        in_or_out_state_edit_field.focusOut.connect(
            lambda associated_qreg_name=qreg_layout.qreg_name, expected_text_length=qreg_layout.qreg_size, is_editing_input_state=is_created_for_input_state: (
                self.handle_input_or_output_state_text_change(
                    associated_qreg_name, expected_text_length, is_editing_input_state
                )
            )
        )
        return in_or_out_state_edit_field

    def create_in_or_out_state_qubit_value_checkbox(
        self,
        associated_qreg_name: str,
        optional_qreg_qubit_values: syrec.n_bit_values_container | None,
        associated_qubit: int,
        relative_qubit_index_in_qreg: int,
        is_qubit_in_input_state: bool,
    ) -> QtWidgets.QCheckBox:
        qubit_value_checkbox = QtWidgets.QCheckBox(
            objectName=INPUT_STATE_QUBIT_CHECKBOX_NAME_FORMAT.format(qubit=associated_qubit)
            if is_qubit_in_input_state
            else OUTPUT_STATE_QUBIT_CHECKBOX_NAME_FORMAT.format(qubit=associated_qubit)
        )
        qubit_value_checkbox.setText(
            STRINGIFIED_QUBIT_VALUE_FORMAT.format(
                stringified_qubit_value=SimulationRunEditorDialog.stringify_qubit_value(
                    None
                    if optional_qreg_qubit_values is None
                    else optional_qreg_qubit_values.test(relative_qubit_index_in_qreg),
                    return_as_high_low_state=True,
                )
            )
        )
        qubit_value_checkbox.setEnabled(optional_qreg_qubit_values is not None)
        qubit_value_checkbox.checkStateChanged.connect(
            lambda _, associated_qreg_name=associated_qreg_name, associated_qubit=associated_qubit, relative_qubit_index_in_quantum_register=relative_qubit_index_in_qreg: (
                self.handle_input_state_qubit_value_checkbox_state_change(
                    associated_qreg_name, associated_qubit, relative_qubit_index_in_quantum_register
                )
                if is_qubit_in_input_state
                else self.handle_output_state_qubit_value_checkbox_state_change(
                    associated_qreg_name, associated_qubit, relative_qubit_index_in_quantum_register
                )
            )
        )
        return qubit_value_checkbox

    def create_search_controls_for_qubits_of_qreg(
        self, associated_qreg_layout: QuantumRegisterLayout
    ) -> QtWidgets.QLayout:
        qubit_search_layout = QtWidgets.QHBoxLayout()
        qubit_search_label = QtWidgets.QLabel("Qubit")
        qubit_search_layout.addWidget(qubit_search_label)

        associated_qreg_name: Final[str] = associated_qreg_layout.qreg_name
        first_qreg_qubit: Final[int] = associated_qreg_layout.first_qubit_of_qreg
        last_qreg_qubit: Final[int] = first_qreg_qubit + associated_qreg_layout.qreg_size

        qubit_search_input_field = QtWidgets.QLineEdit(
            objectName=QREG_QUBIT_SEARCH_INPUT_FIELD_NAME_FORMAT.format(qreg_name=associated_qreg_name)
        )
        qubit_search_input_field.setPlaceholderText("<QUBIT_LABEL>")

        qubit_search_completer = QtWidgets.QCompleter(
            SimulationRunEditorDialog.get_internal_qubit_labels_for_qreg(
                self.annotatable_quantum_computation, first_qreg_qubit, last_qreg_qubit
            )
        )
        qubit_search_completer.setCaseSensitivity(QtCore.Qt.CaseSensitivity.CaseSensitive)
        qubit_search_input_field.setCompleter(qubit_search_completer)

        qubit_search_layout.addWidget(qubit_search_input_field)

        qubit_search_trigger_button = QtWidgets.QPushButton("Search")
        qubit_search_trigger_button.clicked.connect(
            lambda _, associated_qreg_name=associated_qreg_name: self.handle_qubit_search_trigger_button_click(
                associated_qreg_name
            )
        )
        qubit_search_layout.addWidget(qubit_search_trigger_button)
        return qubit_search_layout

    # TODO: Scroll area
    def create_qubit_controls_groupbox(
        self,
        associated_qreg_layout: QuantumRegisterLayout,
        initial_input_state: syrec.n_bit_values_container,
        initial_expected_output_state: syrec.n_bit_values_container | None,
    ) -> QtWidgets.QWidget:
        input_output_qubits_value_controls_groupbox_layout = QtWidgets.QGridLayout()
        input_output_qubits_value_controls_groupbox_layout.addLayout(
            self.create_search_controls_for_qubits_of_qreg(associated_qreg_layout),
            0,
            0,
            1,
            1,
            QtCore.Qt.AlignmentFlag.AlignCenter,
        )

        first_qubit_of_qreg: Final[int] = associated_qreg_layout.first_qubit_of_qreg
        for qubit in range(first_qubit_of_qreg, first_qubit_of_qreg + associated_qreg_layout.qreg_size):
            # Due to first row (idx=0) of group box containing the qubit searc controls, the qubit value controls start in row 1.
            relative_qubit_index_in_qreg: int = qubit - first_qubit_of_qreg
            fetched_internal_qubit_label: str | None = self.annotatable_quantum_computation.get_qubit_label(
                qubit, syrec.qubit_label_type.internal
            )
            qubit_label = QtWidgets.QLabel(
                "Qubit: " + fetched_internal_qubit_label if fetched_internal_qubit_label is not None else "<UNKNOWN>",
                objectName=QUBIT_LABEL_NAME_FORMAT.format(qubit=qubit),
            )
            input_output_qubits_value_controls_groupbox_layout.addWidget(
                qubit_label, 1 + relative_qubit_index_in_qreg, 0
            )

            input_state_qubit_value_checkbox: QtWidgets.QCheckBox = self.create_in_or_out_state_qubit_value_checkbox(
                associated_qreg_layout.qreg_name,
                initial_input_state,
                associated_qubit=qubit,
                relative_qubit_index_in_qreg=relative_qubit_index_in_qreg,
                is_qubit_in_input_state=True,
            )
            input_output_qubits_value_controls_groupbox_layout.addWidget(
                input_state_qubit_value_checkbox,
                1 + relative_qubit_index_in_qreg,
                1,
                alignment=QtCore.Qt.AlignmentFlag.AlignLeft,
            )

            output_state_qubit_value_checkbox: QtWidgets.QCheckBox = self.create_in_or_out_state_qubit_value_checkbox(
                associated_qreg_layout.qreg_name,
                initial_expected_output_state,
                associated_qubit=qubit,
                relative_qubit_index_in_qreg=relative_qubit_index_in_qreg,
                is_qubit_in_input_state=False,
            )
            input_output_qubits_value_controls_groupbox_layout.addWidget(
                output_state_qubit_value_checkbox,
                1 + relative_qubit_index_in_qreg,
                2,
                alignment=QtCore.Qt.AlignmentFlag.AlignLeft,
            )

        # TODO: How can the column widths of the input fields and the checkbox columns be synced?
        input_output_qubits_value_controls_groupbox_layout.setColumnStretch(0, 1)
        input_output_qubits_value_controls_groupbox_layout.setColumnStretch(1, 1)
        input_output_qubits_value_controls_groupbox_layout.setColumnStretch(2, 1)

        input_output_qubits_value_controls_groupbox = QtWidgets.QGroupBox(
            "Qubit values",
            objectName=QREG_QUBIT_VALUES_GROUPBOX_NAME_FORMAT.format(qreg_name=associated_qreg_layout.qreg_name),
        )
        input_output_qubits_value_controls_groupbox.setVisible(False)
        input_output_qubits_value_controls_groupbox.setLayout(input_output_qubits_value_controls_groupbox_layout)
        return input_output_qubits_value_controls_groupbox

    def handle_qubit_search_trigger_button_click(self, associated_quantum_register_name: str) -> None:
        for qreg_layout in self.qreg_layouts:
            if qreg_layout.qreg_name != associated_quantum_register_name:
                continue

            qreg_qubits_groupbox: QtWidgets.QtWidget | None = self.simulation_run_wrapper_box.findChild(
                QtWidgets.QGroupBox,
                QREG_QUBIT_VALUES_GROUPBOX_NAME_FORMAT.format(qreg_name=associated_quantum_register_name),
            )
            if qreg_qubits_groupbox is None:
                # TODO: This should not happen
                continue

            qubit_search_input_field: QtWidgets.QtWidget | None = qreg_qubits_groupbox.findChild(
                QtWidgets.QLineEdit,
                QREG_QUBIT_SEARCH_INPUT_FIELD_NAME_FORMAT.format(qreg_name=associated_quantum_register_name),
            )
            if qubit_search_input_field is None:
                # TODO: This should not happen
                continue

            for qubit in range(
                qreg_layout.first_qubit_of_qreg, qreg_layout.first_qubit_of_qreg + qreg_layout.qreg_size
            ):
                qubit_value_label: QtWidgets.QtWidget | None = qreg_qubits_groupbox.findChild(
                    QtWidgets.QLabel, QUBIT_LABEL_NAME_FORMAT.format(qubit=qubit)
                )
                input_state_qubit_checkbox: QtWidgets.QCheckBox | None = qreg_qubits_groupbox.findChild(
                    QtWidgets.QCheckBox, INPUT_STATE_QUBIT_CHECKBOX_NAME_FORMAT.format(qubit=qubit)
                )
                output_state_qubit_checkbox: QtWidgets.QCheckBox | None = qreg_qubits_groupbox.findChild(
                    QtWidgets.QCheckBox, OUTPUT_STATE_QUBIT_CHECKBOX_NAME_FORMAT.format(qubit=qubit)
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
        for qreg_layout in self.qreg_layouts:
            qreg_name: str = qreg_layout.qreg_name
            # TODO: QtCore.Qt.FindDirectChildrenOnly
            qreg_input_state_input_field: QtWidgets.QtWidget | None = self.simulation_run_wrapper_box.findChild(
                QtWidgets.QLineEdit, QREG_INPUT_STATE_INPUT_FIELD_NAME_FORMAT.format(qreg_name=qreg_name)
            )
            qreg_output_state_input_field: QtWidgets.QtWidget | None = self.simulation_run_wrapper_box.findChild(
                QtWidgets.QLineEdit, QREG_OUTPUT_STATE_INPUT_FIELD_NAME_FORMAT.format(qreg_name=qreg_name)
            )
            qubit_values_groupbox: QtWidgets.QtWidget | None = self.simulation_run_wrapper_box.findChild(
                QtWidgets.QGroupBox, QREG_QUBIT_VALUES_GROUPBOX_NAME_FORMAT.format(qreg_name=qreg_name)
            )
            qubit_values_toggle_button: QtWidgets.QtWidget | None = self.simulation_run_wrapper_box.findChild(
                QtWidgets.QPushButton, QREG_QUBIT_VALUES_TOGGLE_BUTTON_NAME_FORMAT.format(qreg_name=qreg_name)
            )
            expected_output_state_value_toggle_button: QtWidgets.QtQWidget | None = (
                self.simulation_run_wrapper_box.findChild(
                    QtWidgets.QPushButton,
                    QREG_EXPECTED_OUTPUT_STATE_VALUE_INIT_TOGGLE_NAME.format(qreg_name=associated_qreg_name),
                )
            )
            qubit_values_groupbox_qubit_search_field: QtWidgets.QtWidget | None = (
                qubit_values_groupbox.findChild(
                    QtWidgets.QLineEdit, QREG_QUBIT_SEARCH_INPUT_FIELD_NAME_FORMAT.format(qreg_name=qreg_name)
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
                or expected_output_state_value_toggle_button is None
                or qubit_values_groupbox_qubit_search_field is None
            ):
                # TODO: This should not happen
                continue

            if qreg_name == associated_qreg_name and not qubit_values_groupbox.isVisible():
                qubit_values_groupbox.setVisible(True)
                qubit_values_toggle_button.setText("Toggle qubit values edit")
                qreg_input_state_input_field.setEnabled(False)
                qreg_output_state_input_field.setEnabled(False)
                expected_output_state_value_toggle_button.setEnabled(False)
            else:
                qubit_values_groupbox.setVisible(False)
                qubit_values_toggle_button.setText("Edit qubit values")
                expected_output_state_value_toggle_button.setEnabled(True)
                qreg_input_state_input_field.setEnabled(True)
                qreg_output_state_input_field.setEnabled(qreg_output_state_input_field.text() != "")  # noqa: PLC1901
                qubit_values_groupbox_qubit_search_field.setText("")
                self.handle_qubit_search_trigger_button_click(associated_qreg_name)

    def handle_init_expected_output_state_button_click(self, associated_qreg_name: str) -> None:
        expected_output_state_value_toggle_button: QtWidgets.QtQWidget | None = (
            self.simulation_run_wrapper_box.findChild(
                QtWidgets.QPushButton,
                QREG_EXPECTED_OUTPUT_STATE_VALUE_INIT_TOGGLE_NAME.format(qreg_name=associated_qreg_name),
            )
        )

        if expected_output_state_value_toggle_button is None:
            return

        should_reset_output_state: bool = self.edited_simulation_run_model.expected_output_state is not None
        if should_reset_output_state:
            self.edited_simulation_run_model.expected_output_state = None
            expected_output_state_value_toggle_button.setText("Init output state")
        else:
            self.edited_simulation_run_model.initialize_expected_output_state_as_copy_of_input_state()
            expected_output_state_value_toggle_button.setText("Clear output state")

        for qreg_layout in self.qreg_layouts:
            qreg_name: str = qreg_layout.qreg_name
            qreg_input_state_input_field: QtWidgets.QtWidget | None = self.simulation_run_wrapper_box.findChild(
                QtWidgets.QLineEdit, QREG_INPUT_STATE_INPUT_FIELD_NAME_FORMAT.format(qreg_name=qreg_name)
            )

            qreg_output_state_input_field: QtWidgets.QtWidget | None = self.simulation_run_wrapper_box.findChild(
                QtWidgets.QLineEdit, QREG_OUTPUT_STATE_INPUT_FIELD_NAME_FORMAT.format(qreg_name=qreg_name)
            )

            if qreg_input_state_input_field is None or qreg_output_state_input_field is None:
                # TODO: This should not happen
                return

            if should_reset_output_state:
                qreg_output_state_input_field.setText("")
                qreg_output_state_input_field.setEnabled(False)
            else:
                qreg_output_state_input_field.setText(
                    SimulationRunEditorDialog.stringify_some_qubits_of_n_bit_values_container(
                        self.edited_simulation_run_model.expected_output_state,
                        qreg_layout.first_qubit_of_qreg,
                        qreg_layout.qreg_size,
                    )
                )
                qreg_output_state_input_field.setEnabled(qreg_input_state_input_field.isEnabled())

            for qubit in range(
                qreg_layout.first_qubit_of_qreg, qreg_layout.first_qubit_of_qreg + qreg_layout.qreg_size
            ):
                associated_qubit_value_checkbox: QtWidgets.QCheckBox | None = self.simulation_run_wrapper_box.findChild(
                    QtWidgets.QCheckBox, OUTPUT_STATE_QUBIT_CHECKBOX_NAME_FORMAT.format(qubit=qubit)
                )
                if associated_qubit_value_checkbox is None:
                    # TODO: This should not happen
                    return

                qubit_value: bool | None = (
                    self.edited_simulation_run_model.expected_output_state.test(qubit)  # type: ignore[union-attr]
                    if not should_reset_output_state
                    else None
                )
                associated_qubit_value_checkbox.setChecked(qubit_value if qubit_value is not None else False)
                associated_qubit_value_checkbox.setText(
                    STRINGIFIED_QUBIT_VALUE_FORMAT.format(
                        stringified_qubit_value=SimulationRunEditorDialog.stringify_qubit_value(
                            qubit_value, return_as_high_low_state=True
                        )
                    )
                )
                associated_qubit_value_checkbox.setEnabled(not should_reset_output_state)

    def handle_input_or_output_state_text_change(
        self, associated_qreg_name: str, expected_qreg_size: int, is_editing_input_state: bool
    ) -> None:
        input_state_text_field: QtWidgets.QLineEdit | None = self.simulation_run_wrapper_box.findChild(
            QtWidgets.QLineEdit, QREG_INPUT_STATE_INPUT_FIELD_NAME_FORMAT.format(qreg_name=associated_qreg_name)
        )

        output_state_text_field: QtWidgets.QLineEdit | None = self.simulation_run_wrapper_box.findChild(
            QtWidgets.QLineEdit, QREG_OUTPUT_STATE_INPUT_FIELD_NAME_FORMAT.format(qreg_name=associated_qreg_name)
        )

        qreg_qubit_values_edit_toggle_button: QtWidgets.QPushButton | None = self.simulation_run_wrapper_box.findChild(
            QtWidgets.QPushButton,
            QREG_QUBIT_VALUES_TOGGLE_BUTTON_NAME_FORMAT.format(qreg_name=associated_qreg_name),
        )

        expected_output_state_init_button: QtWidgets.QPushButton | None = self.simulation_run_wrapper_box.findChild(
            QtWidgets.QPushButton, QREG_EXPECTED_OUTPUT_STATE_VALUE_INIT_TOGGLE_NAME
        )

        dialog_save_button: QtWidgets.QPushButton | None = self.dialog_button_box.button(
            QtWidgets.QDialogButtonBox.StandardButton.Save
        )
        qreg_values_validation_error_lbl: QtWidgets.QLabel | None = self.findChild(
            QtWidgets.QLabel, QREG_VALUES_VALIDATION_ERROR_LABEL_NAME
        )

        if (
            input_state_text_field is None
            or output_state_text_field is None
            or qreg_qubit_values_edit_toggle_button is None
            or expected_output_state_init_button is None
            or dialog_save_button is None
            or qreg_values_validation_error_lbl is None
        ):
            # TODO: This should not happen
            return

        are_stringified_qreg_contents_valid: bool = False
        if is_editing_input_state:
            are_stringified_qreg_contents_valid = (
                input_state_text_field.hasAcceptableInput() and len(input_state_text_field.text()) == expected_qreg_size
            )
        else:
            are_stringified_qreg_contents_valid = (
                output_state_text_field.hasAcceptableInput()
                and len(output_state_text_field.text()) == expected_qreg_size
            )

        qreg_qubit_values_edit_toggle_button.setEnabled(are_stringified_qreg_contents_valid)
        expected_output_state_init_button.setEnabled(are_stringified_qreg_contents_valid)
        dialog_save_button.setEnabled(are_stringified_qreg_contents_valid)

        if is_editing_input_state:
            output_state_text_field.setEnabled(
                are_stringified_qreg_contents_valid
                if self.edited_simulation_run_model.expected_output_state is not None
                else False
            )

            if not are_stringified_qreg_contents_valid:
                qreg_values_validation_error_lbl.setText(
                    QREG_VALUES_VALIDATION_ERROR_FORMAT.format(
                        qreg_name=associated_qreg_name,
                        expected_num_qubit_values=expected_qreg_size,
                        actual_num_qubit_values=len(input_state_text_field.text()),
                        input_or_output_state_ident="input",
                    )
                )
            else:
                qreg_values_validation_error_lbl.setText("")
        else:
            input_state_text_field.setEnabled(
                are_stringified_qreg_contents_valid
                if self.edited_simulation_run_model.expected_output_state is not None
                else False
            )

            if not are_stringified_qreg_contents_valid:
                qreg_values_validation_error_lbl.setText(
                    QREG_VALUES_VALIDATION_ERROR_FORMAT.format(
                        qreg_name=associated_qreg_name,
                        expected_num_qubit_values=expected_qreg_size,
                        actual_num_qubit_values=len(output_state_text_field.text()),
                        input_or_output_state_ident="output",
                    )
                )
            else:
                qreg_values_validation_error_lbl.setText("")

        for qreg_layout in self.qreg_layouts:
            if qreg_layout.qreg_name == associated_qreg_name:
                if not are_stringified_qreg_contents_valid:
                    continue

                first_qubit_of_qreg: int = qreg_layout.first_qubit_of_qreg
                n_qubits_of_qreg: int = qreg_layout.qreg_size

                qubit_in_input_or_output_state: int
                new_qubit_value: bool
                if is_editing_input_state:
                    for relative_qubit_idx_in_qreg in range(n_qubits_of_qreg):
                        qubit_in_input_or_output_state = first_qubit_of_qreg + relative_qubit_idx_in_qreg
                        associated_input_state_qubit_value_checkbox: QtWidgets.QCheckBox | None = (
                            self.simulation_run_wrapper_box.findChild(
                                QtWidgets.QCheckBox,
                                INPUT_STATE_QUBIT_CHECKBOX_NAME_FORMAT.format(qubit=qubit_in_input_or_output_state),
                            )
                        )

                        if associated_input_state_qubit_value_checkbox is None:
                            # TODO: This should not happen
                            return

                        new_qubit_value = input_state_text_field.text()[relative_qubit_idx_in_qreg] == "1"
                        self.edited_simulation_run_model.update_input_state_qubit_value(
                            qubit_in_input_or_output_state, new_qubit_value
                        )
                        associated_input_state_qubit_value_checkbox.setChecked(new_qubit_value)
                        associated_input_state_qubit_value_checkbox.setText(
                            STRINGIFIED_QUBIT_VALUE_FORMAT.format(
                                stringified_qubit_value=SimulationRunEditorDialog.stringify_qubit_value(
                                    new_qubit_value, return_as_high_low_state=True
                                )
                            )
                        )
                else:
                    for relative_qubit_idx_in_qreg in range(n_qubits_of_qreg):
                        qubit_in_input_or_output_state = first_qubit_of_qreg + relative_qubit_idx_in_qreg
                        associated_output_state_qubit_value_checkbox: QtWidgets.QCheckBox | None = (
                            self.simulation_run_wrapper_box.findChild(
                                QtWidgets.QCheckBox,
                                OUTPUT_STATE_QUBIT_CHECKBOX_NAME_FORMAT.format(qubit=qubit_in_input_or_output_state),
                            )
                        )
                        if associated_output_state_qubit_value_checkbox is None:
                            # TODO: This should not happen
                            return

                        new_qubit_value = output_state_text_field.text()[relative_qubit_idx_in_qreg] == "1"
                        self.edited_simulation_run_model.update_expected_output_state_qubit_value(
                            qubit_in_input_or_output_state, new_qubit_value
                        )
                        associated_output_state_qubit_value_checkbox.setChecked(new_qubit_value)
                        associated_output_state_qubit_value_checkbox.setText(
                            STRINGIFIED_QUBIT_VALUE_FORMAT.format(
                                stringified_qubit_value=SimulationRunEditorDialog.stringify_qubit_value(
                                    new_qubit_value, return_as_high_low_state=True
                                )
                            )
                        )

            qreg_name: str = qreg_layout.qreg_name
            not_edited_input_state_text_field: QtWidgets.QLineEdit | None = self.simulation_run_wrapper_box.findChild(
                QtWidgets.QLineEdit, QREG_INPUT_STATE_INPUT_FIELD_NAME_FORMAT.format(qreg_name=qreg_name)
            )

            not_edited_output_state_text_field: QtWidgets.QLineEdit | None = self.simulation_run_wrapper_box.findChild(
                QtWidgets.QLineEdit, QREG_OUTPUT_STATE_INPUT_FIELD_NAME_FORMAT.format(qreg_name=qreg_name)
            )

            not_edited_qreg_qubit_values_edit_toggle_button: QtWidgets.QPushButton | None = (
                self.simulation_run_wrapper_box.findChild(
                    QtWidgets.QPushButton, QREG_QUBIT_VALUES_TOGGLE_BUTTON_NAME_FORMAT.format(qreg_name=qreg_name)
                )
            )

            if (
                not_edited_input_state_text_field is None
                or not_edited_output_state_text_field is None
                or not_edited_qreg_qubit_values_edit_toggle_button is None
            ):
                # TODO: This should not happen
                return

            not_edited_input_state_text_field.setEnabled(are_stringified_qreg_contents_valid)
            not_edited_output_state_text_field.setEnabled(
                are_stringified_qreg_contents_valid
                if self.edited_simulation_run_model.expected_output_state is not None
                else False
            )
            not_edited_qreg_qubit_values_edit_toggle_button.setEnabled(are_stringified_qreg_contents_valid)

    @staticmethod
    def stringify_some_qubits_of_n_bit_values_container(
        n_bit_values_container: syrec.n_bit_values_container, first_qubit: int, n_qubits: int
    ) -> str:
        if (
            first_qubit >= n_bit_values_container.size()
            or first_qubit + (n_qubits - 1) >= n_bit_values_container.size()
        ):
            return ""
        return "".join([
            "1" if n_bit_values_container.test(i) else "0" for i in range(first_qubit, first_qubit + n_qubits)
        ])

    @staticmethod
    def get_internal_qubit_labels_for_qreg(
        annotatable_quantum_computation: syrec.annotatable_quantum_computation,
        first_qubit_of_qreg: int,
        n_qubits_in_qreg: int,
    ) -> list[str]:
        internal_qubit_labels: list[str] = []
        for qubit in range(first_qubit_of_qreg, first_qubit_of_qreg + n_qubits_in_qreg):
            fetched_internal_qubit_label: str | None = annotatable_quantum_computation.get_qubit_label(
                qubit, syrec.qubit_label_type.internal
            )
            if fetched_internal_qubit_label is None:
                continue
            internal_qubit_labels.append(fetched_internal_qubit_label)
        return internal_qubit_labels

    @staticmethod
    def stringify_qubit_value(qubit_value: bool | None, return_as_high_low_state: bool) -> str:
        if qubit_value is None:
            return "UNKNOWN" if return_as_high_low_state else "-"

        if qubit_value is True:
            return "HIGH" if return_as_high_low_state else "1"

        return "LOW" if return_as_high_low_state else "0"
