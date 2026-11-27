InlinedQubitInformation
========================

Stores debug information about the ancillary and local module variable qubits that can be used to determine the origin of the qubit in the SyReC program or to determine the user declared identifier of the associated variable for a qubit. This information is not available for the parameters of a SyReC module.

    .. autoclass:: mqt.syrec.InlinedQubitInformation
        :undoc-members:
        :members:

Utility class to track the origin of a qubit in a hierarchy of Call-/UncallStatements.

    .. autoclass:: mqt.syrec.QubitInliningStack
        :undoc-members:
        :members:
        :special-members: __getitem__

Contains information about the source code line as well as the type of call performed to call/uncall the target module.

    .. autoclass:: mqt.syrec.QubitInliningStackEntry
        :undoc-members:
        :members:
