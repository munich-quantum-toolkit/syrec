# Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
# Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

from __future__ import annotations

import re
import sys
from enum import Enum
from pathlib import Path
from typing import TYPE_CHECKING, Final, cast

if sys.version_info >= (3, 12):
    from typing import override
else:
    from typing_extensions import override

if sys.version_info >= (3, 11):
    from typing import assert_never
else:
    from typing_extensions import assert_never

from PyQt6 import QtCore, QtGui, QtWidgets

from mqt.syrec import NBitValuesContainer, QubitLabelType

from .message_box_utils import MessageBoxType, show_and_request_ok_in_optionally_cancellable_notification
from .simulation_view.dialogs.all_input_states_generator_dialog import AllInputStatesGeneratorDialog
from .simulation_view.dialogs.base_progress_dialog import BaseProgressDialog
from .simulation_view.dialogs.simulation_run_dialog import SimulationRunDialog
from .simulation_view.dialogs.simulation_run_editor_dialog import SimulationRunEditorDialog
from .simulation_view.dialogs.simulation_run_json_export_dialog import SimulationRunJsonExportDialog
from .simulation_view.dialogs.simulation_run_json_import_dialog import SimulationRunJsonImportDialog
from .simulation_view.simulation_run_model import (
    SIMULATION_RUN_IO_STATE_QT_ROLE,
    QtSimulationRunModel,
    SimulationRunModel,
)
from .simulation_view.styled_item_delegates.simulation_run_overview_styled_item_delegate import (
    SimulationRunOverviewStyledItemDelegate,
)
from .widget_check_utils import assert_all_required_widgets_found_or_close_dialog

if TYPE_CHECKING:
    from mqt.syrec import AnnotatableQuantumComputation

LOADED_FROM_FILE_INPUT_FIELD_NAME: Final[str] = "load_from_file_input_field"
IMPORT_FROM_FILE_BUTTON_NAME: Final[str] = "import_from_file_btn"
ADD_SIM_RUN_BTN_NAME: Final[str] = "add_sim_run_btn"
EDIT_SIM_RUN_BTN_NAME: Final[str] = "edit_sim_run_btn"
DELETE_SIM_RUN_BTN_NAME: Final[str] = "delete_sim_run_btn"
SAVE_SIM_RUNS_TO_FILE_BTN_NAME: Final[str] = "save_sims_to_file_btn"
SIMULATION_RUNS_LIST_VIEW_NAME: Final[str] = "sim_runs_list_view"
SIM_RUN_EXECUTION_TRIGGER_BTN_NAME: Final[str] = "sim_run_exec_trigger_btn"
SIM_RUN_EXECUTION_MODE_DROPDOWN_NAME: Final[str] = "sim_run_exec_mode_dropbown"

IMPORT_FROM_FILE_NO_FILE_SELECTED_PLACEHOLDER_TEXT: Final[str] = "<NONE>"

SOME_SIM_RUNS_TAB_WIDGET_NAME: Final[str] = "some_sim_runs_tab"
ALL_SIM_RUNS_TAB_WIDGET_NAME: Final[str] = "all_sim_runs_tab"
LOAD_SIM_RUNS_FROM_FILE_TAB_WIDGET_NAME: Final[str] = "load_sim_runs_from_file_tab"


class SimulationRunExecutionMode(Enum):
    RUN_ALL = 0
    RUN_ALL_STOP_AT_FIRST_FAILURE = 1
    RUN_SINGLE = 2


class QuantumCircuitSimulationDialog(QtWidgets.QDialog):  # type: ignore[misc]
    # We would like to reuse the regex in multiple one-time use dialog instances. Additionally, the regex is used only once in these instances so declaring the
    # regex as an instance variable seems wasteful. Whether a compiled or non-compiled regex should be used would have to be benchmarked.
    __syrec_program_comment_regex: re.Pattern[str] = re.compile("|".join(map(re.escape, [r"//", r"/*"])))

    def __init__(
        self,
        associated_stringified_syrec_program: str,
        annotatable_quantum_computation: AnnotatableQuantumComputation,
        parent: QtWidgets.QWidget,
    ) -> None:
        super().__init__(parent)
        self._did_syrec_program_contain_comments: Final[bool] = (
            self.__syrec_program_comment_regex.search(associated_stringified_syrec_program) is not None
        )
        self._associated_stringified_syrec_program: Final[str] = (
            associated_stringified_syrec_program if not self._did_syrec_program_contain_comments else ""
        )

        self._annotatable_quantum_computation: Final[AnnotatableQuantumComputation] = annotatable_quantum_computation
        self.setWindowTitle("Define simulation runs for quantum computation")
        # Ensure the dialog is deleted when closed this may not be strictly necessary but seems to be a good cleanup practice
        self.setAttribute(QtCore.Qt.WidgetAttribute.WA_DeleteOnClose)
        self.setModal(True)

        dialog_size: Final[QtCore.QSize] = BaseProgressDialog.get_default_big_dialog_size()
        center_dialog_pos_for_size: Final[QtCore.QPoint] = BaseProgressDialog.get_center_screen_position_for_size(
            dialog_size
        )
        self.setGeometry(
            center_dialog_pos_for_size.x(), center_dialog_pos_for_size.y(), dialog_size.width(), dialog_size.height()
        )

        self._simulation_run_editor_dialog: SimulationRunEditorDialog | None = None
        self._all_input_states_generator_dialog: AllInputStatesGeneratorDialog | None = None
        self._simulation_run_import_from_file_dialog: SimulationRunJsonImportDialog | None = None
        self._simulation_run_export_to_file_dialog: SimulationRunJsonExportDialog | None = None
        self._simulation_run_dialog: SimulationRunDialog | None = None

        self._expected_input_output_state_size: Final[int] = (
            QuantumCircuitSimulationDialog._determine_num_non_ancillary_qubits(
                self._annotatable_quantum_computation, potential_error_dialog_parent=self
            )
        )
        self._simulation_runs_model: QtSimulationRunModel = QtSimulationRunModel(annotatable_quantum_computation, self)
        self._shared_selected_sim_run_execution_mode_dropdown_index: int = 0
        self._prev_active_simulation_runs_tab_idx: int = 0

        self._simulation_runs_tab_widget = QtWidgets.QTabWidget(self)
        self._simulation_runs_tab_widget.currentChanged.connect(self.handle_simulation_runs_tab_widget_tab_changed)
        self._simulation_runs_tab_widget.addTab(
            self.initialize_simulation_runs_tab_widget(self._simulation_runs_model, SOME_SIM_RUNS_TAB_WIDGET_NAME),
            "Check some input-output mapping combinations",
        )
        self._simulation_runs_tab_widget.addTab(
            self.initialize_simulation_runs_tab_widget(self._simulation_runs_model, ALL_SIM_RUNS_TAB_WIDGET_NAME),
            "Check all input-output mapping combinations",
        )
        self._simulation_runs_tab_widget.addTab(
            self.initialize_simulation_runs_tab_widget(
                self._simulation_runs_model,
                LOAD_SIM_RUNS_FROM_FILE_TAB_WIDGET_NAME,
                create_load_from_file_controls=True,
            ),
            "Check input-output mapping combinations from file",
        )

        self.layout = QtWidgets.QVBoxLayout()
        self.layout.addWidget(self._simulation_runs_tab_widget)
        self.setLayout(self.layout)
        self.setSizeGripEnabled(True)

    def show_save_changes_reminder(self) -> None:
        show_and_request_ok_in_optionally_cancellable_notification(
            message_box_type=MessageBoxType.INFO,
            message_box_parent=self,
            message_box_title="Remember to save your changes",
            message_box_content="All simulation runs not saved via the save to file option are removed when this dialog closes!",
            is_cancellable=False,
            log_contents=False,
        )

    def show_optional_comments_in_syrec_program_not_supported_notification(self) -> None:
        if not self._did_syrec_program_contain_comments:
            return

        show_and_request_ok_in_optionally_cancellable_notification(
            message_box_type=MessageBoxType.WARNING,
            message_box_parent=self,
            message_box_title="Synthesized SyReC program contained line or block comments!",
            message_box_content="SyReC programs containing line or block comments cannot be serialized to JSON since this would generate invalid JSON. Export to JSON functionality is disabled.",
            is_cancellable=False,
            log_contents=False,
        )

    def initialize_simulation_runs_tab_widget(
        self,
        shared_simulation_runs_model: QtSimulationRunModel,
        tab_widget_object_name: str,
        create_load_from_file_controls: bool = False,
    ) -> QtWidgets.QWidget:
        tab_wrapper_widget = QtWidgets.QFrame(objectName=tab_widget_object_name)
        tab_wrapper_widget_layout = QtWidgets.QVBoxLayout()
        tab_wrapper_widget.setLayout(tab_wrapper_widget_layout)
        tab_wrapper_widget.setAutoFillBackground(True)

        manual_y_space_size: Final[int] = 35
        if create_load_from_file_controls:
            tab_wrapper_widget_layout.addLayout(
                QuantumCircuitSimulationDialog.initialize_load_simulation_runs_from_file_controls(self)
            )
            tab_wrapper_widget_layout.addSpacing(manual_y_space_size)

        # BEGIN: Create simulation runs list view Qt elements
        simulation_runs_list_view = QtWidgets.QListView(objectName=SIMULATION_RUNS_LIST_VIEW_NAME)
        simulation_runs_list_view.setModel(shared_simulation_runs_model)
        simulation_runs_list_view.setItemDelegate(SimulationRunOverviewStyledItemDelegate())
        simulation_runs_list_view.setUniformItemSizes(True)
        simulation_runs_list_view.setResizeMode(QtWidgets.QListView.ResizeMode.Adjust)
        simulation_runs_list_view.setAutoFillBackground(False)
        simulation_runs_list_view.setSpacing(5)
        simulation_runs_list_view.setFlow(QtWidgets.QListView.Flow.TopToBottom)
        # By default the vertical scroll mode is set to ScrollPerItem which will prevent the user to view not displayed if the vertical viewport size is larger than the required height of the list view item.
        simulation_runs_list_view.setVerticalScrollMode(QtWidgets.QAbstractItemView.ScrollMode.ScrollPerPixel)
        # Select with click on item, unselect with Ctrl+Click on already selected item (see https://doc.qt.io/qt-6/qabstractitemview.html#SelectionMode-enum)
        simulation_runs_list_view.setSelectionMode(QtWidgets.QAbstractItemView.SelectionMode.SingleSelection)
        simulation_runs_list_view.selectionModel().selectionChanged.connect(self.handle_simulation_run_selection_change)

        simulation_runs_list_scrollarea = QtWidgets.QScrollArea()
        simulation_runs_list_scrollarea.setAutoFillBackground(False)
        simulation_runs_list_scrollarea.setWidget(simulation_runs_list_view)
        simulation_runs_list_scrollarea.setWidgetResizable(True)
        tab_wrapper_widget_layout.addWidget(simulation_runs_list_scrollarea)

        simulation_runs_list_selection_info_layout = QtWidgets.QHBoxLayout()
        simulation_runs_list_selection_info_lbl: QtWidgets.QLabel = QtWidgets.QLabel(
            "Select simulation runs with a left click while unselecting them with CTRL+left click"
        )
        simulation_runs_list_selection_info_lbl.setStyleSheet("QLabel { color : gray; }")
        simulation_runs_list_selection_info_layout.addStretch()
        simulation_runs_list_selection_info_layout.addWidget(simulation_runs_list_selection_info_lbl)
        simulation_runs_list_selection_info_layout.addStretch()
        tab_wrapper_widget_layout.addLayout(simulation_runs_list_selection_info_layout)
        # END: Create simulation runs list view Qt elements

        # BEGIN: Create simulation runs list modification Qt elements
        simulation_runs_list_modification_buttons_layout = QtWidgets.QHBoxLayout()
        simulation_runs_list_modification_buttons_layout.addStretch()

        add_simulation_run_button = QtWidgets.QPushButton(
            QtGui.QIcon.fromTheme(QtGui.QIcon.ThemeIcon.ListAdd), "Add simulation run", objectName=ADD_SIM_RUN_BTN_NAME
        )
        add_simulation_run_button.setEnabled(not create_load_from_file_controls)
        add_simulation_run_button.clicked.connect(self.handle_simulation_run_add_btn_click)
        simulation_runs_list_modification_buttons_layout.addWidget(add_simulation_run_button)

        edit_simulation_run_button = QtWidgets.QPushButton(
            QtGui.QIcon.fromTheme(QtGui.QIcon.ThemeIcon.DocumentProperties),
            "Edit simulation run",
            objectName=EDIT_SIM_RUN_BTN_NAME,
        )
        edit_simulation_run_button.setEnabled(False)
        edit_simulation_run_button.clicked.connect(self.handle_simulation_run_edit_btn_click)
        simulation_runs_list_modification_buttons_layout.addWidget(edit_simulation_run_button)

        delete_simulation_run_button = QtWidgets.QPushButton(
            QtGui.QIcon.fromTheme(QtGui.QIcon.ThemeIcon.EditDelete),
            "Delete simulation run",
            objectName=DELETE_SIM_RUN_BTN_NAME,
        )
        delete_simulation_run_button.setEnabled(False)
        delete_simulation_run_button.clicked.connect(self.handle_simulation_run_delete_btn_click)
        simulation_runs_list_modification_buttons_layout.addWidget(delete_simulation_run_button)

        simulation_runs_list_modification_buttons_layout.addStretch()
        tab_wrapper_widget_layout.addLayout(simulation_runs_list_modification_buttons_layout)
        # END: Create simulation runs list modification Qt elements

        # BEGIN: Create simulation runs execution Qt elements
        simulation_runs_execution_buttons_layout = QtWidgets.QHBoxLayout()
        simulation_runs_execution_buttons_layout.addStretch()

        sim_run_execution_mode_controls_layout = QtWidgets.QGridLayout()
        simulation_runs_execution_buttons_layout.addLayout(sim_run_execution_mode_controls_layout)

        sim_run_execution_mode_dropdown_lbl = QtWidgets.QLabel("Simulation run execution mode:")
        sim_run_execution_mode_dropdown = QtWidgets.QComboBox(objectName=SIM_RUN_EXECUTION_MODE_DROPDOWN_NAME)
        sim_run_execution_mode_dropdown.insertItem(0, "Run all simulation runs", SimulationRunExecutionMode.RUN_ALL)
        sim_run_execution_mode_dropdown.insertItem(
            1,
            "Run all simulation runs (stop at first output qubit value mismatch)",
            SimulationRunExecutionMode.RUN_ALL_STOP_AT_FIRST_FAILURE,
        )
        sim_run_execution_mode_dropdown.insertItem(
            2, "Run selected simulation run", SimulationRunExecutionMode.RUN_SINGLE
        )
        sim_run_execution_mode_dropdown.setPlaceholderText("<UNKNOWN EXECUTION MODE>")
        sim_run_execution_mode_dropdown.setSizeAdjustPolicy(QtWidgets.QComboBox.SizeAdjustPolicy.AdjustToContents)
        sim_run_execution_mode_dropdown.setEnabled(False)
        sim_run_execution_mode_dropdown.currentIndexChanged.connect(
            self.handle_simulation_run_execution_mode_selection_change
        )

        sim_run_execution_mode_controls_layout.addWidget(sim_run_execution_mode_dropdown_lbl, 0, 0)
        sim_run_execution_mode_controls_layout.addWidget(sim_run_execution_mode_dropdown, 1, 0)

        sim_run_execution_trigger_btn = QtWidgets.QPushButton(
            QtGui.QIcon.fromTheme(QtGui.QIcon.ThemeIcon.MediaPlaybackStart),
            "Execute simulation runs",
            objectName=SIM_RUN_EXECUTION_TRIGGER_BTN_NAME,
        )
        sim_run_execution_trigger_btn.clicked.connect(self._open_simulation_runs_execution_dialog)
        sim_run_execution_trigger_btn.setEnabled(False)
        sim_run_execution_mode_controls_layout.addWidget(sim_run_execution_trigger_btn, 0, 1)

        save_simulation_runs_to_file_button = QtWidgets.QPushButton(
            QtGui.QIcon.fromTheme(QtGui.QIcon.ThemeIcon.DocumentSave),
            "Save simulation runs to file",
            objectName=SAVE_SIM_RUNS_TO_FILE_BTN_NAME,
        )
        save_simulation_runs_to_file_button.clicked.connect(self.handle_sim_run_save_to_file_btn_click)
        save_simulation_runs_to_file_button.setEnabled(False)
        save_simulation_runs_to_file_button.setToolTip(
            "Save the defined simulation runs to a .json file (only the input and expected output qubit values of simulation runs in which both input and output qubit values are known are exported)"
        )
        sim_run_execution_mode_controls_layout.addWidget(save_simulation_runs_to_file_button, 1, 1)
        simulation_runs_execution_buttons_layout.addStretch()

        tab_wrapper_widget_layout.addSpacing(manual_y_space_size)
        tab_wrapper_widget_layout.addLayout(simulation_runs_execution_buttons_layout)
        # END: Create simulation runs execution Qt elements
        return tab_wrapper_widget

    def show_close_confirmation_dialog_and_return_boolean_user_choice(self) -> bool:
        return show_and_request_ok_in_optionally_cancellable_notification(
            message_box_type=MessageBoxType.INFO,
            message_box_parent=self,
            message_box_title="Confirm dialog close",
            message_box_content="Do you want to close the simulation run dialog, any unsaved simulation runs will be lost?",
            is_cancellable=True,
            log_contents=False,
        )

    # Pressing the ESC key will only close the dialog but not close it thus no closeEvent will be triggered.
    @override
    def reject(self) -> None:
        if self.show_close_confirmation_dialog_and_return_boolean_user_choice():
            super().reject()

    @override
    def closeEvent(self, event: QtGui.QCloseEvent) -> None:
        # Ask for confirmation before closing
        self.accept() if self.show_close_confirmation_dialog_and_return_boolean_user_choice() else event.ignore()

    @QtCore.pyqtSlot(QtCore.QItemSelection, QtCore.QItemSelection)  # type: ignore[untyped-decorator]
    def handle_simulation_run_selection_change(
        self,
        selected: QtCore.QItemSelection,
        deselected: QtCore.QItemSelection,
        optional_tab_widget_to_apply_selection_change_to: QtWidgets.QTabWidget | None = None,
    ) -> None:
        # We want to only update the simulation run execution controls in case that a selected simulation run was deselected without selecting a new simulation run or vice versa.
        # In all other cases leave the enabled state of the simulation run controls the same by simply returning from this function.
        if selected.isEmpty() == deselected.isEmpty():
            return

        optional_curr_active_tab_widget: QtWidgets.QWidget | None = (
            self._simulation_runs_tab_widget.currentWidget()
            if optional_tab_widget_to_apply_selection_change_to is None
            else optional_tab_widget_to_apply_selection_change_to
        )
        if not assert_all_required_widgets_found_or_close_dialog(
            error_notification_parent_widget=self,
            required_widgets=[optional_curr_active_tab_widget],
            error_dialog_content="Failed to locate current active tab widget during simulation run selection change",
        ):
            return

        is_list_item_selected: bool = not selected.isEmpty() and deselected.isEmpty()
        curr_active_tab_widget: Final[QtWidgets.QWidget] = cast("QtWidgets.QWidget", optional_curr_active_tab_widget)

        optional_add_simulation_run_btn: QtWidgets.QPushButton | None = curr_active_tab_widget.findChild(
            QtWidgets.QPushButton, ADD_SIM_RUN_BTN_NAME
        )
        optional_edit_simulation_run_btn: QtWidgets.QPushButton | None = curr_active_tab_widget.findChild(
            QtWidgets.QPushButton, EDIT_SIM_RUN_BTN_NAME
        )
        optional_delete_simulation_run_btn: QtWidgets.QPushButton | None = curr_active_tab_widget.findChild(
            QtWidgets.QPushButton, DELETE_SIM_RUN_BTN_NAME
        )

        if not assert_all_required_widgets_found_or_close_dialog(
            error_notification_parent_widget=self,
            required_widgets=[
                optional_add_simulation_run_btn,
                optional_edit_simulation_run_btn,
                optional_delete_simulation_run_btn,
            ],
            error_dialog_content="Failed to locate simulation run control buttons during simulation run selection change",
        ):
            return

        add_simulation_run_btn: Final[QtWidgets.QWidget] = cast(
            "QtWidgets.QPushButton", optional_add_simulation_run_btn
        )
        edit_simulation_run_btn: Final[QtWidgets.QWidget] = cast(
            "QtWidgets.QPushButton", optional_edit_simulation_run_btn
        )
        delete_simulation_run_btn: Final[QtWidgets.QWidget] = cast(
            "QtWidgets.QPushButton", optional_delete_simulation_run_btn
        )

        add_simulation_run_btn.setEnabled(not is_list_item_selected)
        edit_simulation_run_btn.setEnabled(is_list_item_selected)
        delete_simulation_run_btn.setEnabled(is_list_item_selected)
        self.set_enabled_state_of_simulation_run_execution_controls_in_tab_widget_based_on_sim_run_selection_status(
            curr_active_tab_widget, is_list_item_selected
        )

    @QtCore.pyqtSlot()  # type: ignore[untyped-decorator]
    def handle_simulation_run_add_btn_click(self) -> None:
        self._simulation_runs_model.add_simulation_run_model(
            SimulationRunModel(
                input_state=NBitValuesContainer(self._expected_input_output_state_size),
                expected_output_state=None,
            )
        )

        optional_curr_active_tab_widget: QtWidgets.QWidget | None = self._simulation_runs_tab_widget.currentWidget()
        if not assert_all_required_widgets_found_or_close_dialog(
            error_notification_parent_widget=self,
            required_widgets=[optional_curr_active_tab_widget],
            error_dialog_content="Failed to locate current active tab widget during simulation run add button click",
        ):
            return

        curr_active_tab_widget: Final[QtWidgets.QWidget] = cast("QtWidgets.QWidget", optional_curr_active_tab_widget)
        self.set_enabled_state_of_simulation_run_execution_controls_in_tab_widget(curr_active_tab_widget, True)

        optional_simulation_runs_list_view: QtWidgets.QWidget | None = curr_active_tab_widget.findChild(
            QtWidgets.QListView, SIMULATION_RUNS_LIST_VIEW_NAME
        )
        if not assert_all_required_widgets_found_or_close_dialog(
            error_notification_parent_widget=self,
            required_widgets=[optional_simulation_runs_list_view],
            error_dialog_content="Failed to locate simulation run list view during simulation run add button click",
        ):
            return

        simulation_runs_list_view: Final[QtWidgets.QListView] = cast(
            "QtWidgets.QListView", optional_simulation_runs_list_view
        )
        simulation_runs_list_view.scrollToBottom()

    @QtCore.pyqtSlot()  # type: ignore[untyped-decorator]
    def handle_simulation_run_edit_btn_click(self) -> None:
        optional_curr_active_tab_widget: QtWidgets.QWidget | None = self._simulation_runs_tab_widget.currentWidget()
        optional_simulation_runs_list_view: QtWidgets.QListView | None = (
            optional_curr_active_tab_widget.findChild(QtWidgets.QListView, SIMULATION_RUNS_LIST_VIEW_NAME)
            if optional_curr_active_tab_widget is not None
            else None
        )

        if not assert_all_required_widgets_found_or_close_dialog(
            error_notification_parent_widget=self,
            required_widgets=[optional_curr_active_tab_widget, optional_simulation_runs_list_view],
            error_dialog_content="Failed to locate required QtWidgets during simulation run edit button click",
        ):
            return

        simulation_runs_list_view: Final[QtWidgets.QWidget] = cast(
            "QtWidgets.QListView", optional_simulation_runs_list_view
        )

        reference_sim_run_model: SimulationRunModel = simulation_runs_list_view.currentIndex().data(
            SIMULATION_RUN_IO_STATE_QT_ROLE
        )
        if (
            reference_sim_run_model.expected_output_state is not None
            and reference_sim_run_model.input_state.size() != reference_sim_run_model.expected_output_state.size()
        ):
            show_and_request_ok_in_optionally_cancellable_notification(
                message_box_type=MessageBoxType.ERROR,
                message_box_parent=self,
                message_box_title="Initial simulation run model validation error",
                message_box_content=f"Expected reference simulation runs input state size (n={reference_sim_run_model.input_state.size()}) to match expected output states size (n={reference_sim_run_model.expected_output_state.size()})",
                is_cancellable=False,
                log_contents=False,
            )
            return

        # Since we want to be able to discard the changes made in the dialog by either closing the dialog or by pressing the cancel button
        # a copy of the original simulation run object is needed
        copy_of_reference_sim_run_model: SimulationRunModel = SimulationRunModel(
            reference_sim_run_model.input_state,
            reference_sim_run_model.expected_output_state,
            reference_sim_run_model.actual_output_state,
            create_new_n_bit_values_container_instances=True,
        )

        self._simulation_run_editor_dialog = SimulationRunEditorDialog(
            simulation_runs_list_view.currentIndex(), copy_of_reference_sim_run_model, self
        )
        self._simulation_run_editor_dialog.finished.connect(self.handle_simulation_run_editor_dialog_close)
        self._simulation_run_editor_dialog.show()

    @QtCore.pyqtSlot(int)  # type: ignore[untyped-decorator]
    def handle_simulation_run_editor_dialog_close(self, result: int) -> None:
        # This should not happen but is checked nevertheless
        if self._simulation_run_editor_dialog is None or result == QtWidgets.QDialog.DialogCode.Rejected:
            return

        try:
            self._simulation_runs_model.update_edited_simulation_run_model(
                self._simulation_run_editor_dialog.simulation_run_model_index,
                self._simulation_run_editor_dialog.edited_simulation_run_model,
            )
        except ValueError as err:
            show_and_request_ok_in_optionally_cancellable_notification(
                message_box_type=MessageBoxType.ERROR,
                message_box_parent=self,
                message_box_title="Simulation run model update error!",
                message_box_content=f"Update of simulation run model {self._simulation_run_editor_dialog.simulation_run_model_index.row()} failed due to an error!\nReason: {err}",
                is_cancellable=False,
                log_contents=False,
            )
        finally:
            self._simulation_run_editor_dialog = None

    @QtCore.pyqtSlot()  # type: ignore[untyped-decorator]
    def handle_sim_run_save_to_file_btn_click(self) -> None:
        if self._simulation_run_export_to_file_dialog is not None:
            show_and_request_ok_in_optionally_cancellable_notification(
                message_box_type=MessageBoxType.ERROR,
                message_box_parent=self,
                message_box_title="Simulation run export dialog initialization error!",
                message_box_content="Expected no simulation run export dialog instance to exist.",
                is_cancellable=False,
                log_contents=True,
            )
            return

        filename, _ = QtWidgets.QFileDialog.getSaveFileName(
            self, "Select a file to export simulation runs to", str(Path.home()), "Json files (*.json)"
        )

        if not filename:
            return

        self._simulation_run_export_to_file_dialog = SimulationRunJsonExportDialog(
            parent=self, shared_simulation_runs_model=self._simulation_runs_model
        )
        self._simulation_run_export_to_file_dialog.finished.connect(self.handle_sim_run_export_to_file_dialog_close)
        self._simulation_run_export_to_file_dialog.start_export(
            Path(filename),
            self._associated_stringified_syrec_program,
            self._simulation_runs_model.rowCount(QtCore.QModelIndex()),
        )
        self._simulation_run_export_to_file_dialog.show()

    @QtCore.pyqtSlot(int)  # type: ignore[untyped-decorator]
    def handle_sim_run_export_to_file_dialog_close(self, _: int) -> None:
        self._simulation_run_export_to_file_dialog = None

    @QtCore.pyqtSlot(int)  # type: ignore[untyped-decorator]
    def handle_open_and_start_all_input_states_generator_dialog(self, input_state_size: int) -> None:
        if self._all_input_states_generator_dialog is not None:
            show_and_request_ok_in_optionally_cancellable_notification(
                message_box_type=MessageBoxType.ERROR,
                message_box_parent=self,
                message_box_title="Input states generator dialog initialization error!",
                message_box_content="Expected no input states generator dialog instance to exist.",
                is_cancellable=False,
                log_contents=True,
            )
            return

        self._all_input_states_generator_dialog = AllInputStatesGeneratorDialog(
            parent=self, shared_simulation_runs_model=self._simulation_runs_model
        )
        self._all_input_states_generator_dialog.finished.connect(self.handle_input_states_generator_dialog_close)
        self._all_input_states_generator_dialog.show()
        self._all_input_states_generator_dialog.start_generation(input_state_size)

    @QtCore.pyqtSlot(int)  # type: ignore[untyped-decorator]
    def handle_input_states_generator_dialog_close(self, result: int) -> None:
        self._all_input_states_generator_dialog = None

        optional_curr_active_tab_widget: QtWidgets.QWidget | None = self._simulation_runs_tab_widget.currentWidget()
        if not assert_all_required_widgets_found_or_close_dialog(
            error_notification_parent_widget=self,
            required_widgets=[optional_curr_active_tab_widget],
            error_dialog_content="Failed to locate active tab widget in input states generator dialog close handler",
        ):
            return

        curr_active_tab_widget: Final[QtWidgets.QWidget] = cast("QtWidgets.QWidget", optional_curr_active_tab_widget)
        self.set_enabled_state_of_simulation_run_execution_controls_in_tab_widget(
            curr_active_tab_widget, result == QtWidgets.QDialog.DialogCode.Accepted
        )

    @QtCore.pyqtSlot()  # type: ignore[untyped-decorator]
    def handle_simulation_run_delete_btn_click(self) -> None:
        optional_curr_active_tab_widget: QtWidgets.QWidget | None = self._simulation_runs_tab_widget.currentWidget()
        optional_simulation_runs_list_view: QtWidgets.QListView | None = (
            optional_curr_active_tab_widget.findChild(QtWidgets.QListView, SIMULATION_RUNS_LIST_VIEW_NAME)
            if optional_curr_active_tab_widget is not None
            else None
        )

        if not assert_all_required_widgets_found_or_close_dialog(
            error_notification_parent_widget=self,
            required_widgets=[optional_curr_active_tab_widget, optional_simulation_runs_list_view],
            error_dialog_content="Failed to locate required QtWidgets during simulation run delete button click",
        ):
            return

        curr_active_tab_widget: Final[QtWidgets.QWidget] = cast("QtWidgets.QWidget", optional_curr_active_tab_widget)
        simulation_runs_list_view: Final[QtWidgets.QWidget] = cast(
            "QtWidgets.QListView", optional_simulation_runs_list_view
        )

        if not self._simulation_runs_model.delete_simulation_run_model(simulation_runs_list_view.currentIndex()):
            return

        # Deletion of an element should only be enabled when an item in the QListView is selected. After the deletion
        # and the subsequent update of the backing model of the QListView selection will switch to the element the index
        # of the previously selected element thus the simulation run execution controls should not be enabled after an element
        # is deleted
        self.set_enabled_state_of_simulation_run_execution_controls_in_tab_widget(curr_active_tab_widget, False)

    def initialize_load_simulation_runs_from_file_controls(self) -> QtWidgets.QLayout:
        controls_layout = QtWidgets.QHBoxLayout()
        controls_layout.addStretch()

        info_label = QtWidgets.QLabel("File to load simulation runs from:")
        controls_layout.addWidget(info_label)

        selected_file_name_label = QtWidgets.QLabel(
            IMPORT_FROM_FILE_NO_FILE_SELECTED_PLACEHOLDER_TEXT, objectName=LOADED_FROM_FILE_INPUT_FIELD_NAME
        )
        selected_file_name_label.setEnabled(False)
        controls_layout.addWidget(selected_file_name_label)

        open_file_dialog_button = QtWidgets.QPushButton("Select file...")
        open_file_dialog_button.clicked.connect(self.open_import_file_selector)
        controls_layout.addWidget(open_file_dialog_button)

        trigger_load_from_file_button = QtWidgets.QPushButton(
            QtGui.QIcon.fromTheme(QtGui.QIcon.ThemeIcon.DocumentOpen),
            "Load from file",
            objectName=IMPORT_FROM_FILE_BUTTON_NAME,
        )
        trigger_load_from_file_button.clicked.connect(self.open_import_from_file_dialog)
        trigger_load_from_file_button.setEnabled(False)
        trigger_load_from_file_button.setToolTip("""
        <h2>Expected format of .json file</h2>
        <div>
        Simulation runs imported from a .json file need to be defined in the following json structure:
        <code style="display: block; white-space: pre-wrap">
        {
            "simulationRuns": [
                { "in": "1011", "out": "1011" },
                ...
                { "in": "1001", "out": "1001" }
            ]
        }
        </code>
        </div>
        <div>
            <b>Further details about the contents of the .json file are listed below:</b>
            <ul>
                <li>
                    The 'simulationRuns' JSON array needs to be defined as a property of the singular top level JSON object,
                    with every simulation run being defined as a JSON object consisting of an input state qubit values definition
                    and an optional expected output state definition. All other elements of the top-level object are ignored.
                </li>
                <li>
                    All expected json element keys are case sensitive.
                </li>
                <li>
                    Qubit values must be defined as strings containing '0' or '1' characters with the total number of qubits in a
                    state definition matching the number of data qubits of the synthesized quantum computation (i.e. equal to the number of non-ancillary qubits).
                </li>
                <li>
                    Any additional objects defined in a simulation run JSON object is skipped.
                </li>
                <li>
                    No error will be reported if the 'simulationRuns' object was not defined in the .json file or contained no entries.
                </li>
                <li>
                    Any error during the parsing of the .json file will cause the deletion of all parsed simulation runs.
                </li>
            </ul>
        </div>
        """)
        controls_layout.addWidget(trigger_load_from_file_button)

        controls_layout.addStretch()
        return controls_layout

    @QtCore.pyqtSlot(int)  # type: ignore[untyped-decorator]
    def handle_simulation_runs_tab_widget_tab_changed(self, switched_to_tab_index: int) -> None:
        if switched_to_tab_index == -1:
            return

        if switched_to_tab_index == self._prev_active_simulation_runs_tab_idx:
            return

        optional_prev_active_tab_widget: QtWidgets.QWidget | None = self._simulation_runs_tab_widget.widget(
            self._prev_active_simulation_runs_tab_idx
        )
        optional_to_be_switched_to_tab_widget: QtWidgets.QWidget | None = self._simulation_runs_tab_widget.widget(
            switched_to_tab_index
        )
        optional_sim_run_exec_mode_dropdown_in_switched_to_tab: QtWidgets.QComboBox | None = (
            optional_to_be_switched_to_tab_widget.findChild(QtWidgets.QComboBox, SIM_RUN_EXECUTION_MODE_DROPDOWN_NAME)
            if optional_to_be_switched_to_tab_widget is not None
            else None
        )

        if not assert_all_required_widgets_found_or_close_dialog(
            error_notification_parent_widget=self,
            required_widgets=[
                optional_prev_active_tab_widget,
                optional_to_be_switched_to_tab_widget,
                optional_sim_run_exec_mode_dropdown_in_switched_to_tab,
            ],
            error_dialog_content="Failed to locate previous/current active tab widget in simulation run tab change handler",
        ):
            if optional_to_be_switched_to_tab_widget is None:
                self._simulation_runs_tab_widget.setCurrentIndex(self._prev_active_simulation_runs_tab_idx)
            return

        prev_active_tab_widget: Final[QtWidgets.QWidget] = cast("QtWidgets.QWidget", optional_prev_active_tab_widget)
        to_be_switched_to_tab_widget: Final[QtWidgets.QWidget] = cast(
            "QtWidgets.QWidget", optional_to_be_switched_to_tab_widget
        )

        if self._simulation_runs_model.rowCount(QtCore.QModelIndex()) > 0:
            if not show_and_request_ok_in_optionally_cancellable_notification(
                message_box_type=MessageBoxType.WARNING,
                message_box_parent=self,
                message_box_title="Existing simulation runs detected!",
                message_box_content="Switching tabs will delete all existing simulation runs. Do you want to continue?",
                is_cancellable=True,
                log_contents=False,
            ):
                self._simulation_runs_tab_widget.setCurrentIndex(self._prev_active_simulation_runs_tab_idx)
                return
            self._clear_simulation_run_list_and_backing_model(prev_active_tab_widget)
        self.set_enabled_state_of_simulation_run_execution_controls_in_tab_widget(to_be_switched_to_tab_widget, False)

        if to_be_switched_to_tab_widget.objectName() == ALL_SIM_RUNS_TAB_WIDGET_NAME:
            n_input_state_combinations: int = 2**self._expected_input_output_state_size
            if not show_and_request_ok_in_optionally_cancellable_notification(
                message_box_type=MessageBoxType.WARNING,
                message_box_parent=self,
                message_box_title="Generating all possible input state combinations!",
                message_box_content=f"Are you sure that you want to generate {n_input_state_combinations} simulation runs, one for each input state combination?",
                is_cancellable=True,
                log_contents=False,
            ):
                self._simulation_runs_tab_widget.setCurrentIndex(self._prev_active_simulation_runs_tab_idx)
                self.set_default_simulation_run_modification_buttons_enabled_state(prev_active_tab_widget)
                return

            self.handle_open_and_start_all_input_states_generator_dialog(self._expected_input_output_state_size)

        sim_run_exec_mode_dropdown: Final[QtWidgets.QComboBox] = cast(
            "QtWidgets.QComboBox", optional_sim_run_exec_mode_dropdown_in_switched_to_tab
        )
        # Setting an invalid current index will not throw an error but sets the current index to -1.
        # With the assumption that the selectable simulation run execution modes in the ComboBox do not change at runtime
        # and our override of the selection change slot of the ComboBox not changing the selected index then invalid dropdown indices
        # can only stem from this setter call (assuming that no other sets are added to this class in the future).
        sim_run_exec_mode_dropdown.setCurrentIndex(self._shared_selected_sim_run_execution_mode_dropdown_index)
        self.set_enabled_state_of_simulation_run_execution_controls_in_tab_widget(prev_active_tab_widget, False)
        self._prev_active_simulation_runs_tab_idx = switched_to_tab_index

    @QtCore.pyqtSlot()  # type: ignore[untyped-decorator]
    def _open_simulation_runs_execution_dialog(self) -> None:
        if self._simulation_run_dialog is not None:
            show_and_request_ok_in_optionally_cancellable_notification(
                message_box_type=MessageBoxType.ERROR,
                message_box_parent=self,
                message_box_title="Simulation run dialog initialization error!",
                message_box_content="Expected no simulation run dialog instance to exist.",
                is_cancellable=False,
                log_contents=True,
            )
            return

        optional_curr_active_tab_widget: QtWidgets.QWidget | None = self._simulation_runs_tab_widget.widget(
            self._simulation_runs_tab_widget.currentIndex()
        )

        optional_simulation_runs_list_view: QtWidgets.QWidget | None = (
            optional_curr_active_tab_widget.findChild(QtWidgets.QListView, SIMULATION_RUNS_LIST_VIEW_NAME)
            if optional_curr_active_tab_widget is not None
            else None
        )

        optional_sim_run_exec_mode_dropdown: QtWidgets.QComboBox | None = (
            optional_curr_active_tab_widget.findChild(QtWidgets.QComboBox, SIM_RUN_EXECUTION_MODE_DROPDOWN_NAME)
            if optional_curr_active_tab_widget is not None
            else None
        )

        if not assert_all_required_widgets_found_or_close_dialog(
            error_notification_parent_widget=self,
            required_widgets=[
                optional_curr_active_tab_widget,
                optional_simulation_runs_list_view,
                optional_sim_run_exec_mode_dropdown,
            ],
            error_dialog_content="Failed to locate all required QtWidgets in open simulation run execution dialog handler",
        ):
            return

        simulation_runs_list_view: Final[QtWidgets.QListView] = cast(
            "QtWidgets.QListView", optional_simulation_runs_list_view
        )
        sim_run_exec_mode_dropdown: Final[QtWidgets.QComboBox] = cast(
            "QtWidgets.QComboBox", optional_sim_run_exec_mode_dropdown
        )

        selected_sim_run_model_idx: QtCore.QModelIndex | None = None
        curr_sim_run_exec_mode: Final[SimulationRunExecutionMode | None] = sim_run_exec_mode_dropdown.currentData()
        if curr_sim_run_exec_mode is None:
            show_and_request_ok_in_optionally_cancellable_notification(
                message_box_type=MessageBoxType.ERROR,
                message_box_parent=self,
                message_box_title="Unknown selected simulation run execution mode!",
                message_box_content="Failed to determine selected simulation run execution mode while initializing simulation run execution dialog!",
                is_cancellable=False,
            )
            return

        if curr_sim_run_exec_mode == SimulationRunExecutionMode.RUN_SINGLE:
            curr_num_selected_simulation_runs: Final[int] = len(simulation_runs_list_view.selectedIndexes())
            # We are assuming that the QListView only supports single item selection but QListView only offers fetch of all selected indices
            if curr_num_selected_simulation_runs > 0:
                selected_sim_run_model_idx = simulation_runs_list_view.selectedIndexes()[0]
            else:
                show_and_request_ok_in_optionally_cancellable_notification(
                    message_box_type=MessageBoxType.ERROR,
                    message_box_parent=self,
                    message_box_title="Failed to determine selected simulation run!",
                    message_box_content=f"Tried to find the selected simulation run in the list of simulation runs but {curr_num_selected_simulation_runs} where selected!",
                    is_cancellable=False,
                )
                return

        self._simulation_run_dialog = SimulationRunDialog(
            parent=self,
            shared_simulation_runs_model=self._simulation_runs_model,
            annotatable_quantum_computation=self._annotatable_quantum_computation,
            expected_input_output_state_size=self._expected_input_output_state_size,
        )
        self._simulation_run_dialog.finished.connect(self.handle_simulation_runs_dialog_close)
        self._simulation_run_dialog.show()

        match curr_sim_run_exec_mode:
            case SimulationRunExecutionMode.RUN_SINGLE:
                assert selected_sim_run_model_idx is not None
                self._simulation_run_dialog.start_simulation(selected_sim_run_model_idx)
            case SimulationRunExecutionMode.RUN_ALL | SimulationRunExecutionMode.RUN_ALL_STOP_AT_FIRST_FAILURE:
                self._simulation_run_dialog.start_simulations(
                    curr_sim_run_exec_mode == SimulationRunExecutionMode.RUN_ALL_STOP_AT_FIRST_FAILURE
                )
            case _:
                assert_never(curr_sim_run_exec_mode)

    @QtCore.pyqtSlot(int)  # type: ignore[untyped-decorator]
    def handle_simulation_runs_dialog_close(self, _: int) -> None:
        self._simulation_run_dialog = None

    @QtCore.pyqtSlot()  # type: ignore[untyped-decorator]
    def open_import_file_selector(self) -> None:
        filename, _ = QtWidgets.QFileDialog.getOpenFileName(
            self, "Select a file to import simulation runs from", str(Path.home()), "Json files (*.json)"
        )

        if not filename:
            return

        optional_curr_active_tab_widget: QtWidgets.QWidget | None = self._simulation_runs_tab_widget.widget(
            self._simulation_runs_tab_widget.currentIndex()
        )
        optional_selected_filename_lbl: QtWidgets.QWidget | None = (
            optional_curr_active_tab_widget.findChild(QtWidgets.QLabel, LOADED_FROM_FILE_INPUT_FIELD_NAME)
            if optional_curr_active_tab_widget is not None
            else None
        )
        optional_load_from_file_btn: QtWidgets.QWidget | None = (
            optional_curr_active_tab_widget.findChild(QtWidgets.QPushButton, IMPORT_FROM_FILE_BUTTON_NAME)
            if optional_curr_active_tab_widget is not None
            else None
        )

        if not assert_all_required_widgets_found_or_close_dialog(
            error_notification_parent_widget=self,
            required_widgets=[
                optional_curr_active_tab_widget,
                optional_selected_filename_lbl,
                optional_load_from_file_btn,
            ],
            error_dialog_content="Failed to locate required QtWidgets in open import file handle",
        ):
            return

        selected_filename_lbl: Final[QtWidgets.QLabel] = cast("QtWidgets.QLabel", optional_selected_filename_lbl)
        load_from_file_btn: Final[QtWidgets.QPushButton] = cast("QtWidgets.QPushButton", optional_load_from_file_btn)

        selected_filename_lbl.setText(filename)
        load_from_file_btn.setEnabled(True)

    @QtCore.pyqtSlot()  # type: ignore[untyped-decorator]
    def open_import_from_file_dialog(self) -> None:
        if self._simulation_run_import_from_file_dialog is not None:
            show_and_request_ok_in_optionally_cancellable_notification(
                message_box_type=MessageBoxType.ERROR,
                message_box_parent=self,
                message_box_title="Simulation run import dialog initialization error!",
                message_box_content="Expected no simulation run import dialog instance to exist.",
                is_cancellable=False,
                log_contents=True,
            )
            return

        optional_curr_active_tab_widget: QtWidgets.QWidget | None = self._simulation_runs_tab_widget.widget(
            self._simulation_runs_tab_widget.currentIndex()
        )
        optional_selected_filename_lbl: QtWidgets.QWidget | None = (
            optional_curr_active_tab_widget.findChild(QtWidgets.QLabel, LOADED_FROM_FILE_INPUT_FIELD_NAME)
            if optional_curr_active_tab_widget is not None
            else None
        )

        if not assert_all_required_widgets_found_or_close_dialog(
            error_notification_parent_widget=self,
            required_widgets=[optional_curr_active_tab_widget, optional_selected_filename_lbl],
            error_dialog_content="Failed to locate required QtWidgets on import simulation runs from file click",
        ):
            return

        selected_filename_lbl: Final[QtWidgets.QLabel] = cast("QtWidgets.QLabel", optional_selected_filename_lbl)
        curr_active_tab_widget: Final[QtWidgets.QTabWidget] = cast(
            "QtWidgets.QTabWidget", optional_curr_active_tab_widget
        )
        if self._simulation_runs_model.rowCount(QtCore.QModelIndex()) > 0:
            if not show_and_request_ok_in_optionally_cancellable_notification(
                message_box_type=MessageBoxType.WARNING,
                message_box_parent=self,
                message_box_title="Existing simulation runs detected",
                message_box_content="Importing from a file will delete any existing simulation runs. Do you want to continue?",
                is_cancellable=True,
                log_contents=False,
            ):
                return
            self._clear_simulation_run_list_and_backing_model(curr_active_tab_widget)

        self._simulation_run_import_from_file_dialog = SimulationRunJsonImportDialog(
            parent=self, shared_simulation_runs_model=self._simulation_runs_model
        )
        self._simulation_run_import_from_file_dialog.finished.connect(self.handle_import_from_file_dialog_close)
        self._simulation_run_import_from_file_dialog.show()
        self._simulation_run_import_from_file_dialog.start_import(
            Path(selected_filename_lbl.text()),
            expected_input_state_size=self._expected_input_output_state_size,
        )

    @QtCore.pyqtSlot(int)  # type: ignore[untyped-decorator]
    def handle_import_from_file_dialog_close(self, result: int) -> None:
        self._simulation_run_import_from_file_dialog = None

        optional_curr_active_tab_widget: QtWidgets.QWidget | None = self._simulation_runs_tab_widget.widget(
            self._simulation_runs_tab_widget.currentIndex()
        )

        optional_sim_run_exec_mode_dropdown: QtWidgets.QComboBox | None = (
            optional_curr_active_tab_widget.findChild(QtWidgets.QComboBox, SIM_RUN_EXECUTION_MODE_DROPDOWN_NAME)
            if optional_curr_active_tab_widget is not None
            else None
        )

        if not assert_all_required_widgets_found_or_close_dialog(
            error_notification_parent_widget=self,
            required_widgets=[optional_curr_active_tab_widget, optional_sim_run_exec_mode_dropdown],
            error_dialog_content="Failed to locate required QtWidgets in import simulation runs from file dialog close handler",
        ):
            return

        curr_active_tab_widget: Final[QtWidgets.QWidget] = cast("QtWidgets.QWidget", optional_curr_active_tab_widget)
        sim_run_exec_mode_dropdown: Final[QtWidgets.QComboBox] = cast(
            "QtWidgets.QComboBox", optional_sim_run_exec_mode_dropdown
        )
        curr_sim_run_exec_mode: Final[SimulationRunExecutionMode | None] = sim_run_exec_mode_dropdown.currentData()
        should_simulation_run_execution_controls_be_enabled: Final[bool] = (
            result == QtWidgets.QDialog.DialogCode.Accepted
            and curr_sim_run_exec_mode is not None
            and curr_sim_run_exec_mode != SimulationRunExecutionMode.RUN_SINGLE
        )

        self.set_enabled_state_of_simulation_run_execution_controls_in_tab_widget(
            curr_active_tab_widget, should_simulation_run_execution_controls_be_enabled
        )

        optional_add_sim_run_btn: QtWidgets.QWidget | None = curr_active_tab_widget.findChild(
            QtWidgets.QPushButton, ADD_SIM_RUN_BTN_NAME
        )
        if not assert_all_required_widgets_found_or_close_dialog(
            error_notification_parent_widget=self,
            required_widgets=[optional_add_sim_run_btn],
            error_dialog_content="Failed to locate required QtWidgets in import simulation runs from file dialog close handler",
        ):
            return

        add_sim_run_btn: Final[QtWidgets.QPushButton] = cast("QtWidgets.QPushButton", optional_add_sim_run_btn)
        add_sim_run_btn.setEnabled(result == QtWidgets.QDialog.DialogCode.Accepted)

    def set_enabled_state_of_simulation_run_execution_controls_in_tab_widget(
        self, tab_widget: QtWidgets.QWidget, should_controls_be_enabled: bool
    ) -> None:
        optional_run_simulation_runs_btn: QtWidgets.QPushButton | None = tab_widget.findChild(
            QtWidgets.QPushButton, SIM_RUN_EXECUTION_TRIGGER_BTN_NAME
        )

        optional_sim_run_exec_mode_dropdown: QtWidgets.QComboBox | None = tab_widget.findChild(
            QtWidgets.QComboBox, SIM_RUN_EXECUTION_MODE_DROPDOWN_NAME
        )
        optional_save_simulation_runs_to_file_btn: QtWidgets.QPushButton | None = tab_widget.findChild(
            QtWidgets.QPushButton, SAVE_SIM_RUNS_TO_FILE_BTN_NAME
        )

        if not assert_all_required_widgets_found_or_close_dialog(
            error_notification_parent_widget=self,
            required_widgets=[
                optional_run_simulation_runs_btn,
                optional_sim_run_exec_mode_dropdown,
                optional_save_simulation_runs_to_file_btn,
            ],
            error_dialog_content="Failed to locate required QtWidgets during change of enabled state of simulation run execution controls",
        ):
            return

        run_simulation_runs_btn: Final[QtWidgets.QPushButton] = cast(
            "QtWidgets.QPushButton", optional_run_simulation_runs_btn
        )
        sim_run_exec_mode_dropdown: Final[QtWidgets.QComboBox] = cast(
            "QtWidgets.QComboBox", optional_sim_run_exec_mode_dropdown
        )
        save_simulation_runs_to_file_btn: Final[QtWidgets.QPushButton] = cast(
            "QtWidgets.QPushButton", optional_save_simulation_runs_to_file_btn
        )

        run_simulation_runs_btn.setEnabled(should_controls_be_enabled)
        sim_run_exec_mode_dropdown.setEnabled(should_controls_be_enabled)
        save_simulation_runs_to_file_btn.setEnabled(
            should_controls_be_enabled and not self._did_syrec_program_contain_comments
        )

    def set_enabled_state_of_simulation_run_execution_controls_in_tab_widget_based_on_sim_run_selection_status(
        self, tab_widget: QtWidgets.QWidget, is_simulation_run_selected: bool
    ) -> None:
        optional_run_simulation_runs_btn: QtWidgets.QPushButton | None = tab_widget.findChild(
            QtWidgets.QPushButton, SIM_RUN_EXECUTION_TRIGGER_BTN_NAME
        )

        optional_sim_run_exec_mode_dropdown: QtWidgets.QComboBox | None = tab_widget.findChild(
            QtWidgets.QComboBox, SIM_RUN_EXECUTION_MODE_DROPDOWN_NAME
        )
        optional_save_simulation_runs_to_file_btn: QtWidgets.QPushButton | None = tab_widget.findChild(
            QtWidgets.QPushButton, SAVE_SIM_RUNS_TO_FILE_BTN_NAME
        )

        if not assert_all_required_widgets_found_or_close_dialog(
            error_notification_parent_widget=self,
            required_widgets=[
                optional_run_simulation_runs_btn,
                optional_sim_run_exec_mode_dropdown,
                optional_save_simulation_runs_to_file_btn,
            ],
            error_dialog_content="Failed to locate required QtWidgets during change of enabled state of simulation run execution controls",
        ):
            return

        run_simulation_runs_btn: Final[QtWidgets.QPushButton] = cast(
            "QtWidgets.QPushButton", optional_run_simulation_runs_btn
        )
        sim_run_exec_mode_dropdown: Final[QtWidgets.QComboBox] = cast(
            "QtWidgets.QComboBox", optional_sim_run_exec_mode_dropdown
        )
        save_simulation_runs_to_file_btn: Final[QtWidgets.QPushButton] = cast(
            "QtWidgets.QPushButton", optional_save_simulation_runs_to_file_btn
        )

        curr_sim_run_exec_mode: Final[SimulationRunExecutionMode | None] = sim_run_exec_mode_dropdown.currentData()
        if curr_sim_run_exec_mode is None:
            show_and_request_ok_in_optionally_cancellable_notification(
                message_box_type=MessageBoxType.ERROR,
                message_box_parent=self,
                message_box_title="Failed to determine simulation run execution mode",
                message_box_content="Failed to determine simulation run execution mode while changing enabled state of simulation run execution controls.",
                is_cancellable=True,
                log_contents=False,
            )
            return

        match curr_sim_run_exec_mode:
            case SimulationRunExecutionMode.RUN_ALL | SimulationRunExecutionMode.RUN_ALL_STOP_AT_FIRST_FAILURE:
                run_simulation_runs_btn.setEnabled(not is_simulation_run_selected)
            case SimulationRunExecutionMode.RUN_SINGLE:
                run_simulation_runs_btn.setEnabled(is_simulation_run_selected)
            case _:
                # Added guard to handle new simulation run execution modes
                assert_never(curr_sim_run_exec_mode)

        sim_run_exec_mode_dropdown.setEnabled(not is_simulation_run_selected)
        save_simulation_runs_to_file_btn.setEnabled(
            not self._did_syrec_program_contain_comments
            and not is_simulation_run_selected
            and self._simulation_runs_model.rowCount(QtCore.QModelIndex()) > 0
        )

    def set_default_simulation_run_modification_buttons_enabled_state(
        self, associated_tab_widget: QtWidgets.QTabWidget
    ) -> None:
        optional_curr_active_tab_widget: QtWidgets.QWidget | None = self._simulation_runs_tab_widget.widget(
            self._simulation_runs_tab_widget.currentIndex()
        )

        optional_add_sim_run_btn: QtWidgets.QWidget | None = (
            optional_curr_active_tab_widget.findChild(QtWidgets.QPushButton, ADD_SIM_RUN_BTN_NAME)
            if optional_curr_active_tab_widget is not None
            else None
        )

        optional_edit_sim_run_btn: QtWidgets.QWidget | None = (
            optional_curr_active_tab_widget.findChild(QtWidgets.QPushButton, EDIT_SIM_RUN_BTN_NAME)
            if optional_curr_active_tab_widget is not None
            else None
        )

        optional_delete_sim_run_btn: QtWidgets.QWidget | None = (
            optional_curr_active_tab_widget.findChild(QtWidgets.QPushButton, DELETE_SIM_RUN_BTN_NAME)
            if optional_curr_active_tab_widget is not None
            else None
        )

        if not assert_all_required_widgets_found_or_close_dialog(
            error_notification_parent_widget=self,
            required_widgets=[
                optional_curr_active_tab_widget,
                optional_add_sim_run_btn,
                optional_edit_sim_run_btn,
                optional_delete_sim_run_btn,
            ],
            error_dialog_content="Failed to locate required QtWidgets during switch back to previous tab widget",
        ):
            return

        add_sim_run_btn: Final[QtWidgets.QPushButton] = cast("QtWidgets.QPushButton", optional_add_sim_run_btn)
        edit_sim_run_btn: Final[QtWidgets.QPushButton] = cast("QtWidgets.QPushButton", optional_edit_sim_run_btn)
        delete_sim_run_btn: Final[QtWidgets.QPushButton] = cast("QtWidgets.QPushButton", optional_delete_sim_run_btn)

        add_sim_run_btn.setEnabled(associated_tab_widget.objectName() != LOAD_SIM_RUNS_FROM_FILE_TAB_WIDGET_NAME)
        edit_sim_run_btn.setEnabled(False)
        delete_sim_run_btn.setEnabled(False)

    @QtCore.pyqtSlot(int)  # type: ignore[untyped-decorator]
    def handle_simulation_run_execution_mode_selection_change(self, selected_sim_run_index: int) -> None:
        self._shared_selected_sim_run_execution_mode_dropdown_index = selected_sim_run_index
        optional_curr_active_tab_widget: QtWidgets.QWidget | None = self._simulation_runs_tab_widget.widget(
            self._simulation_runs_tab_widget.currentIndex()
        )

        optional_simulation_runs_list_view: QtWidgets.QWidget | None = (
            optional_curr_active_tab_widget.findChild(QtWidgets.QListView, SIMULATION_RUNS_LIST_VIEW_NAME)
            if optional_curr_active_tab_widget is not None
            else None
        )

        if not assert_all_required_widgets_found_or_close_dialog(
            error_notification_parent_widget=self,
            required_widgets=[optional_curr_active_tab_widget, optional_simulation_runs_list_view],
            error_dialog_content="Failed to locate all required QtWidgets in simulation run execution mode selection change handler",
        ):
            return

        curr_active_tab_widget: Final[QtWidgets.QWidget] = cast("QtWidgets.QWidget", optional_curr_active_tab_widget)
        simulation_runs_list_view: Final[QtWidgets.QListView] = cast(
            "QtWidgets.QListView", optional_simulation_runs_list_view
        )
        self.set_enabled_state_of_simulation_run_execution_controls_in_tab_widget_based_on_sim_run_selection_status(
            curr_active_tab_widget, len(simulation_runs_list_view.selectedIndexes()) == 1
        )

    def _clear_simulation_run_list_and_backing_model(
        self, tab_widget_containing_list_view: QtWidgets.QTabWidget
    ) -> None:
        optional_simulation_runs_list_view: QtWidgets.QWidget | None = tab_widget_containing_list_view.findChild(
            QtWidgets.QListView, SIMULATION_RUNS_LIST_VIEW_NAME
        )

        if not assert_all_required_widgets_found_or_close_dialog(
            error_notification_parent_widget=self,
            required_widgets=[optional_simulation_runs_list_view],
            error_dialog_content="Failed to locate all required simulation run list view widget during reset of simulation run list model",
        ):
            return

        simulation_runs_list_view: Final[QtWidgets.QListView] = cast(
            "QtWidgets.QListView", optional_simulation_runs_list_view
        )
        selection_prior_to_model_reset: Final[QtCore.QItemSelection] = (
            simulation_runs_list_view.selectionModel().selection()
        )

        # During the model reset the beginModelReset emitted by the simulation runs model will disconnect the associated QListView from the selection model,
        # after which all elements in the model will be deleted followed by a reconnect of the QListView to the selection model (by the endModelReset signal of the model).
        # When the model is reset, the views selection model throws away every index that became invalid (see https://doc.qt.io/qt-6/qabstractitemmodel.html#modelReset), so after endResetModel() the selection is already empty;
        # Since the selection is already empty, manually resetting the selection of the QListView (via clearSelection() and setCurrentIndex(QtCore.QModelIndex())) will be silently ignored since the selection is already empty thus
        # the connected slot for the selectionChanged signal of the QListView is not invoked and must be invoked manually.
        self._simulation_runs_model.delete_all_simulation_run_models()

        selection_after_model_reset: Final[QtCore.QItemSelection] = (
            simulation_runs_list_view.selectionModel().selection()
        )
        # To tab changed signal is emitted after the tab already changed thus the QTabWidget.currentWidget() will return the already switched to tab widget while we want to reset the selection
        # in the switched from tab widget.
        self.handle_simulation_run_selection_change(
            selection_after_model_reset,
            selection_prior_to_model_reset,
            optional_tab_widget_to_apply_selection_change_to=tab_widget_containing_list_view,
        )

    # This method is only a temporary workaround for the quantum registers created for ancillary qubits not being marked as ancillary in the annotatable quantum computation (Date of comment 04.02.2026)
    @staticmethod
    def _determine_num_non_ancillary_qubits(
        annotatable_quantum_computation: AnnotatableQuantumComputation, potential_error_dialog_parent: QtWidgets.QWidget
    ) -> int:
        num_non_ancillary_qubits: int = 0
        for qubit in range(annotatable_quantum_computation.num_data_qubits):
            fetched_qubit_label: str | None = annotatable_quantum_computation.get_qubit_label(
                qubit, QubitLabelType.internal
            )
            if fetched_qubit_label is None:
                show_and_request_ok_in_optionally_cancellable_notification(
                    message_box_type=MessageBoxType.ERROR,
                    message_box_parent=potential_error_dialog_parent,
                    message_box_title="Failed to determine qubit label",
                    message_box_content=f"Failed to determine internal qubit label for qubit {qubit}!",
                    is_cancellable=False,
                )
                return 0
            num_non_ancillary_qubits += int(
                not QuantumCircuitSimulationDialog._does_qubit_label_start_with_internal_qubit_label_prefix(
                    fetched_qubit_label
                )
            )
        return num_non_ancillary_qubits

    # This method is only a temporary workaround for the quantum registers created for ancillary qubits not being marked as ancillary in the annotatable quantum computation (Date of comment 04.02.2026)
    @staticmethod
    def _does_qubit_label_start_with_internal_qubit_label_prefix(qubit_label: str) -> bool:
        return qubit_label.startswith("__q")
