# Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
# Copyright (c) 2025 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

from __future__ import annotations

import re
from dataclasses import dataclass
from pathlib import Path
from typing import TYPE_CHECKING, Any, cast

from mqt.core.ir.operations import OpType
from PyQt6 import QtCore, QtGui, QtWidgets

from mqt import syrec

if TYPE_CHECKING:
    from collections.abc import Callable

STRINGIFIED_CIRCUIT_VIEW_QUBIT_LABEL_COMPONENTS_EXTRACTOR_REGEX: re.Pattern[str] = re.compile(
    r"^Q:\s*(?P<q>\d+)\s*\|\s*(?P<label>.+)$"
)


@dataclass
class CircuitViewQubitLabel:
    associated_qubit: int
    internal_qubit_label: str

    def __str__(self) -> str:
        return "Q: " + str(self.associated_qubit) + " | " + self.internal_qubit_label

    # Return type annotation can be defined as Self in Python 3.11 (with 'from typing import Self')
    # Starting with Python 3.14 the class name can be used but since also Python 3.10 is supported by this project
    # we need to chose a variant that is compatible with Python >= 3.10 thus the class name needs to be defined and
    # the import (from __future__ import annotations) needs to be used
    @classmethod
    def load_from_string(cls, input_string: str) -> CircuitViewQubitLabel | None:
        m = STRINGIFIED_CIRCUIT_VIEW_QUBIT_LABEL_COMPONENTS_EXTRACTOR_REGEX.match(input_string)
        if m is not None:
            return CircuitViewQubitLabel(int(m.group("q")), m.group("label").strip())
        return None


def show_error_dialog(title: str, message: str) -> None:
    msg = QtWidgets.QMessageBox()
    msg.setBaseSize(QtCore.QSize(300, 200))
    msg.setIcon(QtWidgets.QMessageBox.Icon.Critical)
    msg.setText(message)
    msg.setWindowTitle(title)
    msg.setStandardButtons(QtWidgets.QMessageBox.StandardButton.Ok)
    msg.exec()


def does_qubit_label_start_with_internal_qubit_label_prefix(qubit_label: str) -> bool:
    return qubit_label.startswith("__q")


class CircuitLineItem(QtWidgets.QGraphicsItemGroup):  # type: ignore[misc]
    def __init__(self, index: int, width: int, parent: QtWidgets.QWidget | None = None) -> None:
        QtWidgets.QGraphicsItemGroup.__init__(self, parent)

        # Tool Tip
        self.setToolTip(f'<b><font color="#606060">Line:</font></b> {index:d}')

        # Create sub-lines
        x = 0
        for i in range(width + 1):
            e_width = 15 if i in {0, width} else 30
            self.addToGroup(QtWidgets.QGraphicsLineItem(x, index * 30, x + e_width, index * 30))
            x += e_width


class GateItem(QtWidgets.QGraphicsItemGroup):  # type: ignore[misc]
    def __init__(
        self,
        annotatable_quantum_computation: syrec.annotatable_quantum_computation,
        quantum_operation_index: int,
        parent: QtWidgets.QWidget | None = None,
    ) -> None:
        QtWidgets.QGraphicsItemGroup.__init__(self, parent)
        self.setFlag(QtWidgets.QGraphicsItem.GraphicsItemFlag.ItemIsSelectable)

        quantum_operation = annotatable_quantum_computation[quantum_operation_index]
        qubits_of_operation = list(quantum_operation.targets)
        qubits_of_operation.extend(control.qubit for control in quantum_operation.controls)
        qubits_of_operation.sort()

        quantum_operation_annotations = annotatable_quantum_computation.get_annotations_of_quantum_operation(
            quantum_operation_index
        ).items()

        self.setToolTip(
            "\n".join([f'<b><font color="#606060">{k}:</font></b> {v}' for (k, v) in quantum_operation_annotations])
        )

        if len(qubits_of_operation) > 1:
            circuit_line = QtWidgets.QGraphicsLineItem(
                0, qubits_of_operation[0] * 30, 0, qubits_of_operation[-1] * 30, self
            )
            self.addToGroup(circuit_line)

        for t in quantum_operation.targets:
            if quantum_operation.type_ == OpType.x:
                target = QtWidgets.QGraphicsEllipseItem(-10, t * 30 - 10, 20, 20, self)
                target_line = QtWidgets.QGraphicsLineItem(0, t * 30 - 10, 0, t * 30 + 10, self)
                target_line2 = QtWidgets.QGraphicsLineItem(-10, t * 30, 10, t * 30, self)
                self.addToGroup(target)
                self.addToGroup(target_line)
                self.addToGroup(target_line2)
            if quantum_operation.type_ == OpType.swap:
                cross_tl_br = QtWidgets.QGraphicsLineItem(-5, t * 30 - 5, 5, t * 30 + 5, self)
                cross_tr_bl = QtWidgets.QGraphicsLineItem(5, t * 30 - 5, -5, t * 30 + 5, self)
                self.addToGroup(cross_tl_br)
                self.addToGroup(cross_tr_bl)

        for c in quantum_operation.controls:
            control = QtWidgets.QGraphicsEllipseItem(-5, c.qubit * 30 - 5, 10, 10, self)
            control.setBrush(QtGui.QColorConstants.Black)
            self.addToGroup(control)


