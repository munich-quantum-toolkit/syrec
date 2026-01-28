# Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
# Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

from __future__ import annotations

import sys
from pathlib import Path
from typing import Final, cast

from PyQt6 import QtCore, QtGui, QtWidgets

from mqt import syrec

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

LOADED_FROM_FILE_INPUT_FIELD_NAME: Final[str] = "load_from_file_input_field"
IMPORT_FROM_FILE_BUTTON_NAME: Final[str] = "import_from_file_btn"
ADD_SIM_RUN_BTN_NAME: Final[str] = "add_sim_run_btn"
EDIT_SIM_RUN_BTN_NAME: Final[str] = "edit_sim_run_btn"
DELETE_SIM_RUN_BTN_NAME: Final[str] = "delete_sim_run_btn"
SAVE_SIM_RUNS_TO_FILE_BTN_NAME: Final[str] = "save_sims_to_file_btn"
RUN_SIM_RUNS_BTN_NAME: Final[str] = "run_sims_btn"
RUN_SIM_RUNS_BTN_STOP_AT_FIRST_FAILURE_NAME: Final[str] = "run_sims_stop_first_failure_btn"
SIMULATION_RUNS_LIST_VIEW_NAME: Final[str] = "sim_runs_list_view"

IMPORT_FROM_FILE_NO_FILE_SELECTED_PLACEHOLDER_TEXT: Final[str] = "<NONE>"


