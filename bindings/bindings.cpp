/*
 * Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
 * Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "algorithms/simulation/simple_simulation.hpp"
#include "algorithms/synthesis/syrec_cost_aware_synthesis.hpp"
#include "algorithms/synthesis/syrec_line_aware_synthesis.hpp"
#include "core/annotatable_quantum_computation.hpp"
#include "core/configurable_options.hpp"
#include "core/n_bit_values_container.hpp"
#include "core/qubit_inlining_stack.hpp"
#include "core/statistics.hpp"
#include "core/syrec/parser/utils/syrec_operation_utils.hpp"
#include "core/syrec/program.hpp"
#include "ir/QuantumComputation.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <nanobind/nanobind.h>
#include <nanobind/stl/map.h>         // NOLINT(misc-include-cleaner)
#include <nanobind/stl/optional.h>    // NOLINT(misc-include-cleaner)
#include <nanobind/stl/shared_ptr.h>  // NOLINT(misc-include-cleaner)
#include <nanobind/stl/string.h>      // NOLINT(misc-include-cleaner)
#include <nanobind/stl/string_view.h> // NOLINT(misc-include-cleaner)
#include <optional>

namespace nb = nanobind;
using namespace nb::literals;
using namespace syrec;

namespace {

    using namespace nb;

    // nanobind-compatible implementation of scoped_estream_redirect
    // Taken from https://github.com/wjakob/nanobind/discussions/413

    // Buffer that writes to Python instead of C++
    class pythonbuf: public std::streambuf {
    private:
        using traits_type = std::streambuf::traits_type;

        size_t                  buf_size;
        std::unique_ptr<char[]> d_buffer;
        nb::object              pywrite;
        nb::object              pyflush;

        int overflow(int c) override {
            if (!traits_type::eq_int_type(c, traits_type::eof())) {
                *pptr() = traits_type::to_char_type(c);
                pbump(1);
            }
            return sync() == 0 ? traits_type::not_eof(c) : traits_type::eof();
        }

        // Computes how many bytes at the end of the buffer are part of an
        // incomplete sequence of UTF-8 bytes.
        // Precondition: pbase() < pptr()
        size_t utf8_remainder() const {
            const auto rbase         = std::reverse_iterator<char*>(pbase());
            const auto rpptr         = std::reverse_iterator<char*>(pptr());
            auto       is_ascii      = [](char c) { return (static_cast<unsigned char>(c) & 0x80) == 0x00; };
            auto       is_leading    = [](char c) { return (static_cast<unsigned char>(c) & 0xC0) == 0xC0; };
            auto       is_leading_2b = [](char c) { return static_cast<unsigned char>(c) <= 0xDF; };
            auto       is_leading_3b = [](char c) { return static_cast<unsigned char>(c) <= 0xEF; };
            // If the last character is ASCII, there are no incomplete code points
            if (is_ascii(*rpptr)) {
                return 0;
            }
            // Otherwise, work back from the end of the buffer and find the first
            // UTF-8 leading byte
            const auto rpend   = rbase - rpptr >= 3 ? rpptr + 3 : rbase;
            const auto leading = std::find_if(rpptr, rpend, is_leading);
            if (leading == rbase) {
                return 0;
            }
            const auto dist      = static_cast<size_t>(leading - rpptr);
            size_t     remainder = 0;

            if (dist == 0) {
                remainder = 1; // 1-byte code point is impossible
            } else if (dist == 1) {
                remainder = is_leading_2b(*leading) ? 0 : dist + 1;
            } else if (dist == 2) {
                remainder = is_leading_3b(*leading) ? 0 : dist + 1;
            }
            // else if (dist >= 3), at least 4 bytes before encountering an UTF-8
            // leading byte, either no remainder or invalid UTF-8.
            // Invalid UTF-8 will cause an exception later when converting
            // to a Python string, so that's not handled here.
            return remainder;
        }

        // This function must be non-virtual to be called in a destructor.
        int _sync() {
            if (pbase() != pptr()) { // If buffer is not empty
                //nb::gil_scoped_acquire tmp;
                // This subtraction cannot be negative, so dropping the sign.
                auto   size      = static_cast<size_t>(pptr() - pbase());
                size_t remainder = utf8_remainder();

                if (size > remainder) {
                    nb::str line(pbase(), size - remainder);
                    pywrite(std::move(line));
                    pyflush();
                }

                // Copy the remainder at the end of the buffer to the beginning:
                if (remainder > 0) {
                    std::memmove(pbase(), pptr() - remainder, remainder);
                }
                setp(pbase(), epptr());
                pbump(static_cast<int>(remainder));
            }
            return 0;
        }

        int sync() override { return _sync(); }

    public:
        explicit pythonbuf(const nb::object& pyostream, size_t buffer_size = 1024): buf_size(buffer_size), d_buffer(new char[buf_size]), pywrite(pyostream.attr("write")),
                                                                                    pyflush(pyostream.attr("flush")) {
            setp(d_buffer.get(), d_buffer.get() + buf_size - 1);
        }

        pythonbuf(pythonbuf&&) = default;

        // Sync before destroy
        ~pythonbuf() override { _sync(); }
    };

    class scoped_ostream_redirect {
    protected:
        std::streambuf* old;
        std::ostream&   costream;
        pythonbuf       buffer;

    public:
        explicit scoped_ostream_redirect(
                std::ostream&     costream  = std::cout,
                const nb::object& pyostream = nb::module_::import_("sys").attr("stdout")) noexcept
            : costream(costream), buffer(pyostream) {
            old = costream.rdbuf(&buffer);
        }
        ~scoped_ostream_redirect() {
            costream.rdbuf(old);
        }
        scoped_ostream_redirect(const scoped_ostream_redirect&)            = delete;
        scoped_ostream_redirect(scoped_ostream_redirect&& other)           = default;
        scoped_ostream_redirect& operator=(const scoped_ostream_redirect&) = delete;
        scoped_ostream_redirect& operator=(scoped_ostream_redirect&&)      = delete;
    };

    class scoped_estream_redirect: public scoped_ostream_redirect {
    public:
        explicit scoped_estream_redirect(std::ostream& costream  = std::cerr,
                                         const object& pyostream = module_::import_("sys").attr("stderr")): scoped_ostream_redirect(costream, pyostream) {}
    };

} // namespace

NB_MODULE(MQT_SYREC_MODULE_NAME, m) {
    nb::module_::import_("mqt.core.ir");

    m.doc() = "Python interface for the SyReC programming language for the synthesis of reversible circuits";

    nb::class_<QubitInliningStack::QubitInliningStackEntry>(m, "qubit_inlining_stack_entry")
            .def(nb::init<>(), "Constructs an empty qubit inlining stack entry")
            .def_prop_ro("line_number_of_call_of_target_module", [](const QubitInliningStack::QubitInliningStackEntry& stackEntry) { return stackEntry.lineNumberOfCallOfTargetModule; }, "Returns the line number in the source file in which the call statement variant was defined")
            .def_prop_ro("is_target_module_accessed_via_call_stmt", [](const QubitInliningStack::QubitInliningStackEntry& stackEntry) { return stackEntry.isTargetModuleAccessedViaCallStmt; }, "Returns whether the target module was called using a CallStatement")
            .def_prop_ro("stringified_signature_of_called_module", &QubitInliningStack::QubitInliningStackEntry::stringifySignatureOfCalledModule, "Returns the stringified target module signature");

    nb::class_<QubitInliningStack>(m, "qubit_inlining_stack")
            .def(nb::init<>(), "Constructs an empty qubit inlining stack")
            .def("size", &QubitInliningStack::size, "Get the number of stack entries")
            .def("__getitem__", &QubitInliningStack::getStackEntryAt, "idx"_a, nb::rv_policy::reference_internal);

    nb::class_<AnnotatableQuantumComputation::InlinedQubitInformation>(m, "inlined_qubit_information")
            .def(nb::init<>(), "Constructs an empty inlined qubit information container")
            .def_prop_ro("user_declared_qubit_label", [](const AnnotatableQuantumComputation::InlinedQubitInformation& inlinedQubitInfo) { return inlinedQubitInfo.userDeclaredQubitLabel; }, "Get the label of the qubit as defined by the user in the SyReC program")
            .def_prop_ro("inline_stack", [](const AnnotatableQuantumComputation::InlinedQubitInformation& inlinedQubitInfo) { return inlinedQubitInfo.inlineStack; }, "Get the inline stack associated with the qubit");

    nb::enum_<AnnotatableQuantumComputation::QubitLabelType>(m, "qubit_label_type")
            .value("internal", AnnotatableQuantumComputation::QubitLabelType::Internal, "Generate the qubit label using the internal qubit identifier (only available for ancillary qubits and local SyReC module variables)")
            .value("user_declared", AnnotatableQuantumComputation::QubitLabelType::UserDeclared, "Generate the qubit label using the user declared variable identifier (only available for the qubits of the variables of a SyReC program [ancillary qubits are not associated with a variable and thus have no user declared label])");

    nb::class_<AnnotatableQuantumComputation, qc::QuantumComputation>(m, "annotatable_quantum_computation")
            .def(nb::init<>(), "Constructs an annotatable quantum computation")
            .def(nb::init<bool>(), "generate_quantum_operation_annotations"_a, "Constructs an annotatable quantum computation while also specifying whether quantum operation annotations can be generated")
            .def("get_qubit_label", &AnnotatableQuantumComputation::getQubitLabel, "qubit"_a, "qubit_label_type"_a, "Get either the internal or user-declared label of a qubit as a stringified SyReC variable access based on its location in the quantum register storing the qubit and, optionally, the layout of the SyReC variable stored in the register.")
            .def("get_quantum_cost_for_synthesis", &AnnotatableQuantumComputation::getQuantumCostForSynthesis, "Get the quantum cost to synthesis the quantum computation")
            .def("get_transistor_cost_for_synthesis", &AnnotatableQuantumComputation::getTransistorCostForSynthesis, "Get the transistor cost to synthesis the quantum computation")
            .def("get_annotations_of_quantum_operation", &AnnotatableQuantumComputation::getAnnotationsOfQuantumOperation, "quantum_operation_index_in_quantum_operation"_a, "Get the annotations of a specific quantum operation in the quantum computation")
            .def("get_inlined_qubit_information", &AnnotatableQuantumComputation::getInlinedQubitInformation, "qubit"_a, "Get the inlined information of a qubit");

    nb::class_<NBitValuesContainer>(m, "n_bit_values_container")
            .def(nb::init<>(), "Constructs an empty container of size zero.")
            .def(nb::init<std::size_t>(), "n"_a, "Constructs a zero-initialized container of size n.")
            .def(nb::init<std::size_t, uint64_t>(), "n"_a, "initialLineValues"_a, "Constructs a container of size n from an integer initialLineValues")
            .def("__getitem__", [](const NBitValuesContainer& nBitValuesContainer, std::size_t bitIndex) { return nBitValuesContainer[bitIndex]; })
            .def("test", &NBitValuesContainer::test, "n"_a, "Determine the value of the bit at position n")
            .def("set", nb::overload_cast<std::size_t>(&NBitValuesContainer::set), "n"_a, "Set the value of the bit at position n to TRUE")                 // NOLINT(misc-include-cleaner)
            .def("set", nb::overload_cast<std::size_t, bool>(&NBitValuesContainer::set), "n"_a, "value"_a, "Set the bit at position n to a specific value") // NOLINT(misc-include-cleaner)
            .def("reset", &NBitValuesContainer::reset, "n"_a, "Set the value of the bit at position n to FALSE")
            .def("resize", &NBitValuesContainer::resize, "n"_a, "Changes the number of bits stored in the container")
            .def("size", &NBitValuesContainer::size, "Get the number of values stored in the container")
            .def("flip", &NBitValuesContainer::flip, "n"_a, "Flip the value of the bit at position n")
            .def("__str__", [](const NBitValuesContainer& container) { return container.stringify(); }, "Returns a string containing the stringified values of the stored bits.");

    nb::class_<Statistics>(m, "statistics")
            .def(nb::init<>(), "Constructs an object to record collected statistics.")
            .def_rw("runtime_in_milliseconds", &Statistics::runtimeInMilliseconds, "The recorded runtime in milliseconds");

    nb::enum_<utils::IntegerConstantTruncationOperation>(m, "integer_constant_truncation_operation")
            .value("modulo", utils::IntegerConstantTruncationOperation::Modulo, "Use the modulo operation for the truncation of constant values")
            .value("bitwise_and", utils::IntegerConstantTruncationOperation::BitwiseAnd, "Use the bitwise AND operation for the truncation of constant values");

    nb::class_<ConfigurableOptions>(m, "configurable_options")
            .def(nb::init<>(), "Constructs a configurable options object.")
            .def_rw("default_bitwidth", &ConfigurableOptions::defaultBitwidth, "Defines the default variable bitwidth used by the SyReC parser for variables whose bitwidth specification was omitted")
            .def_rw("integer_constant_truncation_operation", &ConfigurableOptions::integerConstantTruncationOperation, "Defines the operation used by the SyReC parser for the truncation of integer constant values. For further details we refer to the semantics of the SyReC language")
            .def_rw("allow_access_on_assigned_to_variable_parts_in_dimension_access_of_variable_access", &ConfigurableOptions::allowAccessOnAssignedToVariablePartsInDimensionAccessOfVariableAccess, "Defines whether an access on the assigned to signal parts of an assigned is allowed in variable accesses defined in any operand of the assignment. For further details we refer to the semantics of the SyReC language.")
            .def_rw("main_module_identifier", &ConfigurableOptions::optionalProgramEntryPointModuleIdentifier, "Define the identifier of the module serving as the entry-point of the to be processed SyReC program")
            .def_rw("generate_inlined_qubit_debug_information", &ConfigurableOptions::generatedInlinedQubitDebugInformation, "Should debug information for the qubits associated with the local variables of a SyReC module be generated")
            .def_rw("generate_quantum_operation_annotations", &ConfigurableOptions::generateQuantumOperationAnnotations, "Should the optional quantum operation annotations be generated during the synthesis of a SyReC program, disabled by default");

    nb::class_<Program>(m, "program")
            .def(nb::init<>(), "Constructs SyReC program object.")
            .def("add_module", &Program::addModule)
            .def("read", &Program::read, "filename"_a, "configurable_options"_a = ConfigurableOptions(), "Read and process a SyReC program from a file.")
            .def("read_from_string", &Program::readFromString, "stringifiedProgram"_a, "configurable_options"_a = ConfigurableOptions(), "Process an already stringified SyReC program.");

    // Due to the cost and line aware synthesizers reporting found synthesis errors on the std::cerr output stream an explicit redirection to the python sys.stderr output stream is required. However, this should only be a temporary solution and the synthesizer should either use a return value or output parameter to return the found synthesis errors similarly to how the SyReC parser is doing it.
    m.def("cost_aware_synthesis", &CostAwareSynthesis::synthesize, nb::call_guard<scoped_estream_redirect>(), "annotated_quantum_computation"_a, "program"_a, "configurable_options"_a = ConfigurableOptions(), "optional_recorded_statistics"_a = nullptr, "Cost-aware synthesis of the SyReC program.");
    m.def("line_aware_synthesis", &LineAwareSynthesis::synthesize, nb::call_guard<scoped_estream_redirect>(), "annotated_quantum_computation"_a, "program"_a, "configurable_options"_a = ConfigurableOptions(), "optional_recorded_statistics"_a = nullptr, "Line-aware synthesis of the SyReC program.");
    m.def("simple_simulation", &simpleSimulation, "output"_a, "quantum_computation"_a, "input"_a, "optional_recorded_statistics"_a = nullptr, "Simulation of a synthesized SyReC program");
}
