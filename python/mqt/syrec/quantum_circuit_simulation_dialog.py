# Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
# Copyright (c) 2025 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

from __future__ import annotations

from typing import Final

from PyQt6 import QtWidgets

from mqt import syrec

from .simulation_view.qt_simulation_run_model import InputOutputStateMapping, QtSimulationRunModel
from .simulation_view.qt_simulation_run_styled_item_delegate import SimulationRunModelStyledItemDelegate

LOADED_FROM_FILE_INPUT_FIELD_NAME = "load_from_file_input_field"
ADD_SIM_RUN_BTN_NAME = "add_sim_run_btn"
EDIT_SIM_RUN_BTN_NAME = "edit_sim_run_btn"
DELETE_SIM_RUN_BTN_NAME = "delete_sim_run_btn"
SAVE_SIM_RUNS_TO_FILE_BTN_NAME = "save_sims_to_file_btn"
RUN_SIM_RUNS_BTN_NAME = "run_sims_btn"
RUN_SIM_RUNS_BTN_STOP_AT_FIRST_FAILURE_NAME = "run_sims_stop_first_failure_btn"


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

        # TODO: Default background of tabwidget is white on windows (https://forum.qt.io/topic/82262/default-background-color-of-qtabwidget-and-qwidget-qgroupbox/4)
        self.simulation_runs_tab_widget = QtWidgets.QTabWidget(self)
        self.simulation_runs_tab_widget.addTab(
            QuantumCircuitSimulationDialog.initialize_simulation_runs_tab_widget(self.simulation_runs_model),
            "Check some input-output mapping combinations",
        )
        self.simulation_runs_tab_widget.addTab(
            QuantumCircuitSimulationDialog.initialize_simulation_runs_tab_widget(self.simulation_runs_model),
            "Check all input-output mapping combinations",
        )
        self.simulation_runs_tab_widget.addTab(
            QuantumCircuitSimulationDialog.initialize_simulation_runs_tab_widget(
                self.simulation_runs_model, create_load_from_file_controls=True
            ),
            "Check input-output mapping combinations from file",
        )
        self.simulation_runs_tab_widget.tabBarClicked.connect(self.handle_simulation_runs_tab_widget_tab_bar_clicked)

        QuantumCircuitSimulationDialog.generate_some_simulation_runs(
            20, self.annotatable_quantum_computation, self.simulation_runs_model
        )

        self.layout = QtWidgets.QVBoxLayout()
        self.layout.addWidget(self.simulation_runs_tab_widget)
        self.setLayout(self.layout)

    # TODO: Load from file controls
    @staticmethod
    def initialize_simulation_runs_tab_widget(
        shared_simulation_runs_model: QtSimulationRunModel, create_load_from_file_controls: bool = False
    ) -> QtWidgets.QWidget:
        tab_wrapper_widget = QtWidgets.QFrame()
        tab_wrapper_widget_layout = QtWidgets.QVBoxLayout()
        tab_wrapper_widget.setLayout(tab_wrapper_widget_layout)

        manual_y_space_size: Final[int] = 35
        if create_load_from_file_controls:
            tab_wrapper_widget_layout.addLayout(
                QuantumCircuitSimulationDialog.initialize_load_simulation_runs_from_file_controls()
            )
            tab_wrapper_widget_layout.addSpacing(manual_y_space_size)

        # BEGIN: Create simulation runs list view Qt elements
        simulation_runs_list_view: QtWidgets.QListView = QtWidgets.QListView()
        simulation_runs_list_view.setModel(shared_simulation_runs_model)
        simulation_runs_list_view.setItemDelegate(SimulationRunModelStyledItemDelegate())  # type: ignore[no-untyped-call]
        simulation_runs_list_view.setUniformItemSizes(True)
        simulation_runs_list_view.setFlow(QtWidgets.QListView.Flow.TopToBottom)
        # Select with click on item, unselect with Ctrl+Click on already selected item (see https://doc.qt.io/qt-6/qabstractitemview.html#SelectionMode-enum)
        simulation_runs_list_view.setSelectionMode(QtWidgets.QAbstractItemView.SelectionMode.SingleSelection)

        simulation_runs_list_scrollarea = QtWidgets.QScrollArea()
        simulation_runs_list_scrollarea.setAutoFillBackground(True)
        simulation_runs_list_scrollarea.setWidget(simulation_runs_list_view)
        simulation_runs_list_scrollarea.setWidgetResizable(True)
        tab_wrapper_widget_layout.addWidget(simulation_runs_list_scrollarea)
        # END: Create simulation runs list view Qt elements

        # BEGIN: Create simulation runs list modification Qt elements
        simulation_runs_list_modification_buttons_layout = QtWidgets.QHBoxLayout()
        simulation_runs_list_modification_buttons_layout.addStretch()
        add_simulation_run_button = QtWidgets.QPushButton("Add simulation run", objectName=ADD_SIM_RUN_BTN_NAME)
        simulation_runs_list_modification_buttons_layout.addWidget(add_simulation_run_button)

        edit_simulation_run_button = QtWidgets.QPushButton("Edit simulation run", objectName=EDIT_SIM_RUN_BTN_NAME)
        simulation_runs_list_modification_buttons_layout.addWidget(edit_simulation_run_button)

        delete_simulation_run_button = QtWidgets.QPushButton(
            "Delete simulation run", objectName=DELETE_SIM_RUN_BTN_NAME
        )
        simulation_runs_list_modification_buttons_layout.addWidget(delete_simulation_run_button)

        simulation_runs_list_modification_buttons_layout.addStretch()
        tab_wrapper_widget_layout.addLayout(simulation_runs_list_modification_buttons_layout)
        # END: Create simulation runs list modification Qt elements

        # BEGIN: Create simulation runs execution Qt elements
        simulation_runs_execution_buttons_layout = QtWidgets.QHBoxLayout()
        simulation_runs_execution_buttons_layout.addStretch()

        save_simulation_runs_to_file_button = QtWidgets.QPushButton(
            "Save simulation runs to file", objectName="SAVE_SIM_RUNS_TO_FILE_BTN_NAME"
        )
        simulation_runs_execution_buttons_layout.addWidget(save_simulation_runs_to_file_button)

        run_simulation_runs_button = QtWidgets.QPushButton("Run simulation runs", objectName="RUN_SIM_RUNS_BTN_NAME")
        simulation_runs_execution_buttons_layout.addWidget(run_simulation_runs_button)

        run_simulation_runs_stop_at_first_failure_button = QtWidgets.QPushButton(
            "Run simulation runs (stop at first failure)", objectName="RUN_SIM_RUNS_BTN_STOP_AT_FIRST_FAILURE_NAME"
        )
        simulation_runs_execution_buttons_layout.addWidget(run_simulation_runs_stop_at_first_failure_button)

        simulation_runs_execution_buttons_layout.addStretch()

        tab_wrapper_widget_layout.addSpacing(manual_y_space_size)
        tab_wrapper_widget_layout.addLayout(simulation_runs_execution_buttons_layout)
        # END: Create simulation runs execution Qt elements
        return tab_wrapper_widget

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

        trigger_load_from_file_button = QtWidgets.QPushButton("Load from file")
        controls_layout.addWidget(trigger_load_from_file_button)

        controls_layout.addStretch()
        return controls_layout

    def generate_some_simulation_runs(
        self: int,
        annotatable_quantum_computation: syrec.annotatable_quantum_computation,
        shared_simulation_runs_model: QtSimulationRunModel,
    ) -> None:
        for i in range(self):
            in_state = syrec.n_bit_values_container(annotatable_quantum_computation.num_qubits)
            out_state = syrec.n_bit_values_container(annotatable_quantum_computation.num_qubits)

            in_out_state_mapping: InputOutputStateMapping | None = None
            if i < 2:
                in_out_state_mapping = InputOutputStateMapping(in_state, None)
            else:
                in_out_state_mapping = InputOutputStateMapping(in_state, out_state)

            shared_simulation_runs_model.add_simulation_run(in_out_state_mapping)

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
