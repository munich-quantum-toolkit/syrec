# Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
# Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

from __future__ import annotations

import sys
from dataclasses import dataclass
from enum import Enum
from typing import TYPE_CHECKING, Final, cast

from PyQt6 import QtCore, QtGui, QtWidgets

if sys.version_info >= (3, 12):
    from typing import override
else:
    from typing_extensions import override

if sys.version_info >= (3, 11):
    from typing import assert_never
else:
    from typing_extensions import assert_never

from mqt.syrec import QubitLabelType

from ...logger_utils import log_error_to_console
from ...message_box_utils import MessageBoxType, show_and_request_ok_in_optionally_cancellable_notification
from ...widget_check_utils import assert_all_required_widgets_found_or_close_dialog
from ..simulation_run_model import (
    ANNOTATABLE_QUANTUM_COMPUTATION_QT_ROLE,
    QUANTUM_REGISTER_LAYOUT_QT_ROLE,
)
from .base_progress_dialog import BaseProgressDialog

if TYPE_CHECKING:
    from collections.abc import Iterable

    from mqt.syrec import AnnotatableQuantumComputation, NBitValuesContainer

    from ..simulation_run_model import (
        QuantumRegisterLayout,
        SimulationRunModel,
    )


class QubitLocation(Enum):
    INPUT_STATE = 0
    EXPECTED_OUTPUT_STATE = 1
    ACTUAL_OUTPUT_STATE = 2


@dataclass(frozen=True)
class QubitValueLabelAndCheckbox:
    optional_label: QtWidgets.QLabel | None
    checkbox: QtWidgets.QCheckBox


class LineEditWithDynamicWidth(QtWidgets.QLineEdit):  # type: ignore[misc]
    focusOut = QtCore.pyqtSignal()  # noqa: N815

    def __init__(self, expected_max_num_characters: int, parent: QtWidgets.QWidget = None):
        super().__init__(parent)
        self._expected_max_num_characters = expected_max_num_characters
        self.setMaxLength(expected_max_num_characters)
        self.setSizePolicy(QtWidgets.QSizePolicy.Policy.Preferred, QtWidgets.QSizePolicy.Policy.Fixed)

    # Make the widget greedy: whenever the layout offers more
    # than the nominal width, grab it.
    def sizeHint(self) -> QtCore.QSize:  # noqa: N802
        sh = super().sizeHint()
        fm = QtGui.QFontMetrics(self.font())
        nominal = fm.boundingRect("W" * self._expected_max_num_characters).width()
        # use the offered width
        preferred = max(nominal, self.width())
        return QtCore.QSize(preferred, sh.height())

    def focusOutEvent(self, ev: QtGui.QFocusEvent) -> None:  # noqa: N802
        super().focusOutEvent(ev)
        self.focusOut.emit()


@dataclass(frozen=True)
class QRegContentsLabelAndCheckbox:
    optional_label: QtWidgets.QLabel | None
    contents_widget: QtWidgets.QLabel | LineEditWithDynamicWidth


QUBIT_LABEL_NAME_FORMAT: Final[str] = "q_{qubit:d}_lbl"
INPUT_STATE_QUBIT_CHECKBOX_NAME_FORMAT: Final[str] = "q_{qubit:d}_in_checkB"
EXPECTED_OUTPUT_STATE_QUBIT_CHECKBOX_NAME_FORMAT: Final[str] = "q_{qubit:d}_expected_out_checkB"
EXPECTED_OUTPUT_STATE_QUBIT_CHECKBOX_LABEL_NAME_FORMAT: Final[str] = "q_{qubit:d}_expected_out_checkB_lbl"
ACTUAL_OUTPUT_STATE_QUBIT_CHECKBOX_NAME_FORMAT: Final[str] = "q_{qubit:d}_actual_out_checkB"
ACTUAL_OUTPUT_STATE_QUBIT_CHECKBOX_LABEL_NAME_FORMAT: Final[str] = "q_{qubit:d}_actual_out_checkB_lbl"

QREG_QUBIT_VALUES_GROUPBOX_NAME_FORMAT: Final[str] = "qreg_{qreg_name:s}_qubit_values_groupbox"
QREG_LABEL_NAME_FORMAT: Final[str] = "qreg_{qreg_name:s}_lbl"
QREG_LAYOUT_INFO_NAME_FORMAT: Final[str] = "qreg_{qreg_name:s}_layout_info_lbl"
QREG_INPUT_STATE_INPUT_FIELD_NAME_FORMAT: Final[str] = "qreg_{qreg_name:s}_input_state"

EXPECTED_QREG_OUTPUT_STATE_INPUT_FIELD_NAME_FORMAT: Final[str] = "qreg_{qreg_name:s}_expected_output_state"
EXPECTED_QREG_OUTPUT_STATE_PREFIX_LABEL_NAME_FORMAT: Final[str] = "qreg_{qreg_name:s}_expected_output_state_lbl"

ACTUAL_QREG_OUTPUT_STATE_INPUT_FIELD_NAME_FORMAT: Final[str] = "qreg_{qreg_name:s}_actual_output_state"
ACTUAL_QREG_OUTPUT_STATE_PREFIX_LABEL_NAME_FORMAT: Final[str] = "qreg_{qreg_name:s}_actual_output_state_lbl"

QREG_QUBIT_VALUES_TOGGLE_BUTTON_NAME_FORMAT: Final[str] = "qreg_{qreg_name:s}_qubit_values_toggle"
QREG_QUBIT_SEARCH_INPUT_FIELD_NAME_FORMAT: Final[str] = "qreg_{qreg_name:s}_qubit_search_input"

QREG_EXPECTED_OUTPUT_STATE_VALUE_INIT_TOGGLE_NAME: Final[str] = "output_state_value_toggle"
QREG_SEARCH_INPUT_FIELD_NAME: Final[str] = "qreg_name_search_input_field"
QREG_SEARCH_TRIGGER_BUTTON_NAME: Final[str] = "qreg_name_trigger_btn"
QREG_VALUES_VALIDATION_ERROR_LABEL_NAME: Final[str] = "qreg_values_validation_err_lbl"

STRINGIFIED_QUBIT_VALUE_FORMAT: Final[str] = "(Value: {stringified_qubit_value:s})"
QREG_VALUES_VALIDATION_ERROR_FORMAT: Final[str] = (
    "Qubit values of quantum register '{qreg_name:s}' can only be defined as a combination of '0' or '1' literals. Additionally, the value of all qubits of the quantum register (n={expected_num_qubit_values:d}) must be specified but only {actual_num_qubit_values:d} were defined in the {input_or_output_state_ident:s} state!"
)

EDIT_OUTPUT_STATE_QUBIT_VALUES: Final[str] = "Edit qubit values"
TOGGLE_OUTPUT_STATE_QUBIT_VALUES_EDIT: Final[str] = "Toggle qubit values edit"


