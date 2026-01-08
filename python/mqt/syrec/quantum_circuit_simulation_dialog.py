# Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
# Copyright (c) 2025 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

from __future__ import annotations

from typing import Final

from PyQt6 import QtCore, QtGui, QtWidgets

from mqt import syrec

from .simulation_view.qt_simulation_run_dialog import SimulationRunDialog
from .simulation_view.qt_simulation_run_editor_dialog import SimulationRunEditorDialog
from .simulation_view.qt_simulation_run_model import QtSimulationRunModel, SimulationRunModel
from .simulation_view.qt_simulation_run_styled_item_delegate import SimulationRunModelStyledItemDelegate

LOADED_FROM_FILE_INPUT_FIELD_NAME: Final[str] = "load_from_file_input_field"
ADD_SIM_RUN_BTN_NAME: Final[str] = "add_sim_run_btn"
EDIT_SIM_RUN_BTN_NAME: Final[str] = "edit_sim_run_btn"
DELETE_SIM_RUN_BTN_NAME: Final[str] = "delete_sim_run_btn"
SAVE_SIM_RUNS_TO_FILE_BTN_NAME: Final[str] = "save_sims_to_file_btn"
RUN_SIM_RUNS_BTN_NAME: Final[str] = "run_sims_btn"
RUN_SIM_RUNS_BTN_STOP_AT_FIRST_FAILURE_NAME: Final[str] = "run_sims_stop_first_failure_btn"
SIMULATION_RUNS_LIST_VIEW_NAME: Final[str] = "sim_runs_list_view"