class QuantumCircuitSimulationDialog(QtWidgets.QDialog):  # type: ignore[misc]
    def __init__(
        self,
        associated_stringified_syrec_program: str,
        annotatable_quantum_computation: syrec.annotatable_quantum_computation,
        parent: QtWidgets.QWidget,
    ) -> None:
        super().__init__()
        self.parent = parent
        self.associated_stringified_syrec_program = associated_stringified_syrec_program
        self.annotatable_quantum_computation = annotatable_quantum_computation
        self.some_sim_runs_tab_widget_name = "some_sim_runs_tab"
        self.all_sim_runs_tab_widget_name = "all_sim_runs_tab"
        self.load_sim_runs_from_file_tab_widget_name = "load_sim_runs_from_file_tab"

        self.title = "Define simulation runs for quantum computation"
        self.setWindowTitle(self.title)
        self.setModal(True)

        dialog_size: Final[QtCore.QSize] = BaseProgressDialog.get_default_big_dialog_size()
        center_dialog_pos_for_size: Final[QtCore.QPoint] = BaseProgressDialog.get_center_screen_position_for_size(
            dialog_size
        )
        self.setGeometry(
            center_dialog_pos_for_size.x(), center_dialog_pos_for_size.y(), dialog_size.width(), dialog_size.height()
        )

        self.simulation_run_editor_dialog: SimulationRunEditorDialog | None = None
        self.all_input_states_generator_dialog: AllInputStatesGeneratorDialog | None = None
        self.simulation_run_import_from_file_dialog: SimulationRunJsonImportDialog | None = None
        self.simulation_run_export_to_file_dialog: SimulationRunJsonExportDialog | None = None
        self.expected_input_output_state_size: Final[int] = annotatable_quantum_computation.num_data_qubits
        self.simulation_runs_model: QtSimulationRunModel = QtSimulationRunModel(annotatable_quantum_computation, self)
        self.simulation_run_dialog: SimulationRunDialog | None = None

        self.prev_active_simulation_runs_tab_idx: int = 0
        self.simulation_runs_tab_widget = QtWidgets.QTabWidget(self)
        self.simulation_runs_tab_widget.currentChanged.connect(self.handle_simulation_runs_tab_widget_tab_changed)
        self.simulation_runs_tab_widget.addTab(
            self.initialize_simulation_runs_tab_widget(self.simulation_runs_model, self.some_sim_runs_tab_widget_name),
            "Check some input-output mapping combinations",
        )
        self.simulation_runs_tab_widget.addTab(
            self.initialize_simulation_runs_tab_widget(self.simulation_runs_model, self.all_sim_runs_tab_widget_name),
            "Check all input-output mapping combinations",
        )
        self.simulation_runs_tab_widget.addTab(
            self.initialize_simulation_runs_tab_widget(
                self.simulation_runs_model,
                self.load_sim_runs_from_file_tab_widget_name,
                create_load_from_file_controls=True,
            ),
            "Check input-output mapping combinations from file",
        )

        self.layout = QtWidgets.QVBoxLayout()
        self.layout.addWidget(self.simulation_runs_tab_widget)
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
        simulation_runs_list_view: QtWidgets.QListView = QtWidgets.QListView(objectName=SIMULATION_RUNS_LIST_VIEW_NAME)
        simulation_runs_list_view.setModel(shared_simulation_runs_model)
        simulation_runs_list_view.setItemDelegate(SimulationRunOverviewStyledItemDelegate())  # type: ignore[no-untyped-call]
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
        simulation_runs_execution_buttons_layout.addWidget(save_simulation_runs_to_file_button)

        run_simulation_runs_button = QtWidgets.QPushButton(
            QtGui.QIcon.fromTheme(QtGui.QIcon.ThemeIcon.MediaPlaybackStart),
            "Run simulation runs",
            objectName=RUN_SIM_RUNS_BTN_NAME,
        )
        run_simulation_runs_button.setEnabled(False)
        run_simulation_runs_button.clicked.connect(self.handle_run_all_simulation_runs_button_click)
        simulation_runs_execution_buttons_layout.addWidget(run_simulation_runs_button)

        run_simulation_runs_stop_at_first_failure_button = QtWidgets.QPushButton(
            QtGui.QIcon.fromTheme(QtGui.QIcon.ThemeIcon.MediaPlaybackStart),
            "Run simulation runs (stop at first output qubit values mismatch)",
            objectName=RUN_SIM_RUNS_BTN_STOP_AT_FIRST_FAILURE_NAME,
        )
        run_simulation_runs_stop_at_first_failure_button.setEnabled(False)
        run_simulation_runs_stop_at_first_failure_button.clicked.connect(
            self.handle_run_all_simulation_runs_stop_at_first_failure_button_click
        )
        run_simulation_runs_stop_at_first_failure_button.setToolTip(
            "Perform a simulation of all defined simulation runs until a mismatch between the expected and actual output state qubit values is detected (the value of both output states needs to be known)"
        )
        simulation_runs_execution_buttons_layout.addWidget(run_simulation_runs_stop_at_first_failure_button)

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
    def reject(self) -> None:
        if self.show_close_confirmation_dialog_and_return_boolean_user_choice():
            super().reject()

    def closeEvent(self, event: QtGui.QCloseEvent) -> None:  # noqa: N802
        # Ask for confirmation before closing
        self.accept() if self.show_close_confirmation_dialog_and_return_boolean_user_choice() else event.ignore()

    def handle_simulation_run_selection_change(
        self, selected: QtCore.QItemSelection, deselected: QtCore.QItemSelection
    ) -> None:
        if selected.isEmpty() == deselected.isEmpty():
            return

        optional_curr_active_tab_widget: QtWidgets.QWidget | None = self.simulation_runs_tab_widget.currentWidget()
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
        self.set_enabled_state_of_simulation_run_execution_controls_in_tab_widget(
            curr_active_tab_widget,
            not is_list_item_selected and (self.simulation_runs_model.rowCount(QtCore.QModelIndex()) < sys.maxsize),
        )

    def handle_simulation_run_add_btn_click(self) -> None:
        if not self.simulation_runs_model.add_simulation_run_model(
            SimulationRunModel(
                input_state=syrec.n_bit_values_container(self.expected_input_output_state_size),
                expected_output_state=None,
            )
        ):
            return

        optional_curr_active_tab_widget: QtWidgets.QWidget | None = self.simulation_runs_tab_widget.currentWidget()
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

    def handle_simulation_run_edit_btn_click(self) -> None:
        optional_curr_active_tab_widget: QtWidgets.QWidget | None = self.simulation_runs_tab_widget.currentWidget()
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

        cast("QtWidgets.QWidget", optional_curr_active_tab_widget)
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

        self.simulation_run_editor_dialog = SimulationRunEditorDialog(
            simulation_runs_list_view.currentIndex(), copy_of_reference_sim_run_model, self
        )
        self.simulation_run_editor_dialog.finished.connect(self.handle_simulation_run_editor_dialog_close)
        self.simulation_run_editor_dialog.show()

    def handle_simulation_run_editor_dialog_close(self, result: int) -> None:
        # This should not happen but is checked nevertheless
        if self.simulation_run_editor_dialog is None or result == QtWidgets.QDialog.DialogCode.Rejected:
            return

        try:
            self.simulation_runs_model.update_edited_simulation_run_model(
                self.simulation_run_editor_dialog.simulation_run_model_index,
                self.simulation_run_editor_dialog.edited_simulation_run_model,
            )
        except ValueError as err:
            show_and_request_ok_in_optionally_cancellable_notification(
                message_box_type=MessageBoxType.ERROR,
                message_box_parent=self,
                message_box_title="Simulation run model update error!",
                message_box_content=f"Update of simulation run model {self.simulation_run_editor_dialog.simulation_run_model_index.row()} failed due to an error!\nReason: {err}",
                is_cancellable=False,
                log_contents=False,
            )
        finally:
            self.simulation_run_editor_dialog = None

    @QtCore.pyqtSlot()  # type: ignore[untyped-decorator]
    def handle_sim_run_save_to_file_btn_click(self) -> None:
        if self.simulation_run_export_to_file_dialog is not None:
            show_and_request_ok_in_optionally_cancellable_notification(
                message_box_type=MessageBoxType.ERROR,
                message_box_parent=self,
                message_box_title="Simulation run export dialog initialization error!",
                message_box_content="Expected no simulation run export dialog instance to exist.",
                is_cancellable=False,
                log_contents=True,
            )
            return

        filename, _ = QtWidgets.QFileDialog.getOpenFileName(
            self, "Select a file to export simulation runs to", str(Path.home()), "Json files (*.json)"
        )

        if not filename:
            return

        self.simulation_run_export_to_file_dialog = SimulationRunJsonExportDialog(self)
        self.simulation_run_export_to_file_dialog.finished.connect(self.handle_sim_run_export_to_file_dialog_close)
        self.simulation_run_export_to_file_dialog.start_export(
            Path(filename),
            self.associated_stringified_syrec_program,
            self.simulation_runs_model.get_all_simulation_run_models(),
            self.simulation_runs_model.rowCount(QtCore.QModelIndex()),
        )
        self.simulation_run_export_to_file_dialog.show()

    @QtCore.pyqtSlot(int)  # type: ignore[untyped-decorator]
    def handle_sim_run_export_to_file_dialog_close(self, _: int) -> None:
        self.simulation_run_export_to_file_dialog = None

    def handle_open_and_start_all_input_states_generator_dialog(self, input_state_size: int) -> None:
        if self.all_input_states_generator_dialog is not None:
            show_and_request_ok_in_optionally_cancellable_notification(
                message_box_type=MessageBoxType.ERROR,
                message_box_parent=self,
                message_box_title="Input states generator dialog initialization error!",
                message_box_content="Expected no input states generator dialog instance to exist.",
                is_cancellable=False,
                log_contents=True,
            )
            return

        self.all_input_states_generator_dialog = AllInputStatesGeneratorDialog(self)
        self.all_input_states_generator_dialog.finished.connect(self.handle_input_states_generator_dialog_close)
        self.all_input_states_generator_dialog.show()
        self.all_input_states_generator_dialog.start_generation(self.simulation_runs_model, input_state_size)

    def handle_input_states_generator_dialog_close(self, result: int) -> None:
        self.all_input_states_generator_dialog = None

        optional_curr_active_tab_widget: QtWidgets.QWidget | None = self.simulation_runs_tab_widget.currentWidget()
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

    def handle_simulation_run_delete_btn_click(self) -> None:
        optional_curr_active_tab_widget: QtWidgets.QWidget | None = self.simulation_runs_tab_widget.currentWidget()
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

        if not self.simulation_runs_model.delete_simulation_run_model(simulation_runs_list_view.currentIndex()):
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

    def handle_simulation_runs_tab_widget_tab_changed(self, switched_to_tab_index: int) -> None:
        if switched_to_tab_index == -1:
            return

        if switched_to_tab_index == self.prev_active_simulation_runs_tab_idx:
            return

        optional_prev_active_tab_widget: QtWidgets.QWidget | None = self.simulation_runs_tab_widget.widget(
            self.prev_active_simulation_runs_tab_idx
        )
        optional_to_be_switched_to_tab_widget: QtWidgets.QWidget | None = self.simulation_runs_tab_widget.widget(
            switched_to_tab_index
        )
        if not assert_all_required_widgets_found_or_close_dialog(
            error_notification_parent_widget=self,
            required_widgets=[optional_prev_active_tab_widget, optional_to_be_switched_to_tab_widget],
            error_dialog_content="Failed to locate previous/current active tab widget during simulation run tab change handler",
        ):
            if optional_to_be_switched_to_tab_widget is None:
                self.simulation_runs_tab_widget.setCurrentIndex(self.prev_active_simulation_runs_tab_idx)
            return

        prev_active_tab_widget: Final[QtWidgets.QLabel] = cast("QtWidgets.QWidget", optional_prev_active_tab_widget)
        to_be_switched_to_tab_widget: Final[QtWidgets.QLabel] = cast(
            "QtWidgets.QWidget", optional_to_be_switched_to_tab_widget
        )

        if self.simulation_runs_model.rowCount(QtCore.QModelIndex()) > 0:
            if not show_and_request_ok_in_optionally_cancellable_notification(
                message_box_type=MessageBoxType.WARNING,
                message_box_parent=self,
                message_box_title="Existing simulation runs detected!",
                message_box_content="Switching tabs will delete all existing simulation runs. Do you want to continue?",
                is_cancellable=True,
                log_contents=False,
            ):
                self.simulation_runs_tab_widget.setCurrentIndex(self.prev_active_simulation_runs_tab_idx)
                return
            self.simulation_runs_model.delete_all_simulation_run_models()

        self.set_enabled_state_of_simulation_run_execution_controls_in_tab_widget(to_be_switched_to_tab_widget, False)

        if to_be_switched_to_tab_widget.objectName() == self.all_sim_runs_tab_widget_name:
            n_input_state_combinations: int = 2**self.annotatable_quantum_computation.num_data_qubits
            if not show_and_request_ok_in_optionally_cancellable_notification(
                message_box_type=MessageBoxType.WARNING,
                message_box_parent=self,
                message_box_title="Generating all possible input state combinations!",
                message_box_content=f"Are you sure that you want to generate {n_input_state_combinations} simulation runs, one for each input state combination?",
                is_cancellable=True,
                log_contents=False,
            ):
                self.simulation_runs_tab_widget.setCurrentIndex(self.prev_active_simulation_runs_tab_idx)
                return

            self.handle_open_and_start_all_input_states_generator_dialog(
                self.annotatable_quantum_computation.num_data_qubits
            )
        self.set_enabled_state_of_simulation_run_execution_controls_in_tab_widget(prev_active_tab_widget, False)
        self.prev_active_simulation_runs_tab_idx = switched_to_tab_index

    def handle_run_all_simulation_runs_button_click(self) -> None:
        self.open_simulation_runs_execution_dialog(stop_at_first_output_state_mismatch=False)

    def handle_run_all_simulation_runs_stop_at_first_failure_button_click(self) -> None:
        self.open_simulation_runs_execution_dialog(stop_at_first_output_state_mismatch=True)

    def open_simulation_runs_execution_dialog(self, stop_at_first_output_state_mismatch: bool) -> None:
        if self.simulation_run_dialog is not None:
            show_and_request_ok_in_optionally_cancellable_notification(
                message_box_type=MessageBoxType.ERROR,
                message_box_parent=self,
                message_box_title="Simulation run dialog initialization error!",
                message_box_content="Expected no simulation run dialog instance to exist.",
                is_cancellable=False,
                log_contents=True,
            )
            return

        self.simulation_run_dialog = SimulationRunDialog(self)
        self.simulation_run_dialog.finished.connect(self.handle_simulation_runs_dialog_close)
        self.simulation_run_dialog.show()
        self.simulation_run_dialog.start_simulations(
            self.annotatable_quantum_computation, self.simulation_runs_model, stop_at_first_output_state_mismatch
        )

    def handle_simulation_runs_dialog_close(self, _: int) -> None:
        self.simulation_run_dialog = None

    @QtCore.pyqtSlot()  # type: ignore[untyped-decorator]
    def open_import_file_selector(self) -> None:
        filename, _ = QtWidgets.QFileDialog.getOpenFileName(
            self, "Select a file to import simulation runs from", str(Path.home()), "Json files (*.json)"
        )

        if not filename:
            return

        optional_curr_active_tab_widget: QtWidgets.QWidget | None = self.simulation_runs_tab_widget.widget(
            self.simulation_runs_tab_widget.currentIndex()
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
        load_from_file_btn: Final[QtWidgets.QLabel] = cast("QtWidgets.QLabel", optional_load_from_file_btn)

        selected_filename_lbl.setText(filename)
        load_from_file_btn.setEnabled(True)

    @QtCore.pyqtSlot()  # type: ignore[untyped-decorator]
    def open_import_from_file_dialog(self) -> None:
        if self.simulation_run_import_from_file_dialog is not None:
            show_and_request_ok_in_optionally_cancellable_notification(
                message_box_type=MessageBoxType.ERROR,
                message_box_parent=self,
                message_box_title="Simulation run import dialog initialization error!",
                message_box_content="Expected no simulation run import dialog instance to exist.",
                is_cancellable=False,
                log_contents=True,
            )
            return

        optional_curr_active_tab_widget: QtWidgets.QWidget | None = self.simulation_runs_tab_widget.widget(
            self.simulation_runs_tab_widget.currentIndex()
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
        if self.simulation_runs_model.rowCount(QtCore.QModelIndex()) > 0:
            if not show_and_request_ok_in_optionally_cancellable_notification(
                message_box_type=MessageBoxType.WARNING,
                message_box_parent=self,
                message_box_title="Existing simulation runs detected",
                message_box_content="Importing from a file will delete any existing simulation runs. Do you want to continue?",
                is_cancellable=True,
                log_contents=False,
            ):
                return
            self.simulation_runs_model.delete_all_simulation_run_models()

        self.simulation_run_import_from_file_dialog = SimulationRunJsonImportDialog(self)
        self.simulation_run_import_from_file_dialog.finished.connect(self.handle_import_from_file_dialog_close)
        self.simulation_run_import_from_file_dialog.show()
        self.simulation_run_import_from_file_dialog.start_generation(
            Path(selected_filename_lbl.text()),
            self.simulation_runs_model,
            expected_input_state_size=self.annotatable_quantum_computation.num_data_qubits,
        )

    @QtCore.pyqtSlot(int)  # type: ignore[untyped-decorator]
    def handle_import_from_file_dialog_close(self, result: int) -> None:
        self.simulation_run_import_from_file_dialog = None

        optional_curr_active_tab_widget: QtWidgets.QWidget | None = self.simulation_runs_tab_widget.widget(
            self.simulation_runs_tab_widget.currentIndex()
        )
        if not assert_all_required_widgets_found_or_close_dialog(
            error_notification_parent_widget=self,
            required_widgets=[optional_curr_active_tab_widget],
            error_dialog_content="Failed to locate active tab widget in import simulation runs from file dialog close handler",
        ):
            return

        curr_active_tab_widget: Final[QtWidgets.QLabel] = cast("QtWidgets.QWidget", optional_curr_active_tab_widget)
        self.set_enabled_state_of_simulation_run_execution_controls_in_tab_widget(
            curr_active_tab_widget, result == QtWidgets.QDialog.DialogCode.Accepted
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

        add_sim_run_btn: Final[QtWidgets.QLabel] = cast("QtWidgets.QPushButton", optional_add_sim_run_btn)
        add_sim_run_btn.setEnabled(result == QtWidgets.QDialog.DialogCode.Accepted)

    def set_enabled_state_of_simulation_run_execution_controls_in_tab_widget(
        self, tab_widget: QtWidgets.QWidget, should_controls_be_enabled: bool
    ) -> None:
        optional_run_simulation_runs_btn: QtWidgets.QPushButton | None = tab_widget.findChild(
            QtWidgets.QPushButton, RUN_SIM_RUNS_BTN_NAME
        )
        optional_run_simulation_runs_stop_at_first_failure_btn: QtWidgets.QPushButton | None = tab_widget.findChild(
            QtWidgets.QPushButton, RUN_SIM_RUNS_BTN_STOP_AT_FIRST_FAILURE_NAME
        )
        optional_save_simulation_runs_to_file_btn: QtWidgets.QPushButton | None = tab_widget.findChild(
            QtWidgets.QPushButton, SAVE_SIM_RUNS_TO_FILE_BTN_NAME
        )

        if not assert_all_required_widgets_found_or_close_dialog(
            error_notification_parent_widget=self,
            required_widgets=[
                optional_run_simulation_runs_btn,
                optional_run_simulation_runs_stop_at_first_failure_btn,
                optional_save_simulation_runs_to_file_btn,
            ],
            error_dialog_content="Failed to locate required QtWidgets during change of enabled state of simulation run execution controls",
        ):
            return

        run_simulation_runs_btn: Final[QtWidgets.QLabel] = cast(
            "QtWidgets.QPushButton", optional_run_simulation_runs_btn
        )
        run_simulation_runs_stop_at_first_failure_btn: Final[QtWidgets.QLabel] = cast(
            "QtWidgets.QPushButton", optional_run_simulation_runs_stop_at_first_failure_btn
        )
        save_simulation_runs_to_file_btn: Final[QtWidgets.QLabel] = cast(
            "QtWidgets.QPushButton", optional_save_simulation_runs_to_file_btn
        )

        run_simulation_runs_btn.setEnabled(should_controls_be_enabled)
        run_simulation_runs_stop_at_first_failure_btn.setEnabled(should_controls_be_enabled)
        save_simulation_runs_to_file_btn.setEnabled(should_controls_be_enabled)