class CircuitView(QtWidgets.QGraphicsView):  # type: ignore[misc]
    qubit_label_clicked = QtCore.pyqtSignal(str, name="qubitLabelClicked")

    def __init__(
        self,
        annotatable_quantum_computation: syrec.annotatable_quantum_computation | None = None,
        parent: QtWidgets.QWidget | None = None,
    ) -> None:
        QtWidgets.QGraphicsView.__init__(self, parent)

        # Scene
        self.setScene(QtWidgets.QGraphicsScene(self))
        self.scene().setBackgroundBrush(QtGui.QColorConstants.White)

        # Load circuit
        self.annotatable_quantum_computation: syrec.annotatable_quantum_computation | None = None
        # We are assuming that the majority of the qubits in a quantum computation are either garbage or ancillary qubits, so checking whether a given qubit is ancillary or garbage is then
        # equal to whether the lookup does NOT contain an entry for the qubit (this should save us some memory since we only need to store the qubit labels of the non-ancillary and non-garbage qubits)
        self.non_ancillary_or_garbage_qubits_lookup: set[int] = set()
        self.lines: list[CircuitLineItem] = []
        self.inputs: list[QtWidgets.QGraphicsTextItem | None] = []
        self.outputs: list[QtWidgets.QGraphicsTextItem | None] = []
        if annotatable_quantum_computation is not None:
            self.load(annotatable_quantum_computation)

    def clear(self) -> None:
        self.scene().clear()

        self.annotatable_quantum_computation = None
        self.non_ancillary_or_garbage_qubits_lookup.clear()
        self.lines = []
        self.inputs = []
        self.outputs = []

    def mousePressEvent(self, event: QtWidgets.QGraphicsSceneMouseEvent) -> None:  # noqa: N802
        graphics_view_position_of_click: QtCore.QPoint = event.pos()
        item: QtWidgets.QGraphicsTextItem | None = self.itemAt(graphics_view_position_of_click)

        if item is not None and isinstance(item, QtWidgets.QGraphicsTextItem):
            destringified_qubit_label: CircuitViewQubitLabel | None = CircuitViewQubitLabel.load_from_string(
                item.toPlainText()
            )
            if destringified_qubit_label is None:
                show_error_dialog(
                    "Error handling click on circuit view qubit label",
                    "Failed to convert the circuit view qubit label\n"
                    + item.toPlainText()
                    + "\nto internal DTO. This should not happen!",
                )
            elif destringified_qubit_label.associated_qubit not in self.non_ancillary_or_garbage_qubits_lookup:
                self.qubit_label_clicked.emit(str(destringified_qubit_label))

        super().mousePressEvent(event)

    def load(self, annotatable_quantum_computation: syrec.annotatable_quantum_computation) -> None:
        self.clear()

        self.annotatable_quantum_computation = annotatable_quantum_computation
        n_quantum_ops = self.annotatable_quantum_computation.num_ops
        width = 30 * n_quantum_ops

        for i in range(self.annotatable_quantum_computation.num_qubits):
            line = CircuitLineItem(i, n_quantum_ops)
            self.lines.append(line)
            self.scene().addItem(line)

            circuit_view_qubit_label = CircuitViewQubitLabel(i, "")
            internal_qubit_label: str | None = self.annotatable_quantum_computation.get_qubit_label(
                i, syrec.qubit_label_type.internal
            )
            circuit_view_qubit_label.internal_qubit_label = (
                "<UNKNOWN>" if internal_qubit_label is None else internal_qubit_label
            )

            # Since the qubits generated for SyReC variables of type 'in' are also considered garbage we need to also filter the clickable qubits to only consider qubits whose label starts with the prefix "__q" (marking qubits generated for local variables of a SyReC module).
            should_qubit_line_text_be_clickable = (
                self.annotatable_quantum_computation.is_circuit_qubit_ancillary(
                    circuit_view_qubit_label.associated_qubit
                )
                or self.annotatable_quantum_computation.is_circuit_qubit_garbage(
                    circuit_view_qubit_label.associated_qubit
                )
            ) and does_qubit_label_start_with_internal_qubit_label_prefix(circuit_view_qubit_label.internal_qubit_label)
            if not should_qubit_line_text_be_clickable:
                self.non_ancillary_or_garbage_qubits_lookup.add(circuit_view_qubit_label.associated_qubit)

            input_qubit_line_text_item = self.add_line_label(
                0,
                i * 30,
                str(circuit_view_qubit_label),
                QtCore.Qt.AlignmentFlag.AlignRight,
                self.annotatable_quantum_computation.is_circuit_qubit_ancillary(i),
                should_qubit_line_text_be_clickable,
            )
            self.inputs.append(input_qubit_line_text_item)

            output_qubit_line_text_item = self.add_line_label(
                width,
                i * 30,
                str(circuit_view_qubit_label),
                QtCore.Qt.AlignmentFlag.AlignLeft,
                self.annotatable_quantum_computation.is_circuit_qubit_garbage(i),
                should_qubit_line_text_be_clickable,
            )
            self.outputs.append(output_qubit_line_text_item)

        for i in range(n_quantum_ops):
            gate = GateItem(self.annotatable_quantum_computation, i)
            gate.setPos(i * 30 + 15, 0)
            self.scene().addItem(gate)

    def add_line_label(
        self,
        x: int,
        y: int,
        text: str,
        align: QtCore.Qt.AlignmentFlag,
        is_ancillary_or_garbage_qubit: bool,
        should_label_be_clickable: bool,
    ) -> QtWidgets.QGraphicsTextItem | None:
        text_item = self.scene().addText(text)
        text_item.setPlainText(text)

        if is_ancillary_or_garbage_qubit:
            text_item.setDefaultTextColor(QtGui.QColorConstants.Red)
        if should_label_be_clickable:
            text_item.setTextInteractionFlags(QtCore.Qt.TextInteractionFlag.LinksAccessibleByMouse)

        if align == QtCore.Qt.AlignmentFlag.AlignRight:
            x -= text_item.boundingRect().width()

        text_item.setPos(x, y - 12)
        return text_item

    def wheelEvent(self, event):  # noqa: N802
        factor = 1.2
        if event.angleDelta().y() < 0 or event.angleDelta().x() < 0:
            factor = 1.0 / factor
        self.scale(factor, factor)

        return QtWidgets.QGraphicsView.wheelEvent(self, event)


