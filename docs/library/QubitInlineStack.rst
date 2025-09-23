QubitInlineStack
========================

Utility class to track the origin of a qubit in a hierarchy of Call-/UncallStatements.

    .. autoclass:: mqt.syrec.qubit_inlining_stack
        :undoc-members:
        :members:
        :special-members: __getitem__

Contains information about the source code line as well as the type of call performed to call/uncall the target module.

    .. autoclass:: mqt.syrec.qubit_inlining_stack_entry
        :undoc-members:
        :members:
