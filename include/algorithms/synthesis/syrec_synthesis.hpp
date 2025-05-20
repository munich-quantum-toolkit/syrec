/*
 * Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
 * Copyright (c) 2025 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#pragma once

#include "core/properties.hpp"
#include "core/syrec/expression.hpp"
#include "core/syrec/module.hpp"
#include "core/syrec/number.hpp"
#include "core/syrec/program.hpp"
#include "core/syrec/statement.hpp"
#include "core/syrec/variable.hpp"
#include "ir/Definitions.hpp"
#include "ir/QuantumComputation.hpp"

#include <map>
#include <stack>
#include <string>
#include <string_view>
#include <vector>

namespace syrec {
    class SyrecSynthesis {
    public:
        std::stack<qc::Qubit>              expOpp;
        std::stack<std::vector<unsigned>>  expLhss;
        std::stack<std::vector<unsigned>>  expRhss;
        bool                               subFlag = false;
        std::vector<unsigned>              opVec;
        std::vector<unsigned>              assignOpVector;
        std::vector<unsigned>              expOpVector;
        std::vector<std::vector<unsigned>> expLhsVector;
        std::vector<std::vector<unsigned>> expRhsVector;

        using VarLinesMap = std::map<Variable::ptr, qc::Qubit>;

        explicit SyrecSynthesis(qc::QuantumComputation& quantumComputation);
        virtual ~SyrecSynthesis() = default;

        void addVariables(const Variable::vec& variables);
        void setMainModule(const Module::ptr& mainModule);
        [[nodiscard]] std::unordered_set<qc::Qubit> getCreatedAncillaryQubits() const { return addedAncillaryQubitIndices; }

    protected:
        constexpr static std::string_view GATE_ANNOTATION_KEY_ASSOCIATED_STATEMENT_LINE_NUMBER = "lno";
        using GateAnnotationsLookup                                                            = std::map<std::string, std::string, std::less<>>;

        virtual bool processStatement(const Statement::ptr& statement) = 0;

        virtual bool onModule(const Module::ptr&);

        virtual bool opRhsLhsExpression([[maybe_unused]] const Expression::ptr& expression, [[maybe_unused]] std::vector<qc::Qubit>& v);
        virtual bool opRhsLhsExpression([[maybe_unused]] const VariableExpression& expression, [[maybe_unused]] std::vector<qc::Qubit>& v);
        virtual bool opRhsLhsExpression([[maybe_unused]] const BinaryExpression& expression, [[maybe_unused]] std::vector<qc::Qubit>& v);

        virtual bool              onStatement(const Statement::ptr& statement);
        virtual bool              onStatement(const AssignStatement& statement);
        virtual bool              onStatement(const IfStatement& statement);
        virtual bool              onStatement(const ForStatement& statement);
        virtual bool              onStatement(const CallStatement& statement);
        virtual bool              onStatement(const UncallStatement& statement);
        bool                      onStatement(const SwapStatement& statement);
        bool                      onStatement(const UnaryStatement& statement);
        [[nodiscard]] static bool onStatement(const SkipStatement& statement);

        virtual bool assignAdd(std::vector<qc::Qubit>& lhs, std::vector<qc::Qubit>& rhs, [[maybe_unused]] unsigned op)      = 0;
        virtual bool assignSubtract(std::vector<qc::Qubit>& lhs, std::vector<qc::Qubit>& rhs, [[maybe_unused]] unsigned op) = 0;
        virtual bool assignExor(std::vector<qc::Qubit>& lhs, std::vector<qc::Qubit>& rhs, [[maybe_unused]] unsigned op)     = 0;

        virtual bool onExpression(const Expression::ptr& expression, std::vector<qc::Qubit>& lines, std::vector<qc::Qubit> const& lhsStat, unsigned op);
        virtual bool onExpression(const BinaryExpression& expression, std::vector<qc::Qubit>& lines, std::vector<qc::Qubit> const& lhsStat, unsigned op);
        virtual bool onExpression(const ShiftExpression& expression, std::vector<qc::Qubit>& lines, std::vector<qc::Qubit> const& lhsStat, unsigned op);
        virtual bool onExpression(const NumericExpression& expression, std::vector<qc::Qubit>& lines);
        virtual bool onExpression(const VariableExpression& expression, std::vector<qc::Qubit>& lines);

        virtual bool expAdd([[maybe_unused]] unsigned bitwidth, std::vector<qc::Qubit>& lines, const std::vector<qc::Qubit>& lhs, const std::vector<qc::Qubit>& rhs)      = 0;
        virtual bool expSubtract([[maybe_unused]] unsigned bitwidth, std::vector<qc::Qubit>& lines, const std::vector<qc::Qubit>& lhs, const std::vector<qc::Qubit>& rhs) = 0;
        virtual bool expExor([[maybe_unused]] unsigned bitwidth, std::vector<qc::Qubit>& lines, const std::vector<qc::Qubit>& lhs, const std::vector<qc::Qubit>& rhs)     = 0;

        // BEGIN: Add circuit parameter to functions

        // unary operations
        bool bitwiseNegation(const std::vector<qc::Qubit>& dest); // ~
        bool decrement(const std::vector<qc::Qubit>& dest);       // --
        bool increment(const std::vector<qc::Qubit>& dest);       // ++

        // binary operations
        bool         bitwiseAnd(const std::vector<qc::Qubit>& dest, const std::vector<qc::Qubit>& src1, const std::vector<qc::Qubit>& src2); // &
        bool         bitwiseCnot(const std::vector<qc::Qubit>& dest, const std::vector<qc::Qubit>& src);                                     // ^=
        bool         bitwiseOr(const std::vector<qc::Qubit>& dest, const std::vector<qc::Qubit>& src1, const std::vector<qc::Qubit>& src2);  // &
        bool         conjunction(qc::Qubit dest, qc::Qubit src1, qc::Qubit src2);                                                            // &&// -=
        bool         decreaseWithCarry(const std::vector<qc::Qubit>& dest, const std::vector<qc::Qubit>& src, qc::Qubit carry);
        bool         disjunction(qc::Qubit dest, qc::Qubit src1, qc::Qubit src2);                                                          // ||
        bool         division(const std::vector<qc::Qubit>& dest, const std::vector<qc::Qubit>& src1, const std::vector<qc::Qubit>& src2); // /
        bool         equals(qc::Qubit dest, const std::vector<qc::Qubit>& src1, const std::vector<qc::Qubit>& src2);                       // =
        bool         greaterEquals(qc::Qubit dest, const std::vector<qc::Qubit>& srcTwo, const std::vector<qc::Qubit>& srcOne);            // >
        bool         greaterThan(qc::Qubit dest, const std::vector<qc::Qubit>& src2, const std::vector<qc::Qubit>& src1);                  // >// +=
        bool         increaseWithCarry(const std::vector<qc::Qubit>& dest, const std::vector<qc::Qubit>& src, qc::Qubit carry);
        bool         lessEquals(qc::Qubit dest, const std::vector<qc::Qubit>& src2, const std::vector<qc::Qubit>& src1);                         // <=
        bool         lessThan(qc::Qubit dest, const std::vector<qc::Qubit>& src1, const std::vector<qc::Qubit>& src2);                           // <
        bool         modulo(const std::vector<qc::Qubit>& dest, const std::vector<qc::Qubit>& src1, const std::vector<qc::Qubit>& src2);         // %
        bool         multiplication(const std::vector<qc::Qubit>& dest, const std::vector<qc::Qubit>& src1, const std::vector<qc::Qubit>& src2); // *
        bool         notEquals(qc::Qubit dest, const std::vector<qc::Qubit>& src1, const std::vector<qc::Qubit>& src2);                          // !=
        bool         swap(const std::vector<qc::Qubit>& dest1, const std::vector<qc::Qubit>& dest2);                                             // NOLINT(cppcoreguidelines-noexcept-swap, performance-noexcept-swap) <=>
        bool         decrease(const std::vector<qc::Qubit>& rhs, const std::vector<qc::Qubit>& lhs);
        bool         increase(const std::vector<qc::Qubit>& rhs, const std::vector<qc::Qubit>& lhs);
        virtual bool expressionOpInverse([[maybe_unused]] unsigned op, [[maybe_unused]] const std::vector<qc::Qubit>& expLhs, [[maybe_unused]] const std::vector<qc::Qubit>& expRhs);
        bool         checkRepeats();

        // shift operations
        bool leftShift(const std::vector<qc::Qubit>& dest, const std::vector<qc::Qubit>& src1, qc::Qubit src2);  // <<
        bool rightShift(const std::vector<qc::Qubit>& dest, const std::vector<qc::Qubit>& src1, qc::Qubit src2); // >>

        void addVariable(const std::vector<unsigned>& dimensions, const Variable::ptr& var, const std::string& arraystr);
        void getVariables(const VariableAccess::ptr& var, std::vector<qc::Qubit>& lines);

        qc::Qubit getConstantLine(bool value);
        void      getConstantLines(unsigned bitwidth, unsigned value, std::vector<qc::Qubit>& lines);

        static bool synthesize(SyrecSynthesis* synthesizer, const Program& program, const Properties::ptr& settings, const Properties::ptr& statistics);

        [[maybe_unused]] bool addOperationsImplementingNotGate(qc::Qubit targetQubit);
        [[maybe_unused]] bool addOperationsImplementingCnotGate(qc::Qubit controlQubit, qc::Qubit targetQubit);
        [[maybe_unused]] bool addOperationsImplementingToffoliGate(qc::Qubit controlQubitOne, qc::Qubit controlQubitTwo, qc::Qubit targetQubit);
        [[maybe_unused]] bool addOperationsImplementingMultiControlToffoliGate(const std::unordered_set<qc::Qubit>& controlQubitsSet, qc::Qubit targetQubit);
        [[maybe_unused]] bool addOperationsImplementingFredkinGate(qc::Qubit targetQubitOne, qc::Qubit targetQubitTwo);

        [[nodiscard]] static bool isQubitWithinRange(const qc::QuantumComputation& quantumComputation, qc::Qubit qubit) noexcept;
        [[nodiscard]] static bool areQubitsWithinRange(const qc::QuantumComputation& quantumComputation, const std::unordered_set<qc::Qubit>& qubitsToCheck) noexcept;

        // TODO: Rework function comments
        // TODO: Remove custom Circuit and NBitValuesContainer class

        /**
         * Activate a new control qubit propagation scope.
         *
         * @remarks All active control qubits registered in the currently active propagation scopes will be added to any quantum operation, created by any of the addOperationsImplementingXGate functions, in the qc::QuantumComputation.
         * Already existing quantum operations will not be modified.
         */
        void activateControlQubitPropagationScope();

        /**
         * Deactivates the last activated control qubit propagation scope.
         *
         * @remarks
         * All control qubits registered in the last activated control qubit propagation scope are removed from the aggregate of all active control qubits.
         * Control qubits registered for propagation prior to the last activated control qubit propagation scope and deregistered in said scope are registered for propagation again. \n
         * \n
         * Example:
         * Assuming that the aggregate A contains the control qubits (1,2,3), a propagation scope is activated and the control qubits (3,4)
         * registered setting the control qubit aggregate to (1,2,3,4). After the local scope is deactivated, only the control qubit 4 that was registered in the last activate propagation scope,
         * is removed from the aggregate while control qubit 3 will remain in the aggregate due to it also being registered in a parent scope thus the aggregate will be equal to (1,2,3) again.
         */
        void deactivateControlQubitPropagationScope();

        /**
         * Deregister a control qubit from the last activated control qubit propagation scope.
         *
         * @remarks The control qubit is only removed from the aggregate of all registered control qubits if the last activated local scope registered the @p controlQubit.
         * The deregistered control qubit is not 'inherited' by any quantum computation added to the internally used qc::QuantumComputation while the current scope is active. Additionally,
         * the deregistered control qubits are not filtered from the user defined control qubits provided as parameters to any of the addOperationsImplementingXGate calls.
         * @param controlQubit The control qubit to deregister
         * @return Whether the control qubit exists in the internally used qc::QuantumComputation and was deregistered from the last activated propagation scope.
         */
        [[maybe_unused]] bool deregisterControlQubitFromPropagationInCurrentScope(qc::Qubit controlQubit);

        /**
         * Register a control qubit in the last activated control qubit propagation scope.
         *
         * @remarks If no active local control qubit scope exists, a new one is created.
         * @param controlQubit The control qubit to register
         * @return Whether the control qubit exists in the \p quantumComputation and was registered in the last activated propagation scope.
         */
        [[maybe_unused]] bool registerControlQubitForPropagationInCurrentAndNestedScopes(qc::Qubit controlQubit);

        /**
         * Register or update a global quantum operation annotation. Global quantum operation annotations are added to all quantum operations added to the internally used qc::QuantumComputation.
         * Already existing quantum computations in the qc::QuantumComputation are not modified.
         * @param key The key of the global quantum operation annotation
         * @param value The value of the global quantum operation annotation
         * @return Whether an existing annotation was updated.
         */
        [[maybe_unused]] bool setOrUpdateGlobalQuantumOperationAnnotation(const std::string_view& key, const std::string& value);

        /**
         * Remove a global gate annotation. Existing annotations of the gates of the circuit are not modified.
         * @param key The key of the global gate annotation to be removed
         * @return Whether a global gate annotation was removed.
         */
        [[maybe_unused]] bool removeGlobalQuantumOperationAnnotation(const std::string_view& key);

        [[maybe_unused]] bool annotateAllQuantumOperationsAtPositions(std::size_t fromQuantumOperationIndex, std::size_t toQuantumOperationIndex, const GateAnnotationsLookup& userProvidedAnnotationsPerQuantumOperation);

        std::stack<Statement::ptr>    stmts;
        qc::QuantumComputation&       quantumComputation; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
        Number::loop_variable_mapping loopMap;
        std::stack<Module::ptr>       modules;

        std::unordered_set<qc::Qubit>                    aggregateOfPropagatedControlQubits;
        std::vector<std::unordered_map<qc::Qubit, bool>> controlQubitPropgationScopes;

        // To be able to use a std::string_view key lookup (heterogeneous lookup) in a std::map/std::unordered_set
        // we need to define the transparent comparator (std::less<>). This feature is only available starting with C++17
        // For further information, see: https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2012/n3465.pdf
        GateAnnotationsLookup activateGlobalQuantumOperationAnnotations;
        // We are assuming that no operations in the qc::QuantumComputation are removed (i.e. by applying qc::CircuitOptimizer) and will thus use the index of the quantum operation
        // as the search key in the container storing the annotations per quantum operation.
        std::vector<GateAnnotationsLookup> annotationsPerQuantumOperation;
        std::unordered_set<qc::Qubit>      addedAncillaryQubitIndices;

    private:
        VarLinesMap                            varLines;
        std::map<bool, std::vector<qc::Qubit>> freeConstLinesMap;
    };

} // namespace syrec