class SyReCEditor(QtWidgets.QWidget):  # type: ignore[misc]
    widget: CodeEditor | None = None
    build_successful: Callable[[syrec.annotatable_quantum_computation], None] | None = None
    build_failed: Callable[[str], None] | None = None
    before_build: Callable[[], None] | None = None
    parser_failed: Callable[[str], None] | None = None

    cost_aware_synthesis = 0
    line_aware_synthesis = 0

    def __init__(self, parent: QtWidgets.QWidget | None = None) -> None:
        super().__init__()

        self.parent = parent
        self.setup_actions()

        self.filename: str

        self.title = "SyReC Simulation"
        self.left = 0
        self.top = 0
        self.width = 600
        self.height = 400

        self.setWindowTitle(self.title)
        self.setGeometry(self.left, self.top, self.width, self.height)
        self.layout = QtWidgets.QVBoxLayout()

        self.table = QtWidgets.QTableWidget()
        self.layout.addWidget(self.table)
        self.setLayout(self.layout)

    def setup_actions(self) -> None:
        self.open_action = QtGui.QAction(QtGui.QIcon.fromTheme("document-open"), "&Open...", self.parent)
        self.build_action = QtGui.QAction(QtGui.QIcon.fromTheme("media-playback-start"), "&Build...", self.parent)
        self.sim_action = QtGui.QAction(
            QtGui.QIcon.fromTheme("x-office-spreadsheet"), "&Sim...", self.parent
        )  # system-run
        self.stat_action = QtGui.QAction(QtGui.QIcon.fromTheme("applications-other"), "&Stats...", self.parent)

        self.buttonCostAware = QtWidgets.QRadioButton("Cost-aware synthesis", self)
        self.buttonCostAware.toggled.connect(self.item_selected)

        self.buttonLineAware = QtWidgets.QRadioButton("Line-aware synthesis", self)
        self.buttonLineAware.setChecked(True)
        self.line_aware_synthesis = 1
        self.buttonLineAware.toggled.connect(self.item_selected)

        self.sim_action.setDisabled(True)
        self.stat_action.setDisabled(True)

        self.open_action.triggered.connect(self.open_file)

        self.build_action.triggered.connect(self.build)

        self.sim_action.triggered.connect(self.sim)

        self.stat_action.triggered.connect(self.stat)

        self.configurable_parser_and_synthesis_options = syrec.configurable_options()
        self.configurable_parser_and_synthesis_options_update_button = QtWidgets.QPushButton(
            "Update configurable options", self
        )
        self.configurable_parser_and_synthesis_options_update_button.clicked.connect(self.update_configurable_options)

    def update_configurable_options(self) -> None:
        update_configurable_options_modal = ConfigurableOptionsUpdated(
            self, self.configurable_parser_and_synthesis_options
        )
        update_configurable_options_modal.setWindowTitle("Update configurable options")
        update_configurable_options_modal.exec()

    def item_selected(self):
        # Disable sim and stat function
        self.sim_action.setDisabled(True)
        self.stat_action.setDisabled(True)

        # if first button is selected
        if self.sender() == self.buttonCostAware:
            if self.buttonCostAware.isChecked():
                self.cost_aware_synthesis = 1
                self.line_aware_synthesis = 0
                # making other check box to uncheck
                self.buttonLineAware.setChecked(False)
            else:
                self.cost_aware_synthesis = 0
                self.line_aware_synthesis = 1
                # making other check box to uncheck
                self.buttonLineAware.setChecked(True)

        # if second button is selected
        elif self.sender() == self.buttonLineAware:
            if self.buttonLineAware.isChecked():
                self.cost_aware_synthesis = 0
                self.line_aware_synthesis = 1
                # making other check box to uncheck
                self.buttonCostAware.setChecked(False)
            else:
                self.cost_aware_synthesis = 1
                self.line_aware_synthesis = 0
                # making other check box to uncheck
                self.buttonCostAware.setChecked(True)

    def open_file(self) -> None:
        selected_file_name, _ = QtWidgets.QFileDialog.getOpenFileName(
            parent=self.parent,
            caption="Open Specification",
            filter="SyReC specification (*.src)",
            options=QtWidgets.QFileDialog.Option.ReadOnly,
        )

        if len(selected_file_name) > 0 and self.widget is not None:
            self.widget.load(selected_file_name)
            if self.before_build is not None:
                self.before_build()

    def build(self) -> None:
        if self.before_build is not None:
            self.before_build()

        self.prog = syrec.program()

        error_string = self.prog.read_from_string(self.getText(), self.configurable_parser_and_synthesis_options)

        if error_string == "PARSE_STRING_FAILED":
            if self.parser_failed is not None:
                self.parser_failed("Editor is Empty")
            return

        if error_string:
            if self.build_failed is not None:
                self.build_failed(error_string)
            return

        self.annotatable_quantum_computation = syrec.annotatable_quantum_computation()
        if self.cost_aware_synthesis:
            syrec.cost_aware_synthesis(
                self.annotatable_quantum_computation, self.prog, self.configurable_parser_and_synthesis_options
            )
        else:
            syrec.line_aware_synthesis(
                self.annotatable_quantum_computation, self.prog, self.configurable_parser_and_synthesis_options
            )

        self.sim_action.setDisabled(False)
        self.stat_action.setDisabled(False)

        n_total_qubits = self.annotatable_quantum_computation.num_qubits
        n_ancilla_qubits = self.annotatable_quantum_computation.num_ancilla_qubits
        n_garbage_qubits = self.annotatable_quantum_computation.num_garbage_qubits

        n_input_qubits = n_total_qubits - n_ancilla_qubits
        n_output_qubits = n_input_qubits
        n_quantum_operations = self.annotatable_quantum_computation.num_ops

        print("Number Of quantum operations : ", n_quantum_operations)
        print("Number Of qubits             : ", n_total_qubits)
        print("Number Of input qubits       : ", n_input_qubits)
        print("Number Of ancilla qubits     : ", n_ancilla_qubits)
        print("Number of output qubits      : ", n_output_qubits)
        print("Number of garbage qubits     : ", n_garbage_qubits)

        if self.build_successful is not None:
            self.build_successful(self.annotatable_quantum_computation)

    def stat(self) -> None:
        n_quantum_operations = self.annotatable_quantum_computation.num_ops
        n_total_qubits = self.annotatable_quantum_computation.num_qubits
        quantum_cost_for_synthesis = self.annotatable_quantum_computation.get_quantum_cost_for_synthesis()
        transistor_cost_for_synthesis = self.annotatable_quantum_computation.get_transistor_cost_for_synthesis()

        temp = "Number of quantum operations:\t\t{}\nNumber of qubits:\t\t{}\nQuantum cost for synthesis:\t{}\nTransistor cost for synthesis:\t{}\n"

        output = temp.format(
            n_quantum_operations, n_total_qubits, quantum_cost_for_synthesis, transistor_cost_for_synthesis
        )

        msg = QtWidgets.QMessageBox()
        msg.setBaseSize(QtCore.QSize(300, 200))
        msg.setInformativeText(output)
        msg.setWindowTitle("Statistics")
        msg.setStandardButtons(QtWidgets.QMessageBox.StandardButton.Ok)
        msg.exec()

    def sim(self) -> None:
        bit1_mask = 0

        no_of_bits = self.annotatable_quantum_computation.num_qubits
        all_inputs_bit_mask = 2**self.annotatable_quantum_computation.num_data_qubits - 1
        input_list = [all_inputs_bit_mask & x for x in range(2**self.annotatable_quantum_computation.num_data_qubits)]

        n_ancilla_qubits = self.annotatable_quantum_computation.num_ancilla_qubits
        n_data_qubits = self.annotatable_quantum_computation.num_data_qubits
        ancilla_qubit_values = [False] * n_ancilla_qubits

        # Ancilla qubits are assumed to be defined immediately after the data qubits in the quantum computation thus the first ancillary qubit has the index n_data_qubits + 1
        ancillary_qubit_index = self.annotatable_quantum_computation.num_data_qubits
        ancilla_qubit_indices = set()
        ancilla_qubit_indices.update([ancillary_qubit_index + i for i in range(n_ancilla_qubits)])

        if n_ancilla_qubits > 0:
            for quantum_operation_index in range(self.annotatable_quantum_computation.num_ops):
                quantum_operation = self.annotatable_quantum_computation[quantum_operation_index]

                # We assume that the value of the ancillary qubits is set at the start of the quantum computation with the help of X gates operating only on the ancillary qubits
                # The initial state of the ancilla is assumed to be set if any of the following conditions is not met
                if (
                    quantum_operation.type_ != OpType.x
                    or len(quantum_operation.controls) > 0
                    or len(quantum_operation.targets) != 1
                    or quantum_operation.targets[0] not in ancilla_qubit_indices
                ):
                    break

                # There should only be one X gate per ancillary qubit (if its initial state should be 1 instead of the default state of 0) but for now we allow multiple
                ancilla_qubit_values[quantum_operation.targets[0] - ancillary_qubit_index] = not ancilla_qubit_values[
                    quantum_operation.targets[0] - ancillary_qubit_index
                ]

            for i in range(no_of_bits):
                if (
                    self.annotatable_quantum_computation.is_circuit_qubit_ancillary(i) is True
                    and ancilla_qubit_values[i - n_data_qubits]
                ):
                    bit1_mask += 2**i

        input_list_len = len(input_list)

        combination_inp = []
        combination_out = []

        final_inp = []
        final_out = []

        for i in input_list:
            my_inp_bitset = syrec.n_bit_values_container(no_of_bits, i)
            my_out_bitset = syrec.n_bit_values_container(no_of_bits)
            syrec.simple_simulation(my_out_bitset, self.annotatable_quantum_computation, my_inp_bitset)

            inp_bitset_with_ancillaes_set = syrec.n_bit_values_container(no_of_bits, i + bit1_mask)
            combination_inp.append(str(inp_bitset_with_ancillaes_set))
            combination_out.append(str(my_out_bitset))

        sorted_ind = sorted(range(len(combination_inp)), key=lambda k: int(combination_inp[k], 2))

        for i in sorted_ind:
            final_inp.append(combination_inp[i])
            final_out.append(combination_out[i])

        # Initiate table
        self.table.clear()
        self.table.setRowCount(0)
        self.table.setColumnCount(0)
        self.table.setRowCount(input_list_len + 2)
        self.table.setColumnCount(2 * no_of_bits)

        self.table.setSpan(0, 0, 1, no_of_bits)
        header1 = QtWidgets.QTableWidgetItem("INPUTS")
        header1.setTextAlignment(QtCore.Qt.AlignmentFlag.AlignCenter)
        self.table.setItem(0, 0, header1)

        self.table.setSpan(0, no_of_bits, 1, no_of_bits)
        header2 = QtWidgets.QTableWidgetItem("OUTPUTS")
        header2.setTextAlignment(QtCore.Qt.AlignmentFlag.AlignCenter)
        self.table.setItem(0, no_of_bits, header2)

        self.table.horizontalHeader().setVisible(False)
        self.table.verticalHeader().setVisible(False)

        for i in range(no_of_bits):
            to_be_displayed_qubit_label_type = (
                syrec.qubit_label_type.internal
                if self.annotatable_quantum_computation.is_circuit_qubit_ancillary(i)
                else syrec.qubit_label_type.user_declared
            )
            io_qubit_label: str | None = self.annotatable_quantum_computation.get_qubit_label(
                i, to_be_displayed_qubit_label_type
            )

            # Fetching the matching label for a qubit of the annotatable quantum computation should not fail but in case it does, assume a default qubit label <UNKNOWN>.
            # We still display the column in any case because otherwise the user would be shown a different number of qubits than the number of qubits that actual exist in the annotatable quantum computation.
            if io_qubit_label is None:
                io_qubit_label = "<UNKNOWN>"

            input_signal = QtWidgets.QTableWidgetItem(io_qubit_label)
            input_signal.setTextAlignment(QtCore.Qt.AlignmentFlag.AlignCenter)
            self.table.setItem(1, i, QtWidgets.QTableWidgetItem(input_signal))

            output_signal = QtWidgets.QTableWidgetItem(io_qubit_label)
            output_signal.setTextAlignment(QtCore.Qt.AlignmentFlag.AlignCenter)
            self.table.setItem(1, i + no_of_bits, QtWidgets.QTableWidgetItem(output_signal))

        for i in range(input_list_len):
            for j in range(no_of_bits):
                input_cell = QtWidgets.QTableWidgetItem(final_inp[i][j])
                input_cell.setTextAlignment(QtCore.Qt.AlignmentFlag.AlignCenter)
                self.table.setItem(i + 2, j, QtWidgets.QTableWidgetItem(input_cell))

                output_cell = QtWidgets.QTableWidgetItem(final_out[i][j])
                output_cell.setTextAlignment(QtCore.Qt.AlignmentFlag.AlignCenter)
                self.table.setItem(i + 2, j + no_of_bits, QtWidgets.QTableWidgetItem(output_cell))

        self.table.horizontalHeader().setStretchLastSection(True)
        self.table.horizontalHeader().setSectionResizeMode(QtWidgets.QHeaderView.ResizeMode.Stretch)
        self.show()


