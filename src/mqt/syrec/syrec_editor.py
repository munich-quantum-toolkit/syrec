# Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
# Copyright (c) 2025 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

from __future__ import annotations

import re
from pathlib import Path
from typing import TYPE_CHECKING, Any, cast, NamedTuple

from mqt.core.ir.operations import OpType
from PyQt6 import QtCore, QtGui, QtWidgets

from mqt import syrec

if TYPE_CHECKING:
    from collections.abc import Callable


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
        self.lines: list[CircuitLineItem] = []
        self.inputs: list[QtWidgets.QGraphicsTextItem | None] = []
        self.outputs: list[QtWidgets.QGraphicsTextItem | None] = []
        if annotatable_quantum_computation is not None:
            self.load(annotatable_quantum_computation)

    def clear(self) -> None:
        self.scene().clear()

        self.annotatable_quantum_computation = None
        self.lines = []
        self.inputs = []
        self.outputs = []

    def load(self, annotatable_quantum_computation: syrec.annotatable_quantum_computation) -> None:
        self.clear()

        self.annotatable_quantum_computation = annotatable_quantum_computation
        n_quantum_ops = self.annotatable_quantum_computation.num_ops
        width = 30 * n_quantum_ops

        for i in range(self.annotatable_quantum_computation.num_qubits):
            line = CircuitLineItem(i, n_quantum_ops)
            self.lines.append(line)
            self.scene().addItem(line)
            self.inputs.append(
                self.add_line_label(
                    0,
                    i * 30,
                    self.annotatable_quantum_computation.qubit_labels[i],
                    QtCore.Qt.AlignmentFlag.AlignRight,
                    self.annotatable_quantum_computation.is_circuit_qubit_ancillary(i),
                )
            )
            self.outputs.append(
                self.add_line_label(
                    width,
                    i * 30,
                    self.annotatable_quantum_computation.qubit_labels[i],
                    QtCore.Qt.AlignmentFlag.AlignLeft,
                    self.annotatable_quantum_computation.is_circuit_qubit_garbage(i),
                )
            )

        for i in range(n_quantum_ops):
            gate = GateItem(self.annotatable_quantum_computation, i)
            gate.setPos(i * 30 + 15, 0)
            self.scene().addItem(gate)

        #inlined_variable_vertical_offset = int(-self.inputs[0].boundingRect().width() - 30)
        #self.add_inlined_variable_information(inlined_variable_vertical_offset, 0, QtCore.Qt.AlignmentFlag.AlignRight)

    def add_line_label(
        self, x: int, y: int, text: str, align: QtCore.Qt.AlignmentFlag, color: bool
    ) -> QtWidgets.QGraphicsTextItem | None:
        text_item = self.scene().addText(text)
        text_item.setPlainText(text)

        if align == QtCore.Qt.AlignmentFlag.AlignRight:
            x -= text_item.boundingRect().width()

        text_item.setPos(x, y - 12)

        if color:
            text_item.setDefaultTextColor(QtGui.QColorConstants.Red)

        return text_item

    def add_inlined_variable_information(self, x: int, y: int, align: QtCore.Qt.AlignmentFlag) -> None:
        info_button = QtWidgets.QPushButton(icon=QtGui.QIcon.fromTheme("dialog-information"))
        info_button.setGeometry(x, y, 30, 30)
        #info_button.setStyleSheet("QPushButton { border-radius: 50 }")
        #info_button.setGeometry(QtCore.QRect(x, y, 10, 10))
        #info_button.setFixedSize(10,10)
        self.scene().addWidget(info_button)

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

        error_string = self.prog.read_from_string(self.getText())

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
            syrec.cost_aware_synthesis(self.annotatable_quantum_computation, self.prog)
        else:
            syrec.line_aware_synthesis(self.annotatable_quantum_computation, self.prog)

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

        settings = syrec.properties()

        for i in input_list:
            my_inp_bitset = syrec.n_bit_values_container(no_of_bits, i)
            my_out_bitset = syrec.n_bit_values_container(no_of_bits)
            syrec.simple_simulation(my_out_bitset, self.annotatable_quantum_computation, my_inp_bitset, settings)

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
            input_signal = QtWidgets.QTableWidgetItem(self.annotatable_quantum_computation.qubit_labels[i])
            input_signal.setTextAlignment(QtCore.Qt.AlignmentFlag.AlignCenter)
            self.table.setItem(1, i, QtWidgets.QTableWidgetItem(input_signal))

            output_signal = QtWidgets.QTableWidgetItem(self.annotatable_quantum_computation.qubit_labels[i])
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

class LineNumberAndCallTypeTuple(NamedTuple):
    lineNumber: int
    callType: str

class CircuitQubitInlineInformation(QtWidgets.QWidget):
    def __init__(self, parent: QtWidgets.QWidget | None = None) -> None:
        super().__init__()
        self.parent = parent
        
        layout = QtWidgets.QVBoxLayout(self)
        
        nonStackInfoLayout = QtWidgets.QGridLayout()
        associatedModuleSignatureLabel = QtWidgets.QLabel("Associated module signature:")
        self.associatedModuleSignatureValue = QtWidgets.QLabel("module main(inout a(4))")
        nonStackInfoLayout.addWidget(associatedModuleSignatureLabel, 0, 0, 1, 1, QtCore.Qt.AlignmentFlag.AlignLeft)
        nonStackInfoLayout.addWidget(self.associatedModuleSignatureValue, 0, 1, 1, 1, QtCore.Qt.AlignmentFlag.AlignLeft)
        
        originalQubitLabel = QtWidgets.QLabel("Original qubit label:")
        self.originalQubitLabelValue = QtWidgets.QLabel("")
        nonStackInfoLayout.addWidget(originalQubitLabel, 1, 0, 1, 1, QtCore.Qt.AlignmentFlag.AlignLeft)
        nonStackInfoLayout.addWidget(self.originalQubitLabelValue, 1, 1, 1, 1, QtCore.Qt.AlignmentFlag.AlignLeft)
        
        internalQubitLabel = QtWidgets.QLabel("Internal qubit label:")
        self.internalQubitLabelValue = QtWidgets.QLabel("")
        nonStackInfoLayout.addWidget(internalQubitLabel, 2, 0, 1, 1, QtCore.Qt.AlignmentFlag.AlignLeft)
        nonStackInfoLayout.addWidget(self.internalQubitLabelValue, 2, 1, 1, 1, QtCore.Qt.AlignmentFlag.AlignLeft)
        nonStackInfoLayout.rowStretch(1)
        
        inlineStackTreeLayout = QtWidgets.QVBoxLayout()
        inlineStackTreeViewLabel = QtWidgets.QLabel("Inline stack")
        inlineStackTreeViewLabel.setAlignment(QtCore.Qt.AlignmentFlag.AlignCenter)
        
        self.inlineStackTreeView = QtWidgets.QTreeView()
        self.inlineStackTreeView.setHeaderHidden(True)
        
        self.inlineStackTreeModel = QtGui.QStandardItemModel()
        self.inlineStackTreeModelRoot = self.inlineStackTreeModel.invisibleRootItem()
        
        row1 = self.addInlineStackEntryToTreeView("module main(inout a(4))", LineNumberAndCallTypeTuple(102, "CALL"))
        row2 = self.addInlineStackEntryToTreeView("module other(inout a(4))", LineNumberAndCallTypeTuple(12, "UNCALL"))
        row3 = self.addInlineStackEntryToTreeView("module add(inout a(4))", None)
        row2.appendRow(row3)
        row1.appendRow(row2)
        
        self.inlineStackTreeModelRoot.appendRow(row1)
        
        self.inlineStackTreeView.setModel(self.inlineStackTreeModel)
        inlineStackTreeLayout.addWidget(inlineStackTreeViewLabel)
        inlineStackTreeLayout.addWidget(self.inlineStackTreeView)
        
        layout.addLayout(nonStackInfoLayout)
        layout.addLayout(inlineStackTreeLayout)
        self.layout = layout
        self.setLayout(self.layout)
        
    def updateInformation(self, parentModuleSignature: str, originalQubitLabel: str, internalQubitLabel: str) -> None:
        self.associatedModuleSignatureValue.setText(parentModuleSignature)
        self.originalQubitLabelValue.setText(originalQubitLabel)
        self.internalQubitLabelValue.setText(internalQubitLabel)
        
    def clear(self) -> None:
        self.associatedModuleSignatureValue.clear()
        self.originalQubitLabelValue.clear()
        self.internalQubitLabelValue.clear()
        
    def addInlineStackEntryToTreeView(self, signature: str, optionalLineAndCallTypeTuple: LineNumberAndCallTypeTuple | None) -> QtGui.QStandardItem:
        treeEntry = QtGui.QStandardItem(signature)
        boldFont = QtGui.QFont()
        boldFont.setBold(True)
        #QtGui.QFont("Times", 12)
        treeEntry.setFont(boldFont)
        treeEntry.setEditable(False)
        
        if optionalLineAndCallTypeTuple is not None:    
            sourceCodeLineNumberTreeEntry = QtGui.QStandardItem("Line: " + str(optionalLineAndCallTypeTuple.lineNumber))
            sourceCodeLineNumberTreeEntry.setEditable(False)
        
            callTypeTreeEntry = QtGui.QStandardItem("Call type: " + optionalLineAndCallTypeTuple.callType)
            callTypeTreeEntry.setEditable(False)
        
            treeEntry.appendColumn([sourceCodeLineNumberTreeEntry, callTypeTreeEntry])
        return treeEntry
        