class QuantumCircuitSimulationDialog(QtWidgets.QDialog):  # type: ignore[misc]
    def __init__(
        self, annotatable_quantum_computation: syrec.annotatable_quantum_computation, parent: QtWidgets.QWidget
    ) -> None:
        super().__init__()
        self.parent = parent
        self.annotatable_quantum_computation = annotatable_quantum_computation
        self.some_sim_runs_tab_widget_name = "some_sim_runs_tab"
        self.all_sim_runs_tab_widget_name = "all_sim_runs_tab"
        self.load_sim_runs_from_file_tab_widget_name = "load_sim_runs_from_file_tab"

        self.title = "Define simulation runs for quantum computation"
        self.setWindowTitle(self.title)

        self.left = 0
        self.top = 0
        self.width = 1200
        self.height = 800
        self.setGeometry(self.left, self.top, self.width, self.height)

        self.simulation_run_editor_dialog: SimulationRunEditorDialog | None = None
        self.expected_input_output_state_size: Final[int] = annotatable_quantum_computation.num_data_qubits
        self.simulation_runs_model: QtSimulationRunModel = QtSimulationRunModel(annotatable_quantum_computation, self)
        self.simulation_run_dialog: SimulationRunDialog | None = None

        # TODO: Default background of tabwidget is white on windows (https://forum.qt.io/topic/82262/default-background-color-of-qtabwidget-and-qwidget-qgroupbox/4)
        self.simulation_runs_tab_widget = QtWidgets.QTabWidget(self)
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
        self.simulation_runs_tab_widget.tabBarClicked.connect(self.handle_simulation_runs_tab_widget_tab_bar_clicked)

        n_simulation_runs_to_add: Final[int] = 10
        QuantumCircuitSimulationDialog.generate_some_simulation_runs(
            n_simulation_runs_to_add, self.annotatable_quantum_computation, self.simulation_runs_model
        )

        self.layout = QtWidgets.QVBoxLayout()
        self.layout.addWidget(self.simulation_runs_tab_widget)
        self.setLayout(self.layout)
        self.setSizeGripEnabled(True)

    # TODO: Load from file controls

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
                QuantumCircuitSimulationDialog.initialize_load_simulation_runs_from_file_controls()
            )
            tab_wrapper_widget_layout.addSpacing(manual_y_space_size)

        # BEGIN: Create simulation runs list view Qt elements
        simulation_runs_list_view: QtWidgets.QListView = QtWidgets.QListView(objectName=SIMULATION_RUNS_LIST_VIEW_NAME)
        simulation_runs_list_view.setModel(shared_simulation_runs_model)
        simulation_runs_list_view.setItemDelegate(SimulationRunModelStyledItemDelegate())  # type: ignore[no-untyped-call]
        simulation_runs_list_view.setUniformItemSizes(True)
        simulation_runs_list_view.setAutoFillBackground(False)
        simulation_runs_list_view.setSpacing(5)
        simulation_runs_list_view.setFlow(QtWidgets.QListView.Flow.TopToBottom)
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
        add_simulation_run_button.setEnabled(True)
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
        save_simulation_runs_to_file_button.setEnabled(False)
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
            "Run simulation runs (stop at first failure)",
            objectName=RUN_SIM_RUNS_BTN_STOP_AT_FIRST_FAILURE_NAME,
        )
        run_simulation_runs_stop_at_first_failure_button.setEnabled(False)
        run_simulation_runs_stop_at_first_failure_button.clicked.connect(
            self.handle_run_all_simulation_runs_stop_at_first_failure_button_click
        )
        simulation_runs_execution_buttons_layout.addWidget(run_simulation_runs_stop_at_first_failure_button)

        simulation_runs_execution_buttons_layout.addStretch()

        tab_wrapper_widget_layout.addSpacing(manual_y_space_size)
        tab_wrapper_widget_layout.addLayout(simulation_runs_execution_buttons_layout)
        # END: Create simulation runs execution Qt elements
        return tab_wrapper_widget

    def handle_simulation_run_selection_change(
        self, selected: QtCore.QItemSelection, deselected: QtCore.QItemSelection
    ) -> None:
        if selected.isEmpty() == deselected.isEmpty():
            return

        curr_active_tab_widget: QtWidgets.QWidget | None = self.simulation_runs_tab_widget.currentWidget()
        if curr_active_tab_widget is None:
            return

        is_list_item_selected: bool = not selected.isEmpty() and deselected.isEmpty()

        add_simulation_run_btn: QtWidgets.QPushButton | None = curr_active_tab_widget.findChild(
            QtWidgets.QPushButton, ADD_SIM_RUN_BTN_NAME
        )
        edit_simulation_run_btn: QtWidgets.QPushButton | None = curr_active_tab_widget.findChild(
            QtWidgets.QPushButton, EDIT_SIM_RUN_BTN_NAME
        )
        delete_simulation_run_btn: QtWidgets.QPushButton | None = curr_active_tab_widget.findChild(
            QtWidgets.QPushButton, DELETE_SIM_RUN_BTN_NAME
        )

        if add_simulation_run_btn is None or edit_simulation_run_btn is None or delete_simulation_run_btn is None:
            return

        add_simulation_run_btn.setEnabled(not is_list_item_selected)
        edit_simulation_run_btn.setEnabled(is_list_item_selected)
        delete_simulation_run_btn.setEnabled(is_list_item_selected)
        self.set_enabled_state_of_simulation_run_execution_controls_in_tab_widget(
            curr_active_tab_widget, not is_list_item_selected
        )

    def handle_simulation_run_add_btn_click(self) -> None:
        if not self.simulation_runs_model.add_simulation_run_model(
            SimulationRunModel(
                input_state=syrec.n_bit_values_container(self.expected_input_output_state_size),
                expected_output_state=None,
            )
        ):
            return

        curr_active_tab_widget: QtWidgets.QWidget | None = self.simulation_runs_tab_widget.currentWidget()
        if curr_active_tab_widget is None:
            return

        QuantumCircuitSimulationDialog.set_enabled_state_of_simulation_run_execution_controls_in_tab_widget(
            curr_active_tab_widget, True
        )

        simulation_runs_list_view: QtWidgets.QListView | None = curr_active_tab_widget.findChild(
            QtWidgets.QListView, SIMULATION_RUNS_LIST_VIEW_NAME
        )
        if simulation_runs_list_view is None:
            return

        simulation_runs_list_view.scrollToBottom()

    def handle_simulation_run_edit_btn_click(self) -> None:
        curr_active_tab_widget: QtWidgets.QWidget | None = self.simulation_runs_tab_widget.currentWidget()
        if curr_active_tab_widget is None:
            return

        simulation_runs_list_view: QtWidgets.QListView | None = curr_active_tab_widget.findChild(
            QtWidgets.QListView, SIMULATION_RUNS_LIST_VIEW_NAME
        )
        if simulation_runs_list_view is None:
            return

        self.simulation_run_editor_dialog = SimulationRunEditorDialog(simulation_runs_list_view.currentIndex(), self)
        self.simulation_run_editor_dialog.finished.connect(self.handle_simulation_run_editor_dialog_close)
        self.simulation_run_editor_dialog.show()

    def handle_simulation_run_editor_dialog_close(self, result: int) -> None:
        # This should not happen but is checked nevertheless
        if self.simulation_run_editor_dialog is None or result == QtWidgets.QDialog.DialogCode.Rejected:
            return

        try:
            self.simulation_runs_model.update_simulation_run_model(
                self.simulation_run_editor_dialog.simulation_run_model_index,
                self.simulation_run_editor_dialog.edited_simulation_run_model,
            )
        except ValueError as err:
            pressed_message_box_button: QtWidgets.QMessageBox.StandardButton = QtWidgets.QMessageBox.critical(
                self,
                "Simulation run model update error!",
                f"Update of simulation run model {self.simulation_run_editor_dialog.simulation_run_model_index.row()} failed due to an error!\nReason: {err}",
                defaultButton=QtWidgets.QMessageBox.StandardButton.Ok,
            )

            if pressed_message_box_button == QtWidgets.QMessageBox.StandardButton:
                pass
        finally:
            self.simulation_run_editor_dialog = None

    def handle_simulation_run_delete_btn_click(self) -> None:
        curr_active_tab_widget: QtWidgets.QWidget | None = self.simulation_runs_tab_widget.currentWidget()
        if curr_active_tab_widget is None:
            return

        simulation_runs_list_view: QtWidgets.QListView | None = curr_active_tab_widget.findChild(
            QtWidgets.QListView, SIMULATION_RUNS_LIST_VIEW_NAME
        )
        if simulation_runs_list_view is None:
            return

        if not self.simulation_runs_model.delete_simulation_run_model(simulation_runs_list_view.currentIndex()):
            return

        # Deletion of an element should only be enabled when an item in the QListView is selected. After the deletion
        # and the subsequent update of the backing model of the QListView selection will switch to the element the index
        # of the previously selected element thus the simulation run execution controls should not be enabled after an element
        # is deleted
        QuantumCircuitSimulationDialog.set_enabled_state_of_simulation_run_execution_controls_in_tab_widget(
            curr_active_tab_widget, False
        )

    @staticmethod
    def initialize_load_simulation_runs_from_file_controls() -> QtWidgets.QLayout:
        controls_layout = QtWidgets.QHBoxLayout()
        controls_layout.addStretch()

        info_label = QtWidgets.QLabel("File to load simulation runs from:")
        controls_layout.addWidget(info_label)

        selected_file_name_input_field = QtWidgets.QLineEdit(objectName=LOADED_FROM_FILE_INPUT_FIELD_NAME)
        selected_file_name_input_field.setEnabled(False)
        controls_layout.addWidget(selected_file_name_input_field)

        open_file_dialog_button = QtWidgets.QPushButton("Select file...")
        controls_layout.addWidget(open_file_dialog_button)

        trigger_load_from_file_button = QtWidgets.QPushButton(
            QtGui.QIcon.fromTheme(QtGui.QIcon.ThemeIcon.DocumentOpen), "Load from file"
        )
        controls_layout.addWidget(trigger_load_from_file_button)

        controls_layout.addStretch()
        return controls_layout

    def generate_some_simulation_runs(
        self: int,
        annotatable_quantum_computation: syrec.annotatable_quantum_computation,
        shared_simulation_runs_model: QtSimulationRunModel,
    ) -> None:
        for i in range(self):
            in_state = syrec.n_bit_values_container(annotatable_quantum_computation.num_data_qubits)
            expected_out_state = syrec.n_bit_values_container(annotatable_quantum_computation.num_data_qubits)

            sim_run_model: SimulationRunModel | None = None
            if i < 2:
                sim_run_model = SimulationRunModel(in_state, None)
            else:
                sim_run_model = SimulationRunModel(in_state, expected_out_state)

            shared_simulation_runs_model.add_simulation_run_model(sim_run_model)

    def handle_simulation_runs_tab_widget_tab_bar_clicked(self, clicked_on_tab_index: int) -> None:
        if self.simulation_runs_tab_widget.currentIndex() == clicked_on_tab_index:
            self.simulation_runs_tab_widget.setCurrentIndex(self.simulation_runs_tab_widget.currentIndex())
            return

        curr_tab_widget: QtWidgets.QWidget | None = self.simulation_runs_tab_widget.widget(
            self.simulation_runs_tab_widget.currentIndex()
        )
        if curr_tab_widget is None:
            return

        if self.simulation_runs_model.rowCount(QtCore.QModelIndex()) > 0:
            pressed_message_box_button_in_tab_switch_warning: QtWidgets.QMessageBox.StandardButton = (
                QtWidgets.QMessageBox.warning(
                    self,
                    "Existing simulation runs detected!",
                    "Switching tabs will delete all existing simulation runs. Do you want to continue?",
                    buttons=QtWidgets.QMessageBox.StandardButton.Ok | QtWidgets.QMessageBox.StandardButton.Cancel,
                    defaultButton=QtWidgets.QMessageBox.StandardButton.Ok,
                )
            )

            if pressed_message_box_button_in_tab_switch_warning == QtWidgets.QMessageBox.StandardButton.Cancel:
                self.simulation_runs_tab_widget.currentIndex(self.simulation_runs_tab_widget.currentIndex())
                return
            self.simulation_runs_model.delete_all_simulation_run_models()

        to_be_switched_to_tab_widget: QtWidgets.QWidget | None = self.simulation_runs_tab_widget.widget(
            clicked_on_tab_index
        )
        if to_be_switched_to_tab_widget is None:
            self.simulation_runs_tab_widget.setCurrentIndex(self.simulation_runs_tab_widget.currentIndex())
            return

        QuantumCircuitSimulationDialog.set_enabled_state_of_simulation_run_execution_controls_in_tab_widget(
            curr_tab_widget, False
        )

        if to_be_switched_to_tab_widget.objectName() == self.all_sim_runs_tab_widget_name:
            n_input_combinations: int = 2**self.annotatable_quantum_computation.num_data_qubits
            pressed_message_box_button_in_all_sim_run_generation_warning: QtWidgets.QMessageBox.StandardButton = QtWidgets.QMessageBox.warning(
                self,
                "Generating all possible input state combinations!",
                f"Are you sure that you want to generate {n_input_combinations} simulation runs, one for each input state combination?",
                buttons=QtWidgets.QMessageBox.StandardButton.Ok | QtWidgets.QMessageBox.StandardButton.Cancel,
                defaultButton=QtWidgets.QMessageBox.StandardButton.Ok,
            )

            if (
                pressed_message_box_button_in_all_sim_run_generation_warning
                == QtWidgets.QMessageBox.StandardButton.Cancel
            ):
                self.simulation_runs_tab_widget.setCurrentIndex(self.simulation_runs_tab_widget.currentIndex())
                return
            QuantumCircuitSimulationDialog.set_enabled_state_of_simulation_run_execution_controls_in_tab_widget(
                to_be_switched_to_tab_widget, True
            )
            # TODO: Can we ignore return value?
            self.simulation_runs_model.add_all_possible_simulation_run_models()

        QuantumCircuitSimulationDialog.set_enabled_state_of_simulation_run_execution_controls_in_tab_widget(
            curr_tab_widget, False
        )

        self.simulation_runs_tab_widget.setCurrentIndex(clicked_on_tab_index)

    def handle_run_all_simulation_runs_button_click(self) -> None:
        self.open_simulation_runs_execution_dialog(stop_at_first_output_state_mismatch=False)

    def handle_run_all_simulation_runs_stop_at_first_failure_button_click(self) -> None:
        self.open_simulation_runs_execution_dialog(stop_at_first_output_state_mismatch=True)

    def open_simulation_runs_execution_dialog(self, stop_at_first_output_state_mismatch: bool) -> None:
        if self.simulation_run_dialog is not None:
            # TODO: Error logging?
            return

        total_num_simulation_runs: Final[int] = 2**self.annotatable_quantum_computation.num_data_qubits
        self.simulation_run_dialog = SimulationRunDialog(self.simulation_runs_model, self)
        self.simulation_run_dialog.finished.connect(self.handle_simulation_runs_dialog_close)
        self.simulation_run_dialog.start_simulations(
            self.annotatable_quantum_computation, total_num_simulation_runs, stop_at_first_output_state_mismatch
        )
        self.simulation_run_dialog.show()

    def handle_simulation_runs_dialog_close(self) -> None:
        self.simulation_run_dialog = None

    @staticmethod
    def set_enabled_state_of_simulation_run_execution_controls_in_tab_widget(
        tab_widget: QtWidgets.QWidget, should_controls_be_enabled: bool
    ) -> None:
        run_simulation_runs_btn: QtWidgets.QPushButton | None = tab_widget.findChild(
            QtWidgets.QPushButton, RUN_SIM_RUNS_BTN_NAME
        )
        run_simulation_runs_stop_at_first_failure_btn: QtWidgets.QPushButton | None = tab_widget.findChild(
            QtWidgets.QPushButton, RUN_SIM_RUNS_BTN_STOP_AT_FIRST_FAILURE_NAME
        )
        save_simulation_runs_to_file_btn: QtWidgets.QPushButton | None = tab_widget.findChild(
            QtWidgets.QPushButton, SAVE_SIM_RUNS_TO_FILE_BTN_NAME
        )

        if (
            run_simulation_runs_btn is None
            or run_simulation_runs_stop_at_first_failure_btn is None
            or save_simulation_runs_to_file_btn is None
        ):
            return

        run_simulation_runs_btn.setEnabled(should_controls_be_enabled)
        run_simulation_runs_stop_at_first_failure_btn.setEnabled(should_controls_be_enabled)
        # TODO: Button should only be enabled if all simulation runs have their expected output state set?
        save_simulation_runs_to_file_btn.setEnabled(should_controls_be_enabled)