class SyReCHighlighter(QtGui.QSyntaxHighlighter):  # type: ignore[misc]
    def __init__(self, parent: QtGui.QTextDocument) -> None:
        QtGui.QSyntaxHighlighter.__init__(self, parent)

        self.highlightingRules = []

        keyword_format = QtGui.QTextCharFormat()
        keyword_format.setForeground(QtGui.QColorConstants.DarkBlue)
        keyword_format.setFontWeight(QtGui.QFont.Weight.Bold)
        keywords = [
            "module",
            "in",
            "out",
            "inout",
            "wire",
            "state",
            "if",
            "else",
            "then",
            "fi",
            "for",
            "step",
            "to",
            "do",
            "rof",
            "skip",
            "call",
            "uncall",
        ]

        for pattern in [f"\\b{keyword}\\b" for keyword in keywords]:
            self.highlightingRules.append([QtCore.QRegularExpression(pattern), keyword_format])

        number_format = QtGui.QTextCharFormat()
        number_format.setForeground(QtGui.QColorConstants.DarkCyan)
        self.highlightingRules.append([QtCore.QRegularExpression("\\b[0-9]+\\b"), number_format])

        loop_format = QtGui.QTextCharFormat()
        loop_format.setForeground(QtGui.QColorConstants.DarkRed)
        self.highlightingRules.append([QtCore.QRegularExpression("\\$[A-Za-z_0-9]+"), loop_format])

    def highlightBlock(self, text):  # noqa: N802
        for rule in self.highlightingRules:
            expression = rule[0]
            match = expression.match(text)
            while match.hasMatch():
                index = match.capturedStart()
                length = match.capturedLength()
                self.setFormat(index, length, rule[1])
                match = expression.match(text, offset=index + length)


class QtSyReCEditor(SyReCEditor):
    def __init__(self, parent: Any | None = None) -> None:
        SyReCEditor.__init__(self, parent)

        self.widget: CodeEditor = CodeEditor(parent)
        self.widget.setFont(QtGui.QFont("Monospace", 10, QtGui.QFont.Weight.Normal))
        self.widget.highlighter = SyReCHighlighter(self.widget.document())

    def setText(self, text):  # noqa: N802
        self.widget.setPlainText(text)

    def getText(self):  # noqa: N802
        return self.widget.toPlainText()


class LineNumberArea(QtWidgets.QWidget):  # type: ignore[misc]
    def __init__(self, editor: CodeEditor) -> None:
        QtWidgets.QWidget.__init__(self, editor)
        self.codeEditor = editor

    def sizeHint(self) -> QtCore.QSize:  # noqa: N802
        return QtCore.QSize(self.codeEditor.lineNumberAreaWidth(), 0)

    def paintEvent(self, event: QtGui.QPaintEvent) -> None:  # noqa: N802
        self.codeEditor.lineNumberAreaPaintEvent(event)