class SimulationRunEditorDialog(QtWidgets.QDialog):  # type: ignore[misc]
    def __init__(
        self,
        simulation_run_model_index: QtCore.QModelIndex,
        copy_of_reference_edit_sim_run_model: SimulationRunModel,
        parent: QtWidgets.QWidget,
    ) -> None:
        super().__init__(parent)
        self.simulation_run_model_index: Final[QtCore.QModelIndex] = simulation_run_model_index

        self._failed_due_to_internal_error: bool = False
        self.edited_simulation_run_model: SimulationRunModel = copy_of_reference_edit_sim_run_model

        self._qreg_layouts: list[QuantumRegisterLayout] = simulation_run_model_index.data(
            QUANTUM_REGISTER_LAYOUT_QT_ROLE
        )
        self._annotatable_quantum_computation: AnnotatableQuantumComputation = simulation_run_model_index.data(
            ANNOTATABLE_QUANTUM_COMPUTATION_QT_ROLE
        )

        initial_input_state: NBitValuesContainer = self.edited_simulation_run_model.input_state
        initial_expected_output_state: NBitValuesContainer | None = (
            self.edited_simulation_run_model.expected_output_state
        )
        initial_actual_output_state: NBitValuesContainer | None = self.edited_simulation_run_model.actual_output_state

        # Ensure the dialog is deleted when closed this may not be strictly necessary but seems to be a good cleanup practice
        self.setAttribute(QtCore.Qt.WidgetAttribute.WA_DeleteOnClose)
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

        self._simulation_run_wrapper_box = QtWidgets.QGroupBox(
            "Simulation run #" + str(simulation_run_model_index.row())
        )

        quantum_register_controls_grid_layout = QtWidgets.QGridLayout()
        self._simulation_run_wrapper_box.setLayout(quantum_register_controls_grid_layout)
        quantum_register_controls_grid_layout.addLayout(
            self._create_qreg_search_controls(), 0, 0, alignment=QtCore.Qt.AlignmentFlag.AlignCenter
        )

        init_expected_output_state_button = QtWidgets.QPushButton(
            "Init output state"
            if self.edited_simulation_run_model.expected_output_state is None
            else "Clear output state",
            objectName=QREG_EXPECTED_OUTPUT_STATE_VALUE_INIT_TOGGLE_NAME,
        )
        init_expected_output_state_button.clicked.connect(self._handle_init_expected_output_state_button_click)

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

        qreg_controls_grid_row: int = 2
        for qreg_layout in self._qreg_layouts:
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

            input_state_widgets: QRegContentsLabelAndCheckbox = self._create_in_or_out_state_edit_field(
                qreg_layout, optional_qreg_qubit_values=initial_input_state, qubit_location=QubitLocation.INPUT_STATE
            )
            quantum_register_controls_grid_layout.addWidget(
                input_state_widgets.contents_widget,
                qreg_controls_grid_row,
                1,
                alignment=QtCore.Qt.AlignmentFlag.AlignCenter,
            )

            expected_output_state_widgets: QRegContentsLabelAndCheckbox = self._create_in_or_out_state_edit_field(
                qreg_layout,
                optional_qreg_qubit_values=initial_expected_output_state,
                qubit_location=QubitLocation.EXPECTED_OUTPUT_STATE,
            )
            actual_output_state_widgets: QRegContentsLabelAndCheckbox = self._create_in_or_out_state_edit_field(
                qreg_layout,
                optional_qreg_qubit_values=initial_actual_output_state,
                qubit_location=QubitLocation.ACTUAL_OUTPUT_STATE,
            )

            output_state_widgets_layout: QtWidgets.QGridLayout = QtWidgets.QGridLayout()
            output_state_widgets_layout.addWidget(expected_output_state_widgets.optional_label, 0, 0)
            output_state_widgets_layout.addWidget(expected_output_state_widgets.contents_widget, 0, 1)
            output_state_widgets_layout.addWidget(actual_output_state_widgets.optional_label, 1, 0)
            output_state_widgets_layout.addWidget(actual_output_state_widgets.contents_widget, 1, 1)

            output_state_widgets_layout.setColumnStretch(0, 1)
            output_state_widgets_layout.setColumnStretch(1, 0)
            output_state_widgets_layout.setColumnStretch(2, 1)
            quantum_register_controls_grid_layout.addLayout(
                output_state_widgets_layout,
                qreg_controls_grid_row,
                2,
                2,
                1,
                alignment=QtCore.Qt.AlignmentFlag.AlignLeft,
            )

            edit_qubit_values_toggle_button = QtWidgets.QPushButton(
                "Edit qubit values", objectName=QREG_QUBIT_VALUES_TOGGLE_BUTTON_NAME_FORMAT.format(qreg_name=qreg_name)
            )
            quantum_register_controls_grid_layout.addWidget(
                edit_qubit_values_toggle_button, qreg_controls_grid_row, 3, 2, 1
            )
            # We need to ignore the checked parameter that is passed to the clicked slot of the QPushButton
            edit_qubit_values_toggle_button.clicked.connect(
                lambda _, associated_qreg_name=qreg_name: self._handle_qreg_qubit_values_edit_toggle_button_click(
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
                self._create_qubit_controls_groupbox(
                    qreg_layout, initial_input_state, initial_expected_output_state, initial_actual_output_state
                ),
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
        simulation_run_scroll_area.setWidget(self._simulation_run_wrapper_box)
        simulation_run_scroll_area.setWidgetResizable(True)
        simulation_run_scroll_area.setFrameShape(QtWidgets.QFrame.Shape.NoFrame)
        main_layout.addWidget(simulation_run_scroll_area)

        qreg_values_validation_error_label = QtWidgets.QLabel(objectName=QREG_VALUES_VALIDATION_ERROR_LABEL_NAME)
        qreg_values_validation_error_label.setStyleSheet("QLabel { color : red; }")
        main_layout.addWidget(qreg_values_validation_error_label)

        # Add dialog control buttons and link signals to slots of dialog
        self._dialog_button_box = QtWidgets.QDialogButtonBox(
            QtWidgets.QDialogButtonBox.StandardButton.Save | QtWidgets.QDialogButtonBox.StandardButton.Cancel
        )
        self._dialog_button_box.setCenterButtons(True)
        self._dialog_button_box.accepted.connect(self.accept)
        self._dialog_button_box.rejected.connect(self.reject)
        main_layout.addWidget(self._dialog_button_box)
        self.setLayout(main_layout)

    @override
    def reject(self) -> None:
        # Ask for confirmation before closing dialog
        if self._failed_due_to_internal_error or show_and_request_ok_in_optionally_cancellable_notification(
            message_box_type=MessageBoxType.QUESTION,
            message_box_parent=self,
            message_box_title="Confirm close",
            message_box_content="Are you sure you want stop editing the simulation run, all unsaved changes will be lost?",
            is_cancellable=True,
            log_contents=False,
        ):
            super().reject()

    @override
    def closeEvent(self, event: QtGui.QCloseEvent) -> None:
        if self._failed_due_to_internal_error:
            super().reject()
            return

        # Ask for confirmation before closing dialog
        if show_and_request_ok_in_optionally_cancellable_notification(
            message_box_type=MessageBoxType.QUESTION,
            message_box_parent=self,
            message_box_title="Confirm close",
            message_box_content="Are you sure you want stop editing the simulation run, all unsaved changes will be lost?",
            is_cancellable=True,
            log_contents=False,
        ):
            super().reject()
        else:
            event.ignore()

    @QtCore.pyqtSlot()  # type: ignore[untyped-decorator]
    def _handle_quantum_register_name_search(self) -> None:
        for qreg_layout in self._qreg_layouts:
            qreg_name: str = qreg_layout.qreg_name
            optional_qreg_name_search_input_field: QtWidgets.QLineEdit | None = (
                self._simulation_run_wrapper_box.findChild(QtWidgets.QLineEdit, QREG_SEARCH_INPUT_FIELD_NAME)
            )
            optional_qreg_name_label: QtWidgets.QLabel | None = self._simulation_run_wrapper_box.findChild(
                QtWidgets.QLabel, QREG_LABEL_NAME_FORMAT.format(qreg_name=qreg_name)
            )
            optional_qreg_layout_info_label: QtWidgets.QLabel | None = self._simulation_run_wrapper_box.findChild(
                QtWidgets.QLabel, QREG_LAYOUT_INFO_NAME_FORMAT.format(qreg_name=qreg_name)
            )
            optional_qreg_input_state_input_field: QtWidgets.QLineEdit | None = (
                self._simulation_run_wrapper_box.findChild(
                    QtWidgets.QLineEdit, QREG_INPUT_STATE_INPUT_FIELD_NAME_FORMAT.format(qreg_name=qreg_name)
                )
            )
            optional_qreg_expected_output_state_label: QtWidgets.QLabel | None = (
                self._simulation_run_wrapper_box.findChild(
                    QtWidgets.QLabel, EXPECTED_QREG_OUTPUT_STATE_PREFIX_LABEL_NAME_FORMAT.format(qreg_name=qreg_name)
                )
            )
            optional_qreg_expected_output_state_input_field: QtWidgets.QLineEdit | None = (
                self._simulation_run_wrapper_box.findChild(
                    QtWidgets.QLineEdit, EXPECTED_QREG_OUTPUT_STATE_INPUT_FIELD_NAME_FORMAT.format(qreg_name=qreg_name)
                )
            )
            optional_qreg_actual_output_state_label: QtWidgets.QLabel | None = (
                self._simulation_run_wrapper_box.findChild(
                    QtWidgets.QLabel, ACTUAL_QREG_OUTPUT_STATE_PREFIX_LABEL_NAME_FORMAT.format(qreg_name=qreg_name)
                )
            )
            optional_qreg_actual_output_state_widget: QtWidgets.QLabel | None = (
                self._simulation_run_wrapper_box.findChild(
                    QtWidgets.QLabel, ACTUAL_QREG_OUTPUT_STATE_INPUT_FIELD_NAME_FORMAT.format(qreg_name=qreg_name)
                )
            )
            optional_qreg_edit_qubit_values_toggle_button: QtWidgets.QPushButton | None = (
                self._simulation_run_wrapper_box.findChild(
                    QtWidgets.QPushButton, QREG_QUBIT_VALUES_TOGGLE_BUTTON_NAME_FORMAT.format(qreg_name=qreg_name)
                )
            )

            if not self._assert_all_required_widgets_found_or_close_dialog(
                [
                    optional_qreg_name_search_input_field,
                    optional_qreg_name_label,
                    optional_qreg_layout_info_label,
                    optional_qreg_input_state_input_field,
                    optional_qreg_expected_output_state_label,
                    optional_qreg_expected_output_state_input_field,
                    optional_qreg_actual_output_state_label,
                    optional_qreg_actual_output_state_widget,
                    optional_qreg_edit_qubit_values_toggle_button,
                ],
                f"Failed to locate all required Qt widgets required for quantum register '{qreg_name}' during quantum register search",
            ):
                return

            qreg_name_search_input_field = cast("QtWidgets.QLineEdit", optional_qreg_name_search_input_field)
            qreg_name_label = cast("QtWidgets.QLabel", optional_qreg_name_label)
            qreg_layout_info_label = cast("QtWidgets.QLabel", optional_qreg_layout_info_label)
            qreg_input_state_input_field = cast("QtWidgets.QLineEdit", optional_qreg_input_state_input_field)
            qreg_expected_output_state_label = cast("QtWidgets.QLabel", optional_qreg_expected_output_state_label)
            qreg_expected_output_state_input_field = cast(
                "QtWidgets.QLineEdit", optional_qreg_expected_output_state_input_field
            )
            qreg_actual_output_state_label = cast("QtWidgets.QLabel", optional_qreg_actual_output_state_label)
            qreg_actual_output_state_widget = cast("QtWidgets.QLabel", optional_qreg_actual_output_state_widget)
            qreg_edit_qubit_values_toggle_button = cast(
                "QtWidgets.QPushButton", optional_qreg_edit_qubit_values_toggle_button
            )
            should_control_be_visible: bool = qreg_name_search_input_field.text() is None or qreg_name.startswith(
                qreg_name_search_input_field.text()
            )
            qreg_name_label.setVisible(should_control_be_visible)
            qreg_layout_info_label.setVisible(should_control_be_visible)
            qreg_input_state_input_field.setVisible(should_control_be_visible)
            qreg_expected_output_state_label.setVisible(should_control_be_visible)
            qreg_expected_output_state_input_field.setVisible(should_control_be_visible)
            qreg_actual_output_state_label.setVisible(should_control_be_visible)
            qreg_actual_output_state_widget.setVisible(should_control_be_visible)
            qreg_edit_qubit_values_toggle_button.setVisible(should_control_be_visible)

    @QtCore.pyqtSlot(QtCore.Qt.CheckState, str, int, int, bool)  # type: ignore[untyped-decorator]
    def _handle_input_state_qubit_value_checkbox_state_change(
        self,
        new_checkbox_state: QtCore.Qt.CheckState,
        associated_qreg_name: str,
        associated_qubit: int,
        relative_qubit_index_in_quantum_register: int,
        *,
        update_associated_state_input_field: bool = False,
    ) -> None:
        optional_associated_qubit_value_checkbox: QtWidgets.QCheckBox | None = (
            self._simulation_run_wrapper_box.findChild(
                QtWidgets.QCheckBox,
                INPUT_STATE_QUBIT_CHECKBOX_NAME_FORMAT.format(qubit=associated_qubit),
            )
        )

        optional_qreg_input_state_input_field: QtWidgets.QLineEdit | None = self._simulation_run_wrapper_box.findChild(
            QtWidgets.QLineEdit, QREG_INPUT_STATE_INPUT_FIELD_NAME_FORMAT.format(qreg_name=associated_qreg_name)
        )

        if not self._assert_all_required_widgets_found_or_close_dialog(
            [optional_associated_qubit_value_checkbox, optional_qreg_input_state_input_field],
            f"Failed to locate all required Qt widgets required to update value of qubit {relative_qubit_index_in_quantum_register} of quantum register '{associated_qreg_name}' in input state!",
        ):
            return

        associated_qubit_value_checkbox = cast("QtWidgets.QCheckBox", optional_associated_qubit_value_checkbox)
        qreg_input_state_input_field = cast("QtWidgets.QLineEdit", optional_qreg_input_state_input_field)

        updated_qubit_value: bool = new_checkbox_state == QtCore.Qt.CheckState.Checked
        stringified_updated_qubit_value: str = SimulationRunEditorDialog._stringify_qubit_value(
            updated_qubit_value, return_as_high_low_state=True
        )

        if not self.edited_simulation_run_model.update_input_state_qubit_value(
            associated_qubit, new_qubit_value=updated_qubit_value
        ):
            show_and_request_ok_in_optionally_cancellable_notification(
                message_box_type=MessageBoxType.ERROR,
                message_box_parent=self,
                message_box_title="Failed to updated qubit value",
                message_box_content=f"Failed to update value of qubit {relative_qubit_index_in_quantum_register} of quantum register {associated_qreg_name} in input state to new value{stringified_updated_qubit_value}!",
                is_cancellable=False,
                log_contents=True,
            )
            self.reject()
            return

        associated_qubit_value_checkbox.setText(
            STRINGIFIED_QUBIT_VALUE_FORMAT.format(stringified_qubit_value=stringified_updated_qubit_value)
        )

        if update_associated_state_input_field:
            curr_stringified_input_state: str = qreg_input_state_input_field.text()
            qreg_input_state_input_field.setText(
                curr_stringified_input_state[:relative_qubit_index_in_quantum_register]
                + SimulationRunEditorDialog._stringify_qubit_value(updated_qubit_value, return_as_high_low_state=False)
                + curr_stringified_input_state[relative_qubit_index_in_quantum_register + 1 :]
            )
        else:
            associated_qubit_value_checkbox.setCheckState(new_checkbox_state)

    @QtCore.pyqtSlot(QtCore.Qt.CheckState, str, int, int, bool)  # type: ignore[untyped-decorator]
    def _handle_expected_output_state_qubit_value_checkbox_state_change(
        self,
        new_checkbox_state: QtCore.Qt.CheckState,
        associated_qreg_name: str,
        associated_qubit: int,
        relative_qubit_index_in_quantum_register: int,
        *,
        update_associated_state_input_field: bool,
    ) -> None:
        optional_associated_qubit_value_checkbox: QtWidgets.QCheckBox | None = (
            self._simulation_run_wrapper_box.findChild(
                QtWidgets.QCheckBox,
                EXPECTED_OUTPUT_STATE_QUBIT_CHECKBOX_NAME_FORMAT.format(qubit=associated_qubit),
            )
        )

        optional_qreg_output_state_input_field: QtWidgets.QLineEdit | None = self._simulation_run_wrapper_box.findChild(
            QtWidgets.QLineEdit,
            EXPECTED_QREG_OUTPUT_STATE_INPUT_FIELD_NAME_FORMAT.format(qreg_name=associated_qreg_name),
        )

        if not self._assert_all_required_widgets_found_or_close_dialog(
            [optional_associated_qubit_value_checkbox, optional_qreg_output_state_input_field],
            f"Failed to locate all required Qt widgets required to update value of qubit {relative_qubit_index_in_quantum_register} of quantum register '{associated_qreg_name}' in output state!",
        ):
            return

        associated_qubit_value_checkbox = cast("QtWidgets.QCheckBox", optional_associated_qubit_value_checkbox)
        qreg_output_state_input_field = cast("QtWidgets.QLineEdit", optional_qreg_output_state_input_field)

        updated_qubit_value: bool = new_checkbox_state == QtCore.Qt.CheckState.Checked
        stringified_updated_qubit_value: str = SimulationRunEditorDialog._stringify_qubit_value(
            updated_qubit_value, return_as_high_low_state=True
        )

        if self.edited_simulation_run_model.expected_output_state is None:
            stringified_updated_qubit_value = SimulationRunEditorDialog._stringify_qubit_value(
                None, return_as_high_low_state=True
            )
            associated_qubit_value_checkbox.setText(
                STRINGIFIED_QUBIT_VALUE_FORMAT.format(stringified_qubit_value=stringified_updated_qubit_value)
            )
            return

        if not self.edited_simulation_run_model.update_expected_output_state_qubit_value(
            associated_qubit, new_qubit_value=updated_qubit_value
        ):
            show_and_request_ok_in_optionally_cancellable_notification(
                message_box_type=MessageBoxType.ERROR,
                message_box_parent=self,
                message_box_title="Failed to updated qubit value",
                message_box_content=f"Failed to update value of qubit {relative_qubit_index_in_quantum_register} of quantum register '{associated_qreg_name}' in output state to new value{stringified_updated_qubit_value}!",
                is_cancellable=False,
                log_contents=True,
            )
            self.reject()
            return

        associated_qubit_value_checkbox.setText(
            STRINGIFIED_QUBIT_VALUE_FORMAT.format(stringified_qubit_value=stringified_updated_qubit_value)
        )
        if update_associated_state_input_field:
            curr_stringified_output_state: str = qreg_output_state_input_field.text()
            qreg_output_state_input_field.setText(
                curr_stringified_output_state[:relative_qubit_index_in_quantum_register]
                + SimulationRunEditorDialog._stringify_qubit_value(updated_qubit_value, return_as_high_low_state=False)
                + curr_stringified_output_state[relative_qubit_index_in_quantum_register + 1 :]
            )
        else:
            associated_qubit_value_checkbox.setCheckState(new_checkbox_state)

    def _create_qreg_search_controls(self) -> QtWidgets.QLayout:
        qreg_search_controls_layout = QtWidgets.QHBoxLayout()
        qreg_search_label = QtWidgets.QLabel("Quantum register:")
        qreg_search_input_field = QtWidgets.QLineEdit(objectName=QREG_SEARCH_INPUT_FIELD_NAME)
        qreg_search_input_field.setPlaceholderText("<QUANTUM_REGISTER_NAME>")

        qreg_name_regular_expr = QtCore.QRegularExpression(R"(^([_A-Za-z]\w*)?$)")
        qreg_name_validator = QtGui.QRegularExpressionValidator(qreg_name_regular_expr, self)
        qreg_search_input_field.setValidator(qreg_name_validator)

        qreg_name_search_completer = QtWidgets.QCompleter([qreg_layout.qreg_name for qreg_layout in self._qreg_layouts])
        qreg_name_search_completer.setCaseSensitivity(QtCore.Qt.CaseSensitivity.CaseSensitive)
        qreg_search_input_field.setCompleter(qreg_name_search_completer)

        qreg_search_trigger_button = QtWidgets.QPushButton("Search", objectName=QREG_SEARCH_TRIGGER_BUTTON_NAME)
        qreg_search_trigger_button.clicked.connect(self._handle_quantum_register_name_search)

        qreg_search_controls_layout.addWidget(qreg_search_label)
        qreg_search_controls_layout.addWidget(qreg_search_input_field)
        qreg_search_controls_layout.addWidget(qreg_search_trigger_button)
        return qreg_search_controls_layout

    def _create_in_or_out_state_edit_field(
        self,
        qreg_layout: QuantumRegisterLayout,
        optional_qreg_qubit_values: NBitValuesContainer | None,
        qubit_location: QubitLocation,
    ) -> QRegContentsLabelAndCheckbox:

        unknown_qreg_contents_placeholder: Final[str] = "-"
        prefix_label: QtWidgets.QLabel | None = None
        match qubit_location:
            case QubitLocation.INPUT_STATE:
                prefix_label = None
            case QubitLocation.EXPECTED_OUTPUT_STATE:
                prefix_label = QtWidgets.QLabel(
                    "Expected:",
                    objectName=EXPECTED_QREG_OUTPUT_STATE_PREFIX_LABEL_NAME_FORMAT.format(
                        qreg_name=qreg_layout.qreg_name
                    ),
                )
            case QubitLocation.ACTUAL_OUTPUT_STATE:
                prefix_label = QtWidgets.QLabel(
                    "Actual:",
                    objectName=ACTUAL_QREG_OUTPUT_STATE_PREFIX_LABEL_NAME_FORMAT.format(
                        qreg_name=qreg_layout.qreg_name
                    ),
                )
                actual_contents_widget = QtWidgets.QLabel(
                    SimulationRunEditorDialog._stringify_some_qubits_of_n_bit_values_container(
                        optional_qreg_qubit_values, qreg_layout.first_qubit_of_qreg, qreg_layout.qreg_size
                    )
                    if optional_qreg_qubit_values is not None
                    else unknown_qreg_contents_placeholder,
                    objectName=ACTUAL_QREG_OUTPUT_STATE_INPUT_FIELD_NAME_FORMAT.format(qreg_name=qreg_layout.qreg_name),
                )
                return QRegContentsLabelAndCheckbox(prefix_label, actual_contents_widget)
            case _:
                # Added guard to fail on unhandled new qubit location enum values
                assert_never(qubit_location)

        is_control_created_for_input_state: Final[bool] = qubit_location == QubitLocation.INPUT_STATE
        in_or_out_state_edit_field = LineEditWithDynamicWidth(qreg_layout.qreg_size)
        in_or_out_state_edit_field.setObjectName(
            QREG_INPUT_STATE_INPUT_FIELD_NAME_FORMAT.format(qreg_name=qreg_layout.qreg_name)
            if is_control_created_for_input_state
            else EXPECTED_QREG_OUTPUT_STATE_INPUT_FIELD_NAME_FORMAT.format(qreg_name=qreg_layout.qreg_name)
        )

        if optional_qreg_qubit_values is not None:
            in_or_out_state_edit_field.setText(
                SimulationRunEditorDialog._stringify_some_qubits_of_n_bit_values_container(
                    optional_qreg_qubit_values, qreg_layout.first_qubit_of_qreg, qreg_layout.qreg_size
                )
            )

        in_or_out_state_edit_field.setEnabled(optional_qreg_qubit_values is not None)
        in_or_out_state_edit_field.setPlaceholderText(unknown_qreg_contents_placeholder)
        in_or_out_state_edit_field.setCursorPosition(0)
        in_or_out_state_edit_field.setAlignment(QtCore.Qt.AlignmentFlag.AlignJustify)
        in_or_out_state_edit_field.setValidator(
            QtGui.QRegularExpressionValidator(QtCore.QRegularExpression(R"^[0-1]*$"), self)
        )
        # The QLineEdit editingFinished signal is only triggered when its input satisfies the set inputmask/validator or when return/enter is pressed or if the QLineEdit loses focus.
        # However, during testing entering an invalid quantum register state in the input/output state validation seems to not trigger after entering an invalid value into an QLineEdit
        # and moving focus to another item. Only after entering a second invalid state the validation is triggered. Only when confirming the changes with enter/return is the validation triggered immediately.
        # One could in a custom overwrite of the QLineEdit class use the focusOutEvent to emit a custom signal when the QLineEdit element looses focus or use the application-level focusChanged signal to determine
        # whether the QLineEdit widget lost focus.
        in_or_out_state_edit_field.editingFinished.connect(
            lambda associated_qreg_name=qreg_layout.qreg_name, expected_text_length=qreg_layout.qreg_size, is_editing_input_state=is_control_created_for_input_state: (
                self._handle_input_or_output_state_text_change(
                    associated_qreg_name, expected_text_length, is_editing_input_state=is_editing_input_state
                )
            )
        )
        in_or_out_state_edit_field.focusOut.connect(
            lambda associated_qreg_name=qreg_layout.qreg_name, expected_text_length=qreg_layout.qreg_size, is_editing_input_state=is_control_created_for_input_state: (
                self._handle_input_or_output_state_text_change(
                    associated_qreg_name, expected_text_length, is_editing_input_state=is_editing_input_state
                )
            )
        )
        return QRegContentsLabelAndCheckbox(prefix_label, in_or_out_state_edit_field)

    def _create_in_or_out_state_qubit_value_checkbox(
        self,
        associated_qreg_name: str,
        optional_qreg_qubit_values: NBitValuesContainer | None,
        associated_qubit: int,
        relative_qubit_index_in_qreg: int,
        qubit_location: QubitLocation,
    ) -> QubitValueLabelAndCheckbox:

        qubit_value_checkbox_objectname: str = ""
        match qubit_location:
            case QubitLocation.INPUT_STATE:
                qubit_value_checkbox_objectname = INPUT_STATE_QUBIT_CHECKBOX_NAME_FORMAT.format(qubit=associated_qubit)
            case QubitLocation.EXPECTED_OUTPUT_STATE:
                qubit_value_checkbox_objectname = EXPECTED_OUTPUT_STATE_QUBIT_CHECKBOX_NAME_FORMAT.format(
                    qubit=associated_qubit
                )
            case QubitLocation.ACTUAL_OUTPUT_STATE:
                qubit_value_checkbox_objectname = ACTUAL_OUTPUT_STATE_QUBIT_CHECKBOX_NAME_FORMAT.format(
                    qubit=associated_qubit
                )
            case _:
                # Added guard to fail on unhandled new qubit location enum values
                assert_never(qubit_location)

        qubit_value_checkbox = QtWidgets.QCheckBox(objectName=qubit_value_checkbox_objectname)
        qubit_value_checkbox.setText(
            STRINGIFIED_QUBIT_VALUE_FORMAT.format(
                stringified_qubit_value=SimulationRunEditorDialog._stringify_qubit_value(
                    None if optional_qreg_qubit_values is None else optional_qreg_qubit_values.test(associated_qubit),
                    return_as_high_low_state=True,
                )
            )
        )
        if optional_qreg_qubit_values is not None:
            qubit_value_checkbox.setChecked(optional_qreg_qubit_values.test(associated_qubit))

        qubit_value_checkbox.setEnabled(
            optional_qreg_qubit_values is not None and qubit_location != QubitLocation.ACTUAL_OUTPUT_STATE
        )
        qubit_value_label: QtWidgets.QLabel | None = None
        match qubit_location:
            case QubitLocation.INPUT_STATE:
                qubit_value_checkbox.checkStateChanged.connect(
                    lambda new_check_state, associated_qreg_name=associated_qreg_name, associated_qubit=associated_qubit, relative_qubit_index_in_quantum_register=relative_qubit_index_in_qreg: (
                        self._handle_input_state_qubit_value_checkbox_state_change(
                            new_check_state,
                            associated_qreg_name,
                            associated_qubit,
                            relative_qubit_index_in_quantum_register,
                            update_associated_state_input_field=True,
                        )
                    )
                )
            case QubitLocation.EXPECTED_OUTPUT_STATE:
                qubit_value_checkbox.checkStateChanged.connect(
                    lambda new_check_state, associated_qreg_name=associated_qreg_name, associated_qubit=associated_qubit, relative_qubit_index_in_quantum_register=relative_qubit_index_in_qreg: (
                        self._handle_expected_output_state_qubit_value_checkbox_state_change(
                            new_check_state,
                            associated_qreg_name,
                            associated_qubit,
                            relative_qubit_index_in_quantum_register,
                            update_associated_state_input_field=True,
                        )
                    )
                )
                qubit_value_label = QtWidgets.QLabel(
                    "Expected:",
                    objectName=EXPECTED_OUTPUT_STATE_QUBIT_CHECKBOX_LABEL_NAME_FORMAT.format(qubit=associated_qubit),
                )
            case QubitLocation.ACTUAL_OUTPUT_STATE:
                qubit_value_label = QtWidgets.QLabel(
                    "Actual:",
                    objectName=ACTUAL_OUTPUT_STATE_QUBIT_CHECKBOX_LABEL_NAME_FORMAT.format(qubit=associated_qubit),
                )
        return QubitValueLabelAndCheckbox(qubit_value_label, qubit_value_checkbox)

    def _create_search_controls_for_qubits_of_qreg(
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
            SimulationRunEditorDialog._get_internal_qubit_labels_for_qreg(
                self._annotatable_quantum_computation, first_qreg_qubit, last_qreg_qubit - first_qreg_qubit
            )
        )
        qubit_search_completer.setCaseSensitivity(QtCore.Qt.CaseSensitivity.CaseSensitive)
        qubit_search_input_field.setCompleter(qubit_search_completer)

        qubit_search_layout.addWidget(qubit_search_input_field)

        qubit_search_trigger_button = QtWidgets.QPushButton("Search")
        qubit_search_trigger_button.clicked.connect(
            lambda _, associated_qreg_name=associated_qreg_name: self._handle_qubit_search_trigger_button_click(
                associated_qreg_name
            )
        )
        qubit_search_layout.addWidget(qubit_search_trigger_button)
        return qubit_search_layout

    def _create_qubit_controls_groupbox(
        self,
        associated_qreg_layout: QuantumRegisterLayout,
        initial_input_state: NBitValuesContainer,
        initial_expected_output_state: NBitValuesContainer | None,
        initial_actual_output_state: NBitValuesContainer | None,
    ) -> QtWidgets.QWidget:
        input_output_qubits_value_controls_groupbox_layout = QtWidgets.QGridLayout()
        # The inability to use named parameters for the addWidget(...) or addLayout(...) calls makes the code a bit more harder read (https://forum.qt.io/topic/160589/pyside6-unsupported-keyword-on-grid-layout/2)
        # when used in combination with a QGridLayout.
        input_output_qubits_value_controls_groupbox_layout.addLayout(
            self._create_search_controls_for_qubits_of_qreg(associated_qreg_layout),
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
            fetched_internal_qubit_label: str | None = self._annotatable_quantum_computation.get_qubit_label(
                qubit, QubitLabelType.internal
            )
            qubit_label = QtWidgets.QLabel(
                "Qubit: " + fetched_internal_qubit_label if fetched_internal_qubit_label is not None else "<UNKNOWN>",
                objectName=QUBIT_LABEL_NAME_FORMAT.format(qubit=qubit),
            )

            qubit_controls_grid_layout_row: int = 1 + (2 * relative_qubit_index_in_qreg)
            input_output_qubits_value_controls_groupbox_layout.addWidget(
                qubit_label, qubit_controls_grid_layout_row, 0, 2, 1
            )

            input_state_qubit_value_checkbox_and_lbl: QubitValueLabelAndCheckbox = (
                self._create_in_or_out_state_qubit_value_checkbox(
                    associated_qreg_layout.qreg_name,
                    initial_input_state,
                    associated_qubit=qubit,
                    relative_qubit_index_in_qreg=relative_qubit_index_in_qreg,
                    qubit_location=QubitLocation.INPUT_STATE,
                )
            )
            input_output_qubits_value_controls_groupbox_layout.addWidget(
                input_state_qubit_value_checkbox_and_lbl.checkbox, qubit_controls_grid_layout_row, 1, 2, 1
            )

            expected_output_state_qubit_value_checkbox_and_lbl: QubitValueLabelAndCheckbox = (
                self._create_in_or_out_state_qubit_value_checkbox(
                    associated_qreg_layout.qreg_name,
                    initial_expected_output_state,
                    associated_qubit=qubit,
                    relative_qubit_index_in_qreg=relative_qubit_index_in_qreg,
                    qubit_location=QubitLocation.EXPECTED_OUTPUT_STATE,
                )
            )

            output_qubits_controls_layout = QtWidgets.QGridLayout()
            if expected_output_state_qubit_value_checkbox_and_lbl.optional_label is None:
                log_error_to_console(
                    f"Failed to create label for expected output state qubit {relative_qubit_index_in_qreg}",
                    num_additionally_skipped_stack_frames_starting_from_caller_function=1,
                )
            else:
                output_qubits_controls_layout.addWidget(
                    expected_output_state_qubit_value_checkbox_and_lbl.optional_label, 0, 0
                )
            output_qubits_controls_layout.addWidget(expected_output_state_qubit_value_checkbox_and_lbl.checkbox, 0, 1)

            actual_output_state_qubit_value_checkbox_and_lbl: QubitValueLabelAndCheckbox = (
                self._create_in_or_out_state_qubit_value_checkbox(
                    associated_qreg_layout.qreg_name,
                    initial_actual_output_state,
                    associated_qubit=qubit,
                    relative_qubit_index_in_qreg=relative_qubit_index_in_qreg,
                    qubit_location=QubitLocation.ACTUAL_OUTPUT_STATE,
                )
            )

            if actual_output_state_qubit_value_checkbox_and_lbl.optional_label is None:
                log_error_to_console(
                    f"Failed to create label for actual output state qubit {relative_qubit_index_in_qreg}",
                    num_additionally_skipped_stack_frames_starting_from_caller_function=1,
                )
            else:
                output_qubits_controls_layout.addWidget(
                    actual_output_state_qubit_value_checkbox_and_lbl.optional_label, 1, 0
                )
            output_qubits_controls_layout.addWidget(actual_output_state_qubit_value_checkbox_and_lbl.checkbox, 1, 1)

            output_qubits_controls_layout.addItem(
                QtWidgets.QSpacerItem(
                    2, 2, QtWidgets.QSizePolicy.Policy.Minimum, QtWidgets.QSizePolicy.Policy.Expanding
                ),
                qubit_controls_grid_layout_row,
                2,
                1,
                1,
            )

            output_qubits_controls_layout.setColumnStretch(0, 0)
            output_qubits_controls_layout.setColumnStretch(1, 1)
            output_qubits_controls_layout.setColumnStretch(2, 1)
            input_output_qubits_value_controls_groupbox_layout.addLayout(
                output_qubits_controls_layout, qubit_controls_grid_layout_row, 2, 2, 1
            )

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

    @QtCore.pyqtSlot(str)  # type: ignore[untyped-decorator]
    def _handle_qubit_search_trigger_button_click(self, associated_quantum_register_name: str) -> None:
        associated_qreg_layout: Final[QuantumRegisterLayout | None] = next(
            filter(lambda qreg_layout: qreg_layout.qreg_name == associated_quantum_register_name, self._qreg_layouts),
            None,
        )
        if associated_qreg_layout is None:
            return

        optional_qreg_qubits_groupbox: QtWidgets.QtWidget | None = self._simulation_run_wrapper_box.findChild(
            QtWidgets.QGroupBox,
            QREG_QUBIT_VALUES_GROUPBOX_NAME_FORMAT.format(qreg_name=associated_quantum_register_name),
            QtCore.Qt.FindChildOption.FindDirectChildrenOnly,
        )

        if not self._assert_all_required_widgets_found_or_close_dialog(
            [optional_qreg_qubits_groupbox],
            f"Failed to find required qubits groupbox for quantum register '{associated_quantum_register_name}' during handling of qubit label search!",
        ):
            return

        qreg_qubits_groupbox = cast("QtWidgets.QGroupBox", optional_qreg_qubits_groupbox)
        optional_qubit_search_input_field: QtWidgets.QtWidget | None = qreg_qubits_groupbox.findChild(
            QtWidgets.QLineEdit,
            QREG_QUBIT_SEARCH_INPUT_FIELD_NAME_FORMAT.format(qreg_name=associated_quantum_register_name),
            QtCore.Qt.FindChildOption.FindDirectChildrenOnly,
        )

        if not self._assert_all_required_widgets_found_or_close_dialog(
            [optional_qubit_search_input_field],
            f"Failed to find required qubit label search input field for quantum register '{associated_quantum_register_name}' during handling of qubit label search!",
        ):
            return

        qubit_search_input_field = cast("QtWidgets.QLineEdit", optional_qubit_search_input_field)
        for qubit in range(
            associated_qreg_layout.first_qubit_of_qreg,
            associated_qreg_layout.first_qubit_of_qreg + associated_qreg_layout.qreg_size,
        ):
            optional_qubit_value_label: QtWidgets.QtWidget | None = qreg_qubits_groupbox.findChild(
                QtWidgets.QLabel,
                QUBIT_LABEL_NAME_FORMAT.format(qubit=qubit),
                QtCore.Qt.FindChildOption.FindDirectChildrenOnly,
            )
            optional_input_state_qubit_checkbox: QtWidgets.QCheckBox | None = qreg_qubits_groupbox.findChild(
                QtWidgets.QCheckBox,
                INPUT_STATE_QUBIT_CHECKBOX_NAME_FORMAT.format(qubit=qubit),
                QtCore.Qt.FindChildOption.FindDirectChildrenOnly,
            )
            optional_expected_output_state_qubit_checkbox_label: QtWidgets.QLabel | None = (
                qreg_qubits_groupbox.findChild(
                    QtWidgets.QLabel, EXPECTED_OUTPUT_STATE_QUBIT_CHECKBOX_LABEL_NAME_FORMAT.format(qubit=qubit)
                )
            )
            optional_expected_output_state_qubit_checkbox: QtWidgets.QCheckBox | None = qreg_qubits_groupbox.findChild(
                QtWidgets.QCheckBox, EXPECTED_OUTPUT_STATE_QUBIT_CHECKBOX_NAME_FORMAT.format(qubit=qubit)
            )
            optional_actual_output_state_qubit_checkbox_label: QtWidgets.QLabel | None = qreg_qubits_groupbox.findChild(
                QtWidgets.QLabel, ACTUAL_OUTPUT_STATE_QUBIT_CHECKBOX_LABEL_NAME_FORMAT.format(qubit=qubit)
            )
            optional_actual_output_state_qubit_checkbox: QtWidgets.QCheckBox | None = qreg_qubits_groupbox.findChild(
                QtWidgets.QCheckBox, ACTUAL_OUTPUT_STATE_QUBIT_CHECKBOX_NAME_FORMAT.format(qubit=qubit)
            )
            if not self._assert_all_required_widgets_found_or_close_dialog(
                [
                    optional_qubit_value_label,
                    optional_input_state_qubit_checkbox,
                    optional_expected_output_state_qubit_checkbox_label,
                    optional_expected_output_state_qubit_checkbox,
                    optional_actual_output_state_qubit_checkbox_label,
                    optional_actual_output_state_qubit_checkbox,
                ],
                f"Failed to find required controls for qubits for quantum register '{associated_quantum_register_name}' during handling of qubit label search!",
            ):
                return

            qubit_value_label = cast("QtWidgets.QLabel", optional_qubit_value_label)
            input_state_qubit_checkbox = cast("QtWidgets.QCheckBox", optional_input_state_qubit_checkbox)
            expected_output_state_qubit_checkbox_label = cast(
                "QtWidgets.QLabel", optional_expected_output_state_qubit_checkbox_label
            )
            expected_output_state_qubit_checkbox = cast(
                "QtWidgets.QCheckBox", optional_expected_output_state_qubit_checkbox
            )
            actual_output_state_qubit_checkbox_label = cast(
                "QtWidgets.QLabel", optional_actual_output_state_qubit_checkbox_label
            )
            actual_output_state_qubit_checkbox = cast(
                "QtWidgets.QCheckBox", optional_actual_output_state_qubit_checkbox
            )

            matched_with_qubit_label: str | None = self._annotatable_quantum_computation.get_qubit_label(
                qubit, QubitLabelType.internal
            )
            does_qubit_label_match_search_text: bool = (
                matched_with_qubit_label.startswith(qubit_search_input_field.text())
                if matched_with_qubit_label is not None
                else False
            )
            qubit_value_label.setVisible(does_qubit_label_match_search_text)
            input_state_qubit_checkbox.setVisible(does_qubit_label_match_search_text)
            expected_output_state_qubit_checkbox_label.setVisible(does_qubit_label_match_search_text)
            expected_output_state_qubit_checkbox.setVisible(does_qubit_label_match_search_text)
            actual_output_state_qubit_checkbox_label.setVisible(does_qubit_label_match_search_text)
            actual_output_state_qubit_checkbox.setVisible(does_qubit_label_match_search_text)

    @QtCore.pyqtSlot(str)  # type: ignore[untyped-decorator]
    def _handle_qreg_qubit_values_edit_toggle_button_click(self, associated_qreg_name: str) -> None:
        is_any_qubit_values_groupbox_collapsed: bool = False
        optional_expected_output_state_value_toggle_button: QtWidgets.QtQWidget | None = (
            self._simulation_run_wrapper_box.findChild(
                QtWidgets.QPushButton,
                QREG_EXPECTED_OUTPUT_STATE_VALUE_INIT_TOGGLE_NAME,
                QtCore.Qt.FindChildOption.FindDirectChildrenOnly,
            )
        )

        optional_qreg_search_input_field: QtWidgets.QtWidget | None = self._simulation_run_wrapper_box.findChild(
            QtWidgets.QLineEdit, QREG_SEARCH_INPUT_FIELD_NAME
        )

        optional_qreg_search_trigger_btn: QtWidgets.QtWidget | None = self._simulation_run_wrapper_box.findChild(
            QtWidgets.QPushButton, QREG_SEARCH_TRIGGER_BUTTON_NAME
        )

        if not self._assert_all_required_widgets_found_or_close_dialog(
            [
                optional_expected_output_state_value_toggle_button,
                optional_qreg_search_input_field,
                optional_qreg_search_trigger_btn,
            ],
            f"Failed to find expected output state init/reset button during handling of edit qubit values of output state toggle button of quantum register {associated_qreg_name}!",
        ):
            return

        for qreg_layout in self._qreg_layouts:
            qreg_name: str = qreg_layout.qreg_name
            optional_qreg_input_state_input_field: QtWidgets.QtWidget | None = (
                self._simulation_run_wrapper_box.findChild(
                    QtWidgets.QLineEdit,
                    QREG_INPUT_STATE_INPUT_FIELD_NAME_FORMAT.format(qreg_name=qreg_name),
                    QtCore.Qt.FindChildOption.FindDirectChildrenOnly,
                )
            )
            optional_qreg_expected_output_state_input_field: QtWidgets.QtWidget | None = (
                self._simulation_run_wrapper_box.findChild(
                    QtWidgets.QLineEdit, EXPECTED_QREG_OUTPUT_STATE_INPUT_FIELD_NAME_FORMAT.format(qreg_name=qreg_name)
                )
            )
            optional_qubit_values_groupbox: QtWidgets.QtWidget | None = self._simulation_run_wrapper_box.findChild(
                QtWidgets.QGroupBox,
                QREG_QUBIT_VALUES_GROUPBOX_NAME_FORMAT.format(qreg_name=qreg_name),
                QtCore.Qt.FindChildOption.FindDirectChildrenOnly,
            )
            optional_qubit_values_toggle_button: QtWidgets.QtWidget | None = self._simulation_run_wrapper_box.findChild(
                QtWidgets.QPushButton,
                QREG_QUBIT_VALUES_TOGGLE_BUTTON_NAME_FORMAT.format(qreg_name=qreg_name),
                QtCore.Qt.FindChildOption.FindDirectChildrenOnly,
            )
            optional_qubit_values_groupbox_qubit_search_field: QtWidgets.QtWidget | None = (
                optional_qubit_values_groupbox.findChild(
                    QtWidgets.QLineEdit,
                    QREG_QUBIT_SEARCH_INPUT_FIELD_NAME_FORMAT.format(qreg_name=qreg_name),
                    QtCore.Qt.FindChildOption.FindDirectChildrenOnly,
                )
                if optional_qubit_values_groupbox is not None
                else None
            )

            if not self._assert_all_required_widgets_found_or_close_dialog(
                [
                    optional_qreg_input_state_input_field,
                    optional_qreg_expected_output_state_input_field,
                    optional_qubit_values_groupbox,
                    optional_qubit_values_toggle_button,
                    optional_qubit_values_groupbox_qubit_search_field,
                ],
                f"Failed to find all required QtWidgets for quantum register '{qreg_name}' during handling of edit toggle of output state!",
            ):
                return

            qreg_input_state_input_field = cast("QtWidgets.QLineEdit", optional_qreg_input_state_input_field)
            qreg_expected_output_state_input_field = cast(
                "QtWidgets.QLineEdit", optional_qreg_expected_output_state_input_field
            )
            qubit_values_groupbox = cast("QtWidgets.QCheckBox", optional_qubit_values_groupbox)
            qubit_values_toggle_button = cast("QtWidgets.QPushButton", optional_qubit_values_toggle_button)

            qubit_values_groupbox_qubit_search_field = cast(
                "QtWidgets.QLineEdit", optional_qubit_values_groupbox_qubit_search_field
            )
            if qreg_name == associated_qreg_name and not qubit_values_groupbox.isVisible():
                is_any_qubit_values_groupbox_collapsed = True
                qubit_values_groupbox.setVisible(True)
                qubit_values_toggle_button.setText(TOGGLE_OUTPUT_STATE_QUBIT_VALUES_EDIT)
                qreg_input_state_input_field.setEnabled(False)
                qreg_expected_output_state_input_field.setEnabled(False)
            else:
                qubit_values_groupbox.setVisible(False)
                qubit_values_toggle_button.setText(EDIT_OUTPUT_STATE_QUBIT_VALUES)
                qreg_input_state_input_field.setEnabled(True)
                qreg_expected_output_state_input_field.setEnabled(qreg_expected_output_state_input_field.text() != "")  # noqa: PLC1901
                qubit_values_groupbox_qubit_search_field.setText("")
                self._handle_qubit_search_trigger_button_click(associated_qreg_name)

        expected_output_state_value_toggle_button = cast(
            "QtWidgets.QPushButton", optional_expected_output_state_value_toggle_button
        )
        qreg_search_input_field = cast("QtWidgets.QLineEdit", optional_qreg_search_input_field)
        qreg_search_trigger_btn = cast("QtWidgets.QPushButtonLineEdit", optional_qreg_search_trigger_btn)

        expected_output_state_value_toggle_button.setEnabled(not is_any_qubit_values_groupbox_collapsed)
        qreg_search_input_field.setEnabled(not is_any_qubit_values_groupbox_collapsed)
        qreg_search_trigger_btn.setEnabled(not is_any_qubit_values_groupbox_collapsed)

    @QtCore.pyqtSlot()  # type: ignore[untyped-decorator]
    def _handle_init_expected_output_state_button_click(self) -> None:
        optional_expected_output_state_value_toggle_button: QtWidgets.QtQWidget | None = (
            self._simulation_run_wrapper_box.findChild(
                QtWidgets.QPushButton,
                QREG_EXPECTED_OUTPUT_STATE_VALUE_INIT_TOGGLE_NAME,
                QtCore.Qt.FindChildOption.FindDirectChildrenOnly,
            )
        )

        if not self._assert_all_required_widgets_found_or_close_dialog(
            [optional_expected_output_state_value_toggle_button],
            "Failed to find all required QtWidgets for init/clear operation of expected output state!",
        ):
            return

        expected_output_state_value_toggle_button = cast(
            "QtWidgets.QPushButton", optional_expected_output_state_value_toggle_button
        )

        should_reset_output_state: bool = self.edited_simulation_run_model.expected_output_state is not None
        if should_reset_output_state:
            self.edited_simulation_run_model.expected_output_state = None
            expected_output_state_value_toggle_button.setText("Init output state")
        else:
            self.edited_simulation_run_model.initialize_expected_output_state_as_copy_of_input_state()
            expected_output_state_value_toggle_button.setText("Clear output state")

        for qreg_layout in self._qreg_layouts:
            qreg_name: str = qreg_layout.qreg_name
            optional_qreg_input_state_input_field: QtWidgets.QtWidget | None = (
                self._simulation_run_wrapper_box.findChild(
                    QtWidgets.QLineEdit, QREG_INPUT_STATE_INPUT_FIELD_NAME_FORMAT.format(qreg_name=qreg_name)
                )
            )

            optional_qreg_output_state_input_field: QtWidgets.QtWidget | None = (
                self._simulation_run_wrapper_box.findChild(
                    QtWidgets.QLineEdit, EXPECTED_QREG_OUTPUT_STATE_INPUT_FIELD_NAME_FORMAT.format(qreg_name=qreg_name)
                )
            )

            if not self._assert_all_required_widgets_found_or_close_dialog(
                [optional_qreg_input_state_input_field, optional_qreg_output_state_input_field],
                f"Failed to find all required QtWidgets for quantum register '{qreg_name}' during handling of initialization/clearing of output state!",
            ):
                return

            qreg_input_state_input_field = cast("QtWidgets.QLineEdit", optional_qreg_input_state_input_field)
            qreg_output_state_input_field = cast("QtWidgets.QLineEdit", optional_qreg_output_state_input_field)

            if should_reset_output_state:
                qreg_output_state_input_field.setText("")
                qreg_output_state_input_field.setEnabled(False)
            else:
                # The value of the should_reset_output_state flag is dependent on the value of the expected output state member variable of the associated simulation run
                # and since no reset is requested, i.e. we initialized the expected output state member variable which in turn means it is not None at this point.
                qreg_output_state_input_field.setText(
                    SimulationRunEditorDialog._stringify_some_qubits_of_n_bit_values_container(
                        self.edited_simulation_run_model.expected_output_state,  # type: ignore[arg-type]
                        qreg_layout.first_qubit_of_qreg,
                        qreg_layout.qreg_size,
                    )
                )
                qreg_output_state_input_field.setEnabled(qreg_input_state_input_field.isEnabled())

            for qubit in range(
                qreg_layout.first_qubit_of_qreg, qreg_layout.first_qubit_of_qreg + qreg_layout.qreg_size
            ):
                optional_associated_qubit_value_checkbox: QtWidgets.QCheckBox | None = (
                    self._simulation_run_wrapper_box.findChild(
                        QtWidgets.QCheckBox, EXPECTED_OUTPUT_STATE_QUBIT_CHECKBOX_NAME_FORMAT.format(qubit=qubit)
                    )
                )

                if not self._assert_all_required_widgets_found_or_close_dialog(
                    [optional_associated_qubit_value_checkbox],
                    f"Failed to find all required QtWidget for qubit of output state of quantum register '{qreg_name}' during initialization/clearing of output state!",
                ):
                    return

                associated_qubit_value_checkbox = cast("QtWidgets.QCheckBox", optional_associated_qubit_value_checkbox)
                qubit_value: bool | None = (
                    self.edited_simulation_run_model.expected_output_state.test(qubit)  # type: ignore[union-attr]
                    if not should_reset_output_state
                    else None
                )
                associated_qubit_value_checkbox.setChecked(qubit_value if qubit_value is not None else False)
                associated_qubit_value_checkbox.setText(
                    STRINGIFIED_QUBIT_VALUE_FORMAT.format(
                        stringified_qubit_value=SimulationRunEditorDialog._stringify_qubit_value(
                            qubit_value, return_as_high_low_state=True
                        )
                    )
                )
                associated_qubit_value_checkbox.setEnabled(not should_reset_output_state)

    @QtCore.pyqtSlot(str, int, bool)  # type: ignore[untyped-decorator]
    def _handle_input_or_output_state_text_change(
        self, associated_qreg_name: str, expected_qreg_size: int, *, is_editing_input_state: bool
    ) -> None:
        optional_input_state_text_field: QtWidgets.QLineEdit | None = self._simulation_run_wrapper_box.findChild(
            QtWidgets.QLineEdit,
            QREG_INPUT_STATE_INPUT_FIELD_NAME_FORMAT.format(qreg_name=associated_qreg_name),
            QtCore.Qt.FindChildOption.FindDirectChildrenOnly,
        )

        optional_output_state_text_field: QtWidgets.QLineEdit | None = self._simulation_run_wrapper_box.findChild(
            QtWidgets.QLineEdit,
            EXPECTED_QREG_OUTPUT_STATE_INPUT_FIELD_NAME_FORMAT.format(qreg_name=associated_qreg_name),
            QtCore.Qt.FindChildOption.FindDirectChildrenOnly,
        )

        optional_qreg_qubit_values_edit_toggle_button: QtWidgets.QPushButton | None = (
            self._simulation_run_wrapper_box.findChild(
                QtWidgets.QPushButton,
                QREG_QUBIT_VALUES_TOGGLE_BUTTON_NAME_FORMAT.format(qreg_name=associated_qreg_name),
                QtCore.Qt.FindChildOption.FindDirectChildrenOnly,
            )
        )

        optional_expected_output_state_init_button: QtWidgets.QPushButton | None = (
            self._simulation_run_wrapper_box.findChild(
                QtWidgets.QPushButton,
                QREG_EXPECTED_OUTPUT_STATE_VALUE_INIT_TOGGLE_NAME,
                QtCore.Qt.FindChildOption.FindDirectChildrenOnly,
            )
        )

        optional_dialog_save_button: QtWidgets.QPushButton | None = self._dialog_button_box.button(
            QtWidgets.QDialogButtonBox.StandardButton.Save
        )
        optional_qreg_values_validation_error_lbl: QtWidgets.QLabel | None = self.findChild(
            QtWidgets.QLabel, QREG_VALUES_VALIDATION_ERROR_LABEL_NAME
        )

        if not self._assert_all_required_widgets_found_or_close_dialog(
            [
                optional_input_state_text_field,
                optional_output_state_text_field,
                optional_qreg_qubit_values_edit_toggle_button,
                optional_expected_output_state_init_button,
                optional_dialog_save_button,
                optional_qreg_values_validation_error_lbl,
            ],
            f"Failed to find all required QtWidgets for edited quantum register '{associated_qreg_name}' during handling of input/output state edit!",
        ):
            return

        input_state_text_field = cast("QtWidgets.QLineEdit", optional_input_state_text_field)
        output_state_text_field = cast("QtWidgets.QLineEdit", optional_output_state_text_field)
        qreg_qubit_values_edit_toggle_button = cast(
            "QtWidgets.QPushButton", optional_qreg_qubit_values_edit_toggle_button
        )
        expected_output_state_init_button = cast("QtWidgets.QPushButton", optional_expected_output_state_init_button)
        dialog_save_button = cast("QtWidgets.QPushButton", optional_dialog_save_button)
        qreg_values_validation_error_lbl = cast("QtWidgets.QLabel", optional_qreg_values_validation_error_lbl)

        curr_edited_input_text_field: QtWidgets.QLineEdit = (
            input_state_text_field if is_editing_input_state else output_state_text_field
        )
        curr_not_edited_input_text_field: QtWidgets.QLineEdit = (
            output_state_text_field if is_editing_input_state else input_state_text_field
        )
        are_stringified_qreg_contents_valid: Final[bool] = (
            curr_edited_input_text_field.hasAcceptableInput()
            and len(curr_edited_input_text_field.text()) == expected_qreg_size
        )

        qreg_qubit_values_edit_toggle_button.setEnabled(are_stringified_qreg_contents_valid)
        expected_output_state_init_button.setEnabled(are_stringified_qreg_contents_valid)
        dialog_save_button.setEnabled(are_stringified_qreg_contents_valid)

        qreg_values_validation_error_lbl.setText(
            QREG_VALUES_VALIDATION_ERROR_FORMAT.format(
                qreg_name=associated_qreg_name,
                expected_num_qubit_values=expected_qreg_size,
                actual_num_qubit_values=len(curr_edited_input_text_field.text()),
                input_or_output_state_ident="input" if is_editing_input_state else "output",
            )
            if not are_stringified_qreg_contents_valid
            else ""
        )
        curr_not_edited_input_text_field.setEnabled(
            are_stringified_qreg_contents_valid
            and (self.edited_simulation_run_model.expected_output_state is not None or not is_editing_input_state)
        )

        if are_stringified_qreg_contents_valid:
            edited_qreg_layout: QuantumRegisterLayout | None = next(
                filter(lambda qreg_layout: qreg_layout.qreg_name == associated_qreg_name, self._qreg_layouts), None
            )
            if edited_qreg_layout is None:
                self._failed_due_to_internal_error = True
                show_and_request_ok_in_optionally_cancellable_notification(
                    message_box_type=MessageBoxType.ERROR,
                    message_box_parent=self,
                    message_box_title="Could not find layout of quantum register!",
                    message_box_content=f"Failed to find layout of edited quantum register '{associated_qreg_name}'.\nUnsaved changed will be lost and edit dialog will be closed!",
                    is_cancellable=False,
                    log_contents=True,
                )
                self.reject()
                return

            optional_effected_qreg_qubit_values_groupbox: QtWidgets.QGroupBox | None = (
                self._simulation_run_wrapper_box.findChild(
                    QtWidgets.QGroupBox,
                    QREG_QUBIT_VALUES_GROUPBOX_NAME_FORMAT.format(qreg_name=associated_qreg_name),
                    QtCore.Qt.FindChildOption.FindDirectChildrenOnly,
                )
            )

            if not self._assert_all_required_widgets_found_or_close_dialog(
                [optional_effected_qreg_qubit_values_groupbox],
                f"Failed to find required group box for edited quantum register '{associated_qreg_name}' QtWidgets during handling of input/output state edit!",
            ):
                return

            cast("QtWidgets.QGroupBox", optional_effected_qreg_qubit_values_groupbox)

            first_qubit_of_edited_qreg: Final[int] = edited_qreg_layout.first_qubit_of_qreg
            n_qubits_of_edited_qreg: Final[int] = edited_qreg_layout.qreg_size
            for qubit_of_edited_qreg in range(
                first_qubit_of_edited_qreg, first_qubit_of_edited_qreg + n_qubits_of_edited_qreg
            ):
                relative_qubit_idx_in_qreg: int = qubit_of_edited_qreg - first_qubit_of_edited_qreg
                new_checkbox_state: QtCore.Qt.CheckState = (
                    QtCore.Qt.CheckState.Checked
                    if curr_edited_input_text_field.text()[relative_qubit_idx_in_qreg] == "1"
                    else QtCore.Qt.CheckState.Unchecked
                )
                if is_editing_input_state:
                    self._handle_input_state_qubit_value_checkbox_state_change(
                        new_checkbox_state,
                        associated_qreg_name,
                        qubit_of_edited_qreg,
                        relative_qubit_idx_in_qreg,
                        update_associated_state_input_field=False,
                    )
                else:
                    self._handle_expected_output_state_qubit_value_checkbox_state_change(
                        new_checkbox_state,
                        associated_qreg_name,
                        qubit_of_edited_qreg,
                        relative_qubit_idx_in_qreg,
                        update_associated_state_input_field=False,
                    )

        for qreg_layout in self._qreg_layouts:
            qreg_name: str = qreg_layout.qreg_name
            optional_qreg_qubit_values_groupbox: QtWidgets.QGroupBox | None = (
                self._simulation_run_wrapper_box.findChild(
                    QtWidgets.QGroupBox,
                    QREG_QUBIT_VALUES_GROUPBOX_NAME_FORMAT.format(qreg_name=qreg_name),
                    QtCore.Qt.FindChildOption.FindDirectChildrenOnly,
                )
            )

            if not self._assert_all_required_widgets_found_or_close_dialog(
                [optional_qreg_qubit_values_groupbox],
                f"Failed to find required group box for not edited quantum register '{qreg_name}' QtWidgets during handling of input/output state edit!",
            ):
                return

            qreg_qubit_values_groupbox = cast("QtWidgets.QGroupBox", optional_qreg_qubit_values_groupbox)

            if qreg_name != associated_qreg_name:
                optional_not_edited_input_state_text_field: QtWidgets.QLineEdit | None = (
                    self._simulation_run_wrapper_box.findChild(
                        QtWidgets.QLineEdit,
                        QREG_INPUT_STATE_INPUT_FIELD_NAME_FORMAT.format(qreg_name=qreg_name),
                        QtCore.Qt.FindChildOption.FindDirectChildrenOnly,
                    )
                )

                optional_not_edited_output_state_text_field: QtWidgets.QLineEdit | None = (
                    self._simulation_run_wrapper_box.findChild(
                        QtWidgets.QLineEdit,
                        EXPECTED_QREG_OUTPUT_STATE_INPUT_FIELD_NAME_FORMAT.format(qreg_name=qreg_name),
                        QtCore.Qt.FindChildOption.FindDirectChildrenOnly,
                    )
                )

                optional_not_edited_qreg_qubit_values_edit_toggle_button: QtWidgets.QPushButton | None = (
                    self._simulation_run_wrapper_box.findChild(
                        QtWidgets.QPushButton,
                        QREG_QUBIT_VALUES_TOGGLE_BUTTON_NAME_FORMAT.format(qreg_name=qreg_name),
                        QtCore.Qt.FindChildOption.FindDirectChildrenOnly,
                    )
                )

                if not self._assert_all_required_widgets_found_or_close_dialog(
                    [
                        optional_not_edited_input_state_text_field,
                        optional_not_edited_output_state_text_field,
                        optional_not_edited_qreg_qubit_values_edit_toggle_button,
                    ],
                    f"Failed to find all required QtWidgets for not edited quantum register '{qreg_name}' during handling of input/output state edit!",
                ):
                    return

                not_edited_input_state_text_field = cast(
                    "QtWidgets.QLineEdit", optional_not_edited_input_state_text_field
                )
                not_edited_output_state_text_field = cast(
                    "QtWidgets.QLineEdit", optional_not_edited_output_state_text_field
                )
                not_edited_qreg_qubit_values_edit_toggle_button = cast(
                    "QtWidgets.QPushButton", optional_not_edited_qreg_qubit_values_edit_toggle_button
                )

                should_state_controls_be_visible: bool = (
                    are_stringified_qreg_contents_valid and not qreg_qubit_values_groupbox.isVisible()
                )
                not_edited_input_state_text_field.setEnabled(should_state_controls_be_visible)
                not_edited_output_state_text_field.setEnabled(
                    should_state_controls_be_visible
                    and self.edited_simulation_run_model.expected_output_state is not None
                )
                not_edited_qreg_qubit_values_edit_toggle_button.setEnabled(should_state_controls_be_visible)

            if not qreg_qubit_values_groupbox.isVisible():
                continue

            first_qubit_of_qreg: int = qreg_layout.first_qubit_of_qreg
            n_qubits_of_qreg: int = qreg_layout.qreg_size
            for qubit in range(first_qubit_of_qreg, first_qubit_of_qreg + n_qubits_of_qreg):
                optional_not_edited_input_state_qubit_checkbox: QtWidgets.QCheckBox | None = (
                    qreg_qubit_values_groupbox.findChild(
                        QtWidgets.QCheckBox,
                        INPUT_STATE_QUBIT_CHECKBOX_NAME_FORMAT.format(qubit=qubit),
                        QtCore.Qt.FindChildOption.FindDirectChildrenOnly,
                    )
                )
                optional_not_edited_output_state_qubit_checkbox: QtWidgets.QCheckBox | None = (
                    qreg_qubit_values_groupbox.findChild(
                        QtWidgets.QCheckBox, EXPECTED_OUTPUT_STATE_QUBIT_CHECKBOX_NAME_FORMAT.format(qubit=qubit)
                    )
                )

                if not self._assert_all_required_widgets_found_or_close_dialog(
                    [optional_not_edited_input_state_qubit_checkbox, optional_not_edited_output_state_qubit_checkbox],
                    f"Failed to find required QtWidgets for not edited input/output state qubit checkboxes of not edited quantum register '{qreg_name}' during handling of input/output state edit!",
                ):
                    return

                not_edited_input_state_qubit_checkbox = cast(
                    "QtWidgets.QGroupBox", optional_not_edited_input_state_qubit_checkbox
                )
                not_edited_output_state_qubit_checkbox = cast(
                    "QtWidgets.QGroupBox", optional_not_edited_output_state_qubit_checkbox
                )

                not_edited_input_state_qubit_checkbox.setEnabled(are_stringified_qreg_contents_valid)
                not_edited_output_state_qubit_checkbox.setEnabled(
                    are_stringified_qreg_contents_valid
                    and self.edited_simulation_run_model.expected_output_state is not None
                )

    @staticmethod
    def _stringify_some_qubits_of_n_bit_values_container(
        n_bit_values_container: NBitValuesContainer, first_qubit: int, n_qubits: int
    ) -> str:
        last_qubit_of_qreg: Final[int] = first_qubit + (n_qubits - 1)

        if first_qubit <= last_qubit_of_qreg < n_bit_values_container.size():
            return "".join([
                "1" if n_bit_values_container.test(i) else "0" for i in range(first_qubit, last_qubit_of_qreg + 1)
            ])
        return "<UNKNOWN>"

    @staticmethod
    def _get_internal_qubit_labels_for_qreg(
        annotatable_quantum_computation: AnnotatableQuantumComputation,
        first_qubit_of_qreg: int,
        n_qubits_in_qreg: int,
    ) -> list[str]:
        internal_qubit_labels: list[str] = []
        for qubit in range(first_qubit_of_qreg, first_qubit_of_qreg + n_qubits_in_qreg):
            fetched_internal_qubit_label: str | None = annotatable_quantum_computation.get_qubit_label(
                qubit, QubitLabelType.internal
            )
            if fetched_internal_qubit_label is None:
                continue
            internal_qubit_labels.append(fetched_internal_qubit_label)
        return internal_qubit_labels

    @staticmethod
    def _stringify_qubit_value(qubit_value: bool | None, *, return_as_high_low_state: bool) -> str:
        if qubit_value is None:
            return "UNKNOWN" if return_as_high_low_state else "-"

        if qubit_value is True:
            return "HIGH" if return_as_high_low_state else "1"

        return "LOW" if return_as_high_low_state else "0"

    def _assert_all_required_widgets_found_or_close_dialog(
        self, required_widgets: Iterable[QtWidgets.QWidget], error_dialog_content: str
    ) -> bool:
        if assert_all_required_widgets_found_or_close_dialog(
            self,
            required_widgets,
            error_dialog_content,
            num_additionally_skipped_stack_frames_starting_from_caller_function=1,
        ):
            return True

        self._failed_due_to_internal_error = True
        self.reject()
        return False