class CircuitQubitsInformationLookup(QtWidgets.QWidget):  # type: ignore[misc]
    def __init__(self, parent: QtWidgets.QWidget | None = None) -> None:
        super().__init__()
        self.parent = parent
        self.lookupInformation : Dict[str, str] = {}
        
        self.layout = QtWidgets.QVBoxLayout(self)
        
        searchControlsLayout = QtWidgets.QHBoxLayout()
        qubitSearchFieldLabel = QtWidgets.QLabel("Qubit label: ")
        self.selectableQubitLabelsComboBox = QtWidgets.QComboBox()
        self.selectableQubitLabelsComboBox.setPlaceholderText("<SELECT A QUBIT_LABEL>")
        self.selectableQubitLabelsComboBox.currentIndexChanged.connect(self.handleComboBoxSelectionChange)
        self.disableControls()
        
        searchControlsLayout.addWidget(qubitSearchFieldLabel)
        searchControlsLayout.addWidget(self.selectableQubitLabelsComboBox)
        searchControlsLayout.addStretch(1)
        self.layout.addLayout(searchControlsLayout)
        
        self.displayedQubitInfoWidget = CircuitQubitInlineInformation(self)
        
        self.displayedQubitInformation = QtWidgets.QLabel("")
        self.layout.addWidget(self.displayedQubitInformation)
        self.layout.addWidget(self.displayedQubitInfoWidget)
        self.layout.addStretch(1)
        self.setLayout(self.layout)
   
    def setLookupInformation(self, annotatable_quantum_computation: syrec.annotatable_quantum_computation) -> None:
        self.lookupInformation.clear()
        
        for i in range(annotatable_quantum_computation.num_qubits):
            if not annotatable_quantum_computation.is_circuit_qubit_garbage(i):
               continue               
            self.lookupInformation.update({annotatable_quantum_computation.qubit_labels[i]: annotatable_quantum_computation.qubit_labels[i]})
        
        self.selectableQubitLabelsComboBox.clear()
        self.selectableQubitLabelsComboBox.insertItems(0, self.lookupInformation.values())
        if self.selectableQubitLabelsComboBox.count() > 0:
           self.searchAndDisplayInformationForQubit(self.selectableQubitLabelsComboBox.itemText(0), True)
        else:
            self.selectableQubitLabelsComboBox.setCurrentIndex(-1)
            self.displayedQubitInformation.clear()
        
    def enableControls(self) -> None:
       self.selectableQubitLabelsComboBox.setDisabled(False)
       return
        
    def disableControls(self) -> None:
       self.selectableQubitLabelsComboBox.setDisabled(True)
       return

    def clear(self) -> None:
        self.lookupInformation.clear()
        self.selectableQubitLabelsComboBox.clear()
        self.displayedQubitInformation.clear()
        self.disableControls()
        self.displayedQubitInfoWidget.clear()

    def searchAndDisplayInformationForQubit(self, qubitLabel: str, updateComboBoxSelection: bool) -> None:
       if qubitLabel not in self.lookupInformation:
         if updateComboBoxSelection:
            self.selectableQubitLabelsCombobox.setCurrentIndex(-1)
            
         self.displayedQubitInfoWidget.clear()
         return
       
       if updateComboBoxSelection:
            comboBoxItemMatchingLabel = self.selectableQubitLabelsComboBox.findText(qubitLabel)
            if comboBoxItemMatchingLabel == -1:
                msg = QtWidgets.QMessageBox()
                msg.setBaseSize(QtCore.QSize(300, 200))
                msg.setInformativeText("While the internal lookup information did contain a qubit with a label equal to " + qubitLabel + ", the combobox did not! This should not happend.")
                msg.setWindowTitle("Error updating information for selected qubit")
                msg.setStandardButtons(QtWidgets.QMessageBox.StandardButton.Error)
                msg.exec()
                self.selectableQubitLabelsCombobox.setCurrentIndex(-1)
                self.displayedQubitInfoWidget.clear()
                return
            else:
                self.selectableQubitLabelsComboBox.setCurrentIndex(comboBoxItemMatchingLabel)
    
       self.displayedQubitInfoWidget.updateInformation("", qubitLabel, qubitLabel)

    def handleComboBoxSelectionChange(self, newlySelectedIndex: int) -> None:
        if newlySelectedIndex == -1:
           return
        self.searchAndDisplayInformationForQubit(self.selectableQubitLabelsComboBox.itemText(newlySelectedIndex), False)

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

    def filter_and_record_parser_errors(self, aggregate_error_string: str) -> None:
        regex_pattern = r"(-- line (\d+) col (\d+): (.*)(\n?))"
        if re.search(regex_pattern, aggregate_error_string) is not None:
            for m in re.finditer(regex_pattern, aggregate_error_string):
                self.logWidget.addMessage(m.group(0))
        else:
            self.logWidget.addMessage("No matching lines found in error message")

    def update_circuit_view_and_qubit_information(self, annotatable_quantum_computation: syrec.annotatable_quantum_computation) -> None:
        self.viewer.load(annotatable_quantum_computation)
        self.qubits_information_lookup.setLookupInformation(annotatable_quantum_computation)
        self.qubits_information_lookup.enableControls()

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


def main() -> int:
    a = QtWidgets.QApplication([])

    w = MainWindow()
    w.show()

    return cast("int", a.exec())


if __name__ == "__main__":
    main()