class CodeEditor(QtWidgets.QPlainTextEdit):  # type: ignore[misc]
    def __init__(self, parent: Any | None = None) -> None:
        QtWidgets.QPlainTextEdit.__init__(self, parent)

        self.lineNumberArea = LineNumberArea(self)

        self.blockCountChanged.connect(self.updateLineNumberAreaWidth)
        self.updateRequest.connect(self.updateLineNumberArea)
        self.cursorPositionChanged.connect(self.highlightCurrentLine)

        self.updateLineNumberAreaWidth()
        self.highlightCurrentLine()

    def load(self, filename: str) -> None:
        if len(filename) > 0:
            with Path(filename).open(encoding="utf-8") as f:
                self.setPlainText(f.read())

    def lineNumberAreaPaintEvent(self, event: QtGui.QPaintEvent) -> None:  # noqa: N802
        painter = QtGui.QPainter(self.lineNumberArea)
        painter.fillRect(event.rect(), QtGui.QColorConstants.LightGray)

        block = self.firstVisibleBlock()
        block_number = block.blockNumber()
        top = self.blockBoundingGeometry(block).translated(self.contentOffset()).top()
        bottom = top + self.blockBoundingGeometry(block).height()

        while block.isValid() and top <= event.rect().bottom():
            if block.isVisible() and bottom >= event.rect().top():
                number = str(block_number + 1)
                painter.setPen(QtGui.QColorConstants.Black)
                painter.drawText(
                    0,
                    round(top),
                    self.lineNumberArea.width(),
                    self.fontMetrics().height(),
                    QtCore.Qt.AlignmentFlag.AlignRight,
                    number,
                )

            block = block.next()
            top = bottom
            bottom = top + self.blockBoundingGeometry(block).height()
            block_number += 1

    def lineNumberAreaWidth(self) -> int:  # noqa: N802
        digits = 1
        max_ = max(1, self.blockCount())
        while max_ >= 10:
            max_ /= 10
            digits += 1

        return cast("int", 3 + self.fontMetrics().horizontalAdvance("9") * digits)

    def resizeEvent(self, event: QtGui.QResizeEvent) -> None:  # noqa: N802
        QtWidgets.QPlainTextEdit.resizeEvent(self, event)

        cr = self.contentsRect()
        self.lineNumberArea.setGeometry(QtCore.QRect(cr.left(), cr.top(), self.lineNumberAreaWidth(), cr.height()))

    def updateLineNumberAreaWidth(self) -> None:  # noqa: N802
        self.setViewportMargins(self.lineNumberAreaWidth(), 0, 0, 0)

    def highlightCurrentLine(self) -> None:  # noqa: N802
        extra_selections = []

        if not self.isReadOnly():
            selection = QtWidgets.QTextEdit.ExtraSelection()

            line_color = QtGui.QColorConstants.Yellow.lighter(160)

            selection.format.setBackground(line_color)
            selection.format.setProperty(QtGui.QTextFormat.Property.FullWidthSelection, True)
            selection.cursor = self.textCursor()
            selection.cursor.clearSelection()
            extra_selections.append(selection)

        self.setExtraSelections(extra_selections)

    def updateLineNumberArea(self, rect: QtCore.QRect, dy: int) -> None:  # noqa: N802
        if dy != 0:
            self.lineNumberArea.scroll(0, dy)
        else:
            self.lineNumberArea.update(0, rect.y(), self.lineNumberArea.width(), rect.height())

        if rect.contains(self.viewport().rect()):
            self.updateLineNumberAreaWidth()


class LogWidget(QtWidgets.QTreeWidget):  # type: ignore[misc]
    def __init__(self, parent: QtWidgets.QWidget | None = None) -> None:
        QtWidgets.QTreeWidget.__init__(self, parent)

        self.setRootIsDecorated(False)
        self.setHeaderLabels(["Message"])

    def addMessage(self, message: str) -> None:  # noqa: N802
        item = QtWidgets.QTreeWidgetItem([message])
        self.addTopLevelItem(item)


class CircuitQubitInlineInformation(QtWidgets.QWidget):  # type: ignore[misc]
    def __init__(self, parent: QtWidgets.QWidget | None = None) -> None:
        super().__init__()
        self.parent = parent

        layout = QtWidgets.QVBoxLayout(self)

        non_stack_info_layout = QtWidgets.QGridLayout()
        self.associated_module_signature_label = QtWidgets.QLabel("Associated module signature:")
        self.associated_module_signature_value = QtWidgets.QLabel("")
        non_stack_info_layout.addWidget(
            self.associated_module_signature_label, 0, 0, 1, 1, QtCore.Qt.AlignmentFlag.AlignLeft
        )
        non_stack_info_layout.addWidget(
            self.associated_module_signature_value, 0, 1, 1, 1, QtCore.Qt.AlignmentFlag.AlignLeft
        )

        self.original_qubit_label = QtWidgets.QLabel("Original qubit label:")
        self.original_qubit_label_value = QtWidgets.QLabel("")
        non_stack_info_layout.addWidget(self.original_qubit_label, 1, 0, 1, 1, QtCore.Qt.AlignmentFlag.AlignLeft)
        non_stack_info_layout.addWidget(self.original_qubit_label_value, 1, 1, 1, 1, QtCore.Qt.AlignmentFlag.AlignLeft)

        self.internal_qubit_label = QtWidgets.QLabel("Internal qubit label:")
        self.internal_qubit_label_value = QtWidgets.QLabel("")
        non_stack_info_layout.addWidget(self.internal_qubit_label, 2, 0, 1, 1, QtCore.Qt.AlignmentFlag.AlignLeft)
        non_stack_info_layout.addWidget(self.internal_qubit_label_value, 2, 1, 1, 1, QtCore.Qt.AlignmentFlag.AlignLeft)
        non_stack_info_layout.rowStretch(1)

        inline_stack_tree_layout = QtWidgets.QVBoxLayout()
        self.inline_stack_tree_view_label = QtWidgets.QLabel("Inline stack")
        self.inline_stack_tree_view_label.setAlignment(QtCore.Qt.AlignmentFlag.AlignCenter)

        self.inline_stack_tree_view = QtWidgets.QTreeView()
        self.inline_stack_tree_view.setHeaderHidden(True)

        self.inline_stack_tree_model = QtGui.QStandardItemModel()
        self.inline_stack_tree_model_root = self.inline_stack_tree_model.invisibleRootItem()

        self.inline_stack_tree_view.setModel(self.inline_stack_tree_model)
        inline_stack_tree_layout.addWidget(self.inline_stack_tree_view_label)
        inline_stack_tree_layout.addWidget(self.inline_stack_tree_view)

        layout.addLayout(non_stack_info_layout)
        layout.addLayout(inline_stack_tree_layout)

        self.help_text_label = QtWidgets.QLabel(
            "Select a garbage or ancillary qubit from the combobox or click on the label of the qubit in the circuit view. This information is not generated by default and needs to be enabled in the configurable options."
        )
        self.help_text_label.setWordWrap(True)
        layout.addWidget(self.help_text_label, QtCore.Qt.AlignmentFlag.AlignCenter)

        self.toggle_all_inline_information_controls(False)

        self.layout = layout
        self.setLayout(self.layout)

    def toggle_all_inline_information_controls(self, show_inline_information: bool) -> None:
        self.associated_module_signature_label.setVisible(show_inline_information)
        self.associated_module_signature_value.setVisible(show_inline_information)

        self.original_qubit_label.setVisible(show_inline_information)
        self.original_qubit_label_value.setVisible(show_inline_information)

        self.internal_qubit_label.setVisible(show_inline_information)
        self.internal_qubit_label_value.setVisible(show_inline_information)

        self.inline_stack_tree_view_label.setVisible(show_inline_information)
        self.inline_stack_tree_view.setVisible(show_inline_information)
        self.help_text_label.setVisible(not show_inline_information)

    def set_inline_stack_controls_visibility_based_on_stack_size(self, stack_size: int) -> None:
        should_be_visible = stack_size != 0
        self.associated_module_signature_label.setVisible(should_be_visible)
        self.associated_module_signature_value.setVisible(should_be_visible)
        self.inline_stack_tree_view_label.setVisible(should_be_visible)
        self.inline_stack_tree_view.setVisible(should_be_visible)

    def update_information(
        self,
        internal_qubit_label: str,
        user_declared_qubit_label: str | None,
        inline_stack: syrec.qubit_inlining_stack | None,
    ) -> None:
        self.clear_inline_data_controls()
        self.toggle_all_inline_information_controls(show_inline_information=True)

        self.internal_qubit_label_value.setText(internal_qubit_label)
        # No user declared qubit label will exist for ancillary qubits
        if user_declared_qubit_label is not None:
            self.original_qubit_label_value.setText(user_declared_qubit_label)
        else:
            self.original_qubit_label_value.setVisible(False)
            self.original_qubit_label.setVisible(False)

        inline_stack_size = inline_stack.size() if inline_stack is not None else 0
        self.set_inline_stack_controls_visibility_based_on_stack_size(inline_stack_size)

        if inline_stack_size == 0:
            return

        # We know at this point that the inline stack is not empty and thus not None
        self.associated_module_signature_value.setText(
            inline_stack[inline_stack_size - 1].stringified_signature_of_called_module  # type: ignore[index]
        )

        prev_tree_model_entry = None
        for i in reversed(range(inline_stack_size)):
            # We know at this point that the inline stack is not empty and thus not None
            parent_tree_model_entry = self.create_tree_view_entry_for_inline_stack_entry(
                inline_stack[i],  # type: ignore[index]
                i == inline_stack_size - 1,
            )

            if prev_tree_model_entry is not None:
                parent_tree_model_entry.appendRow(prev_tree_model_entry)
            prev_tree_model_entry = parent_tree_model_entry
        self.inline_stack_tree_model_root.appendRow(prev_tree_model_entry)

    def clear_inline_data_controls(self) -> None:
        self.associated_module_signature_value.clear()
        self.original_qubit_label_value.clear()
        self.internal_qubit_label_value.clear()
        self.inline_stack_tree_model.removeRows(0, self.inline_stack_tree_model.rowCount())

    def clear_and_hide_all_inline_data_controls(self) -> None:
        self.clear_inline_data_controls()
        self.toggle_all_inline_information_controls(False)

    @staticmethod
    def create_tree_view_entry_for_inline_stack_entry(
        inline_stack_entry: syrec.qubit_inlining_stack_entry, only_print_signature: bool
    ) -> QtGui.QStandardItem:
        tree_entry = QtGui.QStandardItem(inline_stack_entry.stringified_signature_of_called_module)
        bold_font = QtGui.QFont()
        bold_font.setBold(True)
        # QtGui.QFont("Times", 12)
        tree_entry.setFont(bold_font)
        tree_entry.setEditable(False)

        if not only_print_signature:
            source_code_line_number_tree_entry = QtGui.QStandardItem(
                "Line: " + str(inline_stack_entry.line_number_of_call_of_target_module)
                if inline_stack_entry.line_number_of_call_of_target_module is not None
                else "<UNKNOWN>"
            )
            source_code_line_number_tree_entry.setEditable(False)

            target_module_call_type_tree_entry = QtGui.QStandardItem(
                "Call type: " + ("CALL" if inline_stack_entry.is_target_module_accessed_via_call_stmt else "UNCALL")
            )
            target_module_call_type_tree_entry.setEditable(False)

            tree_entry.appendColumn([source_code_line_number_tree_entry, target_module_call_type_tree_entry])
        return tree_entry


class CircuitQubitsInformationLookup(QtWidgets.QWidget):  # type: ignore[misc]
    def __init__(self, parent: QtWidgets.QWidget | None = None) -> None:
        super().__init__()
        self.parent = parent
        self.annotatable_quantum_computation: syrec.annotatable_quantum_computation | None = None
        self.qubits_labels_of_local_variables_lookup: set[str] = set()

        self.layout = QtWidgets.QVBoxLayout(self)

        header_label_layout = QtWidgets.QHBoxLayout()
        header_label = QtWidgets.QLabel("Ancillary/local SyReC module variable qubit inline information")
        header_label_layout.addStretch()
        header_label_layout.addWidget(header_label)
        header_label_layout.addStretch()
        self.layout.addLayout(header_label_layout)

        search_controls_layout = QtWidgets.QHBoxLayout()
        qubit_label_combobox_label = QtWidgets.QLabel("Qubit label: ")
        self.selectable_qubit_labels_combobox = QtWidgets.QComboBox()
        self.selectable_qubit_labels_combobox.setPlaceholderText("<SELECT A QUBIT LABEL>")
        self.selectable_qubit_labels_combobox.currentIndexChanged.connect(self.handle_combobox_selection_change)
        self.selectable_qubit_labels_combobox.setDisabled(True)

        search_controls_layout.addStretch()
        search_controls_layout.addWidget(qubit_label_combobox_label)
        search_controls_layout.addWidget(self.selectable_qubit_labels_combobox)
        search_controls_layout.addStretch()
        self.layout.addLayout(search_controls_layout)

        qubit_info_widget_layout = QtWidgets.QHBoxLayout()
        qubit_info_widget_layout.addStretch()
        self.qubit_info_widget = CircuitQubitInlineInformation(self)
        qubit_info_widget_layout.addWidget(self.qubit_info_widget)
        qubit_info_widget_layout.addStretch()

        self.layout.addLayout(qubit_info_widget_layout)
        self.layout.addStretch(1)

        self.setLayout(self.layout)

    def reset_combobox(self) -> None:
        self.selectable_qubit_labels_combobox.clear()
        self.selectable_qubit_labels_combobox.setDisabled(True)

    def set_lookup_information(self, annotatable_quantum_computation: syrec.annotatable_quantum_computation) -> None:
        self.reset_combobox()
        self.annotatable_quantum_computation = annotatable_quantum_computation

        sorted_qubit_labels: list[str] = []
        for i in range(self.annotatable_quantum_computation.num_qubits):
            internal_qubit_label: str | None = self.annotatable_quantum_computation.get_qubit_label(
                i, syrec.qubit_label_type.internal
            )

            # Fetching the internal qubit label for a valid qubit of the annotatable quantum computation should not fail but we handle the error case nevertheless.
            if internal_qubit_label is None:
                show_error_dialog(
                    "Error generating internal qubit label",
                    "Failed to build internal qubit label for qubit " + str(i) + "! This should not happen.",
                )
                continue

            # Since the qubits generated for SyReC variables of type 'in' are also considered garbage we need to also filter the clickable qubits to only consider qubits whose label starts with the prefix "__q" (marking qubits generated for local variables of a SyReC module).
            if (
                self.annotatable_quantum_computation.is_circuit_qubit_garbage(i)
                or self.annotatable_quantum_computation.is_circuit_qubit_ancillary(i)
            ) and does_qubit_label_start_with_internal_qubit_label_prefix(internal_qubit_label):
                self.qubits_labels_of_local_variables_lookup.add(internal_qubit_label)
                sorted_qubit_labels.append(str(CircuitViewQubitLabel(i, internal_qubit_label)))

        self.selectable_qubit_labels_combobox.insertItems(0, sorted_qubit_labels)
        if self.selectable_qubit_labels_combobox.count() > 0:
            self.selectable_qubit_labels_combobox.setDisabled(False)
            destringified_combo_box_item_data: CircuitViewQubitLabel | None = CircuitViewQubitLabel.load_from_string(
                self.selectable_qubit_labels_combobox.itemText(0)
            )

            # The destringification should not but could fail but we handle the error case nevertheless.
            if destringified_combo_box_item_data is None:
                show_error_dialog(
                    "Error setting initially select qubit in combobox",
                    "Failed to convert the text of the chosen default selected element of the combobox: \n"
                    + self.selectable_qubit_labels_combobox.itemText(0)
                    + "\n"
                    + "into its internal DTO representation! This should not happen and indicates an internal error!",
                )
                self.search_and_display_information_for_qubit(
                    CircuitViewQubitLabel(-1, ""), update_combobox_selection=True
                )
            else:
                self.search_and_display_information_for_qubit(
                    destringified_combo_box_item_data, update_combobox_selection=True
                )
        else:
            self.search_and_display_information_for_qubit(CircuitViewQubitLabel(-1, ""), update_combobox_selection=True)

    def clear(self) -> None:
        self.qubits_labels_of_local_variables_lookup.clear()
        self.reset_combobox()
        self.qubit_info_widget.clear_and_hide_all_inline_data_controls()

    def search_and_display_information_for_qubit(
        self, qubit_internal_label_and_index: CircuitViewQubitLabel, update_combobox_selection: bool
    ) -> None:
        if qubit_internal_label_and_index.internal_qubit_label not in self.qubits_labels_of_local_variables_lookup:
            if update_combobox_selection:
                self.selectable_qubit_labels_combobox.setCurrentIndex(-1)
            self.qubit_info_widget.clear_and_hide_all_inline_data_controls()
            return

        if update_combobox_selection:
            combobox_item_idx_matching_label = self.selectable_qubit_labels_combobox.findText(
                str(qubit_internal_label_and_index)
            )
            if combobox_item_idx_matching_label == -1:
                show_error_dialog(
                    "Error updating information for selected qubit",
                    "Could not find matching item for qubit:\n"
                    + str(qubit_internal_label_and_index)
                    + "\nin combobox defining qubits for which inline information can be displayed!\n"
                    + "This should not happen and is an internal error!",
                )
                self.selectable_qubit_labels_combobox.setCurrentIndex(-1)
                self.qubit_info_widget.clear_and_hide_all_inline_data_controls()
                return
            self.selectable_qubit_labels_combobox.setCurrentIndex(combobox_item_idx_matching_label)

        self.qubit_info_widget.toggle_all_inline_information_controls(show_inline_information=True)
        inline_information_of_qubit: syrec.inlined_qubit_information | None = (
            self.annotatable_quantum_computation.get_inlined_qubit_information(
                qubit_internal_label_and_index.associated_qubit
            )
            if self.annotatable_quantum_computation is not None
            else None
        )

        if inline_information_of_qubit is not None:
            self.qubit_info_widget.update_information(
                qubit_internal_label_and_index.internal_qubit_label,
                inline_information_of_qubit.user_declared_qubit_label,
                inline_information_of_qubit.inline_stack,
            )
        else:
            self.qubit_info_widget.update_information(
                qubit_internal_label_and_index.internal_qubit_label,
                None,
                None,
            )

    def handle_combobox_selection_change(self, new_combobox_idx: int) -> None:
        if new_combobox_idx == -1:
            return

        destringified_combobox_label_data: CircuitViewQubitLabel | None = CircuitViewQubitLabel.load_from_string(
            self.selectable_qubit_labels_combobox.itemText(new_combobox_idx)
        )
        if destringified_combobox_label_data is not None:
            self.search_and_display_information_for_qubit(
                destringified_combobox_label_data, update_combobox_selection=False
            )
        else:
            show_error_dialog(
                "Error during qubit label combobox selection change",
                "Failed to map combobox item (index="
                + str(new_combobox_idx)
                + ") text:\n"
                + self.selectable_qubit_labels_combobox.itemText(new_combobox_idx)
                + "\nto internal DTO! This should not happen and is an internal error!",
            )


class ConfigurableOptionsUpdated(QtWidgets.QDialog):  # type: ignore[misc]
    def __init__(self, parent: QtWidgets.QWidget, configurable_settings: syrec.configurable_options) -> None:
        super().__init__()
        self.parent = parent
        self.configurable_parser_and_synthesis_options = configurable_settings

        layout = QtWidgets.QVBoxLayout(self)
        expected_main_module_identifier_layout = QtWidgets.QHBoxLayout()
        expected_main_module_identifier_label = QtWidgets.QLabel("Expected main module identifier:")
        self.expected_main_module_identifier_textbox = QtWidgets.QLineEdit()
        if self.configurable_parser_and_synthesis_options is not None:
            identifier: str | None = self.configurable_parser_and_synthesis_options.main_module_identifier
            self.expected_main_module_identifier_textbox.setText(identifier or "")

        self.expected_main_module_identifier_textbox.setPlaceholderText(
            "Leave blank if last declared module of SyReC program should be used as main module..."
        )
        module_identifier_regular_expr = QtCore.QRegularExpression(R"(^([_A-Za-z]\w*)?$)")
        module_identifier_validator = QtGui.QRegularExpressionValidator(module_identifier_regular_expr, self)
        self.expected_main_module_identifier_textbox.setValidator(module_identifier_validator)

        expected_main_module_identifier_layout.addWidget(expected_main_module_identifier_label)
        expected_main_module_identifier_layout.addWidget(self.expected_main_module_identifier_textbox)

        generate_inlined_qubit_debug_information_layout = QtWidgets.QHBoxLayout()
        generate_inlined_qubit_debug_information_label = QtWidgets.QLabel("Generate inlined qubit debug information:")
        self.generate_inlined_qubit_debug_information_checkbox = QtWidgets.QCheckBox()
        if self.configurable_parser_and_synthesis_options.generate_inlined_qubit_debug_information:
            self.generate_inlined_qubit_debug_information_checkbox.setCheckState(QtCore.Qt.CheckState.Checked)
        else:
            self.generate_inlined_qubit_debug_information_checkbox.setCheckState(QtCore.Qt.CheckState.Unchecked)

        generate_inlined_qubit_debug_information_layout.addWidget(generate_inlined_qubit_debug_information_label)
        generate_inlined_qubit_debug_information_layout.addWidget(
            self.generate_inlined_qubit_debug_information_checkbox
        )
        generate_inlined_qubit_debug_information_layout.addStretch()

        integer_constant_truncation_operation_selection_layout = QtWidgets.QHBoxLayout()
        integer_constant_truncation_operation_combobox_label = QtWidgets.QLabel(
            "Integer constant truncation operation:"
        )
        self.integer_constant_truncation_operation_combobox = QtWidgets.QComboBox()
        self.integer_constant_truncation_operation_combobox.addItems([
            syrec.integer_constant_truncation_operation.bitwise_and.name,
            syrec.integer_constant_truncation_operation.modulo.name,
        ])
        to_be_selected_integer_constant_truncation_operation_idx: int = (
            self.integer_constant_truncation_operation_combobox.findText(
                self.configurable_parser_and_synthesis_options.integer_constant_truncation_operation.name
            )
        )
        if to_be_selected_integer_constant_truncation_operation_idx == -1:
            show_error_dialog(
                "Error setting selected integer constant truncation operation in combobox",
                "Failed to determine matching element for integer constant truncation operation '"
                + self.configurable_parser_and_synthesis_options.integer_constant_truncation_operation.name
                + "' in combobox, this should not happen! Defaulting to first entry of combobox",
            )
            to_be_selected_integer_constant_truncation_operation_idx = 0

        self.integer_constant_truncation_operation_combobox.setCurrentIndex(
            to_be_selected_integer_constant_truncation_operation_idx
        )

        integer_constant_truncation_operation_selection_layout.addWidget(
            integer_constant_truncation_operation_combobox_label
        )
        integer_constant_truncation_operation_selection_layout.addWidget(
            self.integer_constant_truncation_operation_combobox
        )
        integer_constant_truncation_operation_selection_layout.addStretch()

        default_bitwidth_layout = QtWidgets.QHBoxLayout()
        default_bitwidth_label = QtWidgets.QLabel("Default signal bitwidth:")
        self.default_bitwidth_textbox = QtWidgets.QLineEdit()
        self.default_bitwidth_textbox.setText(str(self.configurable_parser_and_synthesis_options.default_bitwidth))
        self.default_bitwidth_textbox.setPlaceholderText("Valid value range is [1, 2^31)")
        # The value range of the default bitwidth is restricted due to python having no built-in unsigned integer type.
        self.default_bitwidth_textbox.setValidator(QtGui.QIntValidator(1, 2147483647))

        default_bitwidth_layout.addWidget(default_bitwidth_label)
        default_bitwidth_layout.addWidget(self.default_bitwidth_textbox)
        default_bitwidth_layout.addStretch()

        save_settings_button_layout = QtWidgets.QHBoxLayout()
        save_settings_button_layout.addStretch()

        save_settings_button = QtWidgets.QPushButton("Save")
        save_settings_button.clicked.connect(self.save_settings)
        save_settings_button_layout.addWidget(save_settings_button)
        save_settings_button_layout.addStretch()

        layout.addLayout(expected_main_module_identifier_layout)
        layout.addLayout(generate_inlined_qubit_debug_information_layout)
        layout.addLayout(integer_constant_truncation_operation_selection_layout)
        layout.addLayout(default_bitwidth_layout)
        layout.addLayout(save_settings_button_layout)
        layout.addStretch()
        self.setLayout(layout)

    def save_settings(self) -> QtWidgets.QDialog.DialogCode:
        mapped_to_integer_constant_truncation_operation: syrec.integer_constant_truncation_operation | None = None
        try:
            mapped_to_integer_constant_truncation_operation = getattr(
                syrec.integer_constant_truncation_operation,
                self.integer_constant_truncation_operation_combobox.currentText(),
            )
        except AttributeError:
            show_error_dialog(
                "Error updating integer constant truncation operation",
                "Failed to map selected integer constant truncation operation '"
                + self.integer_constant_truncation_operation_combobox.currentText()
                + "' to matching enum value! This should not happen.",
            )
            return self.reject()

        if not self.expected_main_module_identifier_textbox.hasAcceptableInput():
            show_error_dialog(
                "Error updating expected main module identifier",
                "Invalid main module identifier '" + self.expected_main_module_identifier_textbox.text() + "'",
            )
            return self.reject()

        if not self.default_bitwidth_textbox.hasAcceptableInput():
            show_error_dialog(
                "Error updating default bitwidth",
                "Invalid default bitwidth '" + self.default_bitwidth_textbox.text() + "'",
            )
            return self.reject()

        self.configurable_parser_and_synthesis_options.main_module_identifier = (
            self.expected_main_module_identifier_textbox.text() or None
        )
        self.configurable_parser_and_synthesis_options.generate_inlined_qubit_debug_information = (
            self.generate_inlined_qubit_debug_information_checkbox.isChecked()
        )
        self.configurable_parser_and_synthesis_options.integer_constant_truncation_operation = (
            mapped_to_integer_constant_truncation_operation
        )
        self.configurable_parser_and_synthesis_options.default_bitwidth = int(self.default_bitwidth_textbox.text())
        return self.accept()


class MainWindow(QtWidgets.QMainWindow):  # type: ignore[misc]
    def __init__(self, parent: QtWidgets.QWidget | None = None) -> None:
        QtWidgets.QWidget.__init__(self, parent)

        self.setWindowTitle("SyReC Editor")

        self.setup_widgets()
        self.setup_dock_widgets()
        self.setup_actions()
        self.setup_toolbar()

    def setup_widgets(self) -> None:
        self.editor = QtSyReCEditor(self)
        self.viewer = CircuitView(parent=self)
        self.qubits_information_lookup = CircuitQubitsInformationLookup(parent=self)

        self.viewer.qubitLabelClicked.connect(self.handle_qubit_label_click_of_circuit_view)

        variable_info_search_circuit_view_splitter = QtWidgets.QSplitter(QtCore.Qt.Orientation.Horizontal, self)
        variable_info_search_circuit_view_splitter.addWidget(self.qubits_information_lookup)
        variable_info_search_circuit_view_splitter.addWidget(self.viewer)
        variable_info_search_circuit_view_splitter.setStretchFactor(1, 10)

        splitter = QtWidgets.QSplitter(QtCore.Qt.Orientation.Vertical, self)
        splitter.addWidget(self.editor.widget)
        splitter.addWidget(variable_info_search_circuit_view_splitter)

        self.setCentralWidget(splitter)

    def setup_dock_widgets(self) -> None:
        self.logWidget = LogWidget(self)
        self.logDockWidget = QtWidgets.QDockWidget("Log Messages", self)
        self.logDockWidget.setWidget(self.logWidget)
        self.addDockWidget(QtCore.Qt.DockWidgetArea.BottomDockWidgetArea, self.logDockWidget)

    def setup_actions(self) -> None:
        self.editor.before_build = self.clear_error_log_and_circuit_view
        self.editor.build_successful = self.update_circuit_view_and_qubit_information
        self.editor.parser_failed = self.logWidget.addMessage
        self.editor.build_failed = self.filter_and_record_parser_errors

    def handle_qubit_label_click_of_circuit_view(self, stringified_circuit_view_qubit_label: str) -> None:
        destringified_circuit_view_qubit_label = CircuitViewQubitLabel.load_from_string(
            stringified_circuit_view_qubit_label
        )

        if destringified_circuit_view_qubit_label is None:
            show_error_dialog(
                "Error during parsing of clicked qubit label of circuit view",
                "Failed to convert the clicked qubit label in the circuit view: \n"
                + stringified_circuit_view_qubit_label
                + "\n"
                + "into its internal DTO representation! This should not happen and indicates an internal error!",
            )
        else:
            self.qubits_information_lookup.search_and_display_information_for_qubit(
                destringified_circuit_view_qubit_label, update_combobox_selection=True
            )

    def filter_and_record_parser_errors(self, aggregate_error_string: str) -> None:
        regex_pattern = r"(-- line (\d+) col (\d+): (.*)(\n?))"
        if re.search(regex_pattern, aggregate_error_string) is not None:
            for m in re.finditer(regex_pattern, aggregate_error_string):
                self.logWidget.addMessage(m.group(0))
        else:
            self.logWidget.addMessage("No matching lines found in error message")

    def update_circuit_view_and_qubit_information(
        self, annotatable_quantum_computation: syrec.annotatable_quantum_computation
    ) -> None:
        self.viewer.load(annotatable_quantum_computation)
        self.qubits_information_lookup.set_lookup_information(annotatable_quantum_computation)

    def clear_error_log_and_circuit_view(self) -> None:
        self.logWidget.clear()
        self.viewer.clear()
        self.qubits_information_lookup.clear()

    def setup_toolbar(self) -> None:
        toolbar = self.addToolBar("Main")
        toolbar.setIconSize(QtCore.QSize(32, 32))

        toolbar.addAction(self.editor.open_action)
        toolbar.addAction(self.editor.build_action)
        toolbar.addAction(self.editor.sim_action)
        toolbar.addAction(self.editor.stat_action)
        toolbar.addWidget(self.editor.buttonCostAware)
        toolbar.addWidget(self.editor.buttonLineAware)
        toolbar.addWidget(self.editor.configurable_parser_and_synthesis_options_update_button)


def main() -> int:
    a = QtWidgets.QApplication([])

    w = MainWindow()
    w.show()

    return cast("int", a.exec())


if __name__ == "__main__":
    main()
