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
#include "ir/operations/Control.hpp"

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

        void addVariables(qc::QuantumComputation& quantumComputation, const Variable::vec& variables);
        void setMainModule(const Module::ptr& mainModule);

    protected:
        constexpr static std::string_view GATE_ANNOTATION_KEY_ASSOCIATED_STATEMENT_LINE_NUMBER = "lno";
        using GateAnnotationsLookup = std::map<std::string, std::string, std::less<>>;

        virtual bool processStatement(qc::QuantumComputation& quantumComputation, const Statement::ptr& statement) = 0;

        virtual bool onModule(qc::QuantumComputation& quantumComputation, const Module::ptr&);

        virtual bool opRhsLhsExpression([[maybe_unused]] const Expression::ptr& expression, [[maybe_unused]] std::vector<qc::Qubit>& v);
        virtual bool opRhsLhsExpression([[maybe_unused]] const VariableExpression& expression, [[maybe_unused]] std::vector<qc::Qubit>& v);
        virtual bool opRhsLhsExpression([[maybe_unused]] const BinaryExpression& expression, [[maybe_unused]] std::vector<qc::Qubit>& v);

        virtual bool              onStatement(qc::QuantumComputation& quantumComputation, const Statement::ptr& statement);
        virtual bool              onStatement(qc::QuantumComputation& quantumComputation, const AssignStatement& statement);
        virtual bool              onStatement(qc::QuantumComputation& quantumComputation, const IfStatement& statement);
        virtual bool              onStatement(qc::QuantumComputation& quantumComputation, const ForStatement& statement);
        virtual bool              onStatement(qc::QuantumComputation& quantumComputation, const CallStatement& statement);
        virtual bool              onStatement(qc::QuantumComputation& quantumComputation, const UncallStatement& statement);
        bool                      onStatement(qc::QuantumComputation& quantumComputation, const SwapStatement& statement);
        bool                      onStatement(qc::QuantumComputation& quantumComputation, const UnaryStatement& statement);
        [[nodiscard]] static bool onStatement(const SkipStatement& statement);

        virtual bool assignAdd(qc::QuantumComputation& quantumComputation, std::vector<qc::Qubit>& lhs, std::vector<qc::Qubit>& rhs, [[maybe_unused]] unsigned op)      = 0;
        virtual bool assignSubtract(qc::QuantumComputation& quantumComputation, std::vector<qc::Qubit>& lhs, std::vector<qc::Qubit>& rhs, [[maybe_unused]] unsigned op) = 0;
        virtual bool assignExor(qc::QuantumComputation& quantumComputation, std::vector<qc::Qubit>& lhs, std::vector<qc::Qubit>& rhs, [[maybe_unused]] unsigned op)     = 0;

        virtual bool onExpression(qc::QuantumComputation& quantumComputation, const Expression::ptr& expression, std::vector<qc::Qubit>& lines, std::vector<qc::Qubit> const& lhsStat, unsigned op);
        virtual bool onExpression(qc::QuantumComputation& quantumComputation, const BinaryExpression& expression, std::vector<qc::Qubit>& lines, std::vector<qc::Qubit> const& lhsStat, unsigned op);
        virtual bool onExpression(qc::QuantumComputation& quantumComputation, const ShiftExpression& expression, std::vector<qc::Qubit>& lines, std::vector<qc::Qubit> const& lhsStat, unsigned op);
        virtual bool onExpression(qc::QuantumComputation& quantumComputation, const NumericExpression& expression, std::vector<qc::Qubit>& lines);
        virtual bool onExpression(const VariableExpression& expression, std::vector<qc::Qubit>& lines);

        virtual bool expAdd(qc::QuantumComputation& quantumComputation, [[maybe_unused]] unsigned bitwidth, std::vector<qc::Qubit>& lines, const std::vector<qc::Qubit>& lhs, const std::vector<qc::Qubit>& rhs)      = 0;
        virtual bool expSubtract(qc::QuantumComputation& quantumComputation, [[maybe_unused]] unsigned bitwidth, std::vector<qc::Qubit>& lines, const std::vector<qc::Qubit>& lhs, const std::vector<qc::Qubit>& rhs) = 0;
        virtual bool expExor(qc::QuantumComputation& quantumComputation, [[maybe_unused]] unsigned bitwidth, std::vector<qc::Qubit>& lines, const std::vector<qc::Qubit>& lhs, const std::vector<qc::Qubit>& rhs)     = 0;

        // BEGIN: Add circuit parameter to functions

        // unary operations
        bool bitwiseNegation(qc::QuantumComputation& quantumComputation, const std::vector<qc::Qubit>& dest); // ~
        bool decrement(qc::QuantumComputation& quantumComputation, const std::vector<qc::Qubit>& dest);             // --
        bool increment(qc::QuantumComputation& quantumComputation, const std::vector<qc::Qubit>& dest);             // ++

        // binary operations
        bool         bitwiseAnd(qc::QuantumComputation& quantumComputation, const std::vector<qc::Qubit>& dest, const std::vector<qc::Qubit>& src1, const std::vector<qc::Qubit>& src2); // &
        bool         bitwiseCnot(qc::QuantumComputation& quantumComputation, const std::vector<qc::Qubit>& dest, const std::vector<qc::Qubit>& src);                                     // ^=
        bool         bitwiseOr(qc::QuantumComputation& quantumComputation, const std::vector<qc::Qubit>& dest, const std::vector<qc::Qubit>& src1, const std::vector<qc::Qubit>& src2);  // &
        bool         conjunction(qc::QuantumComputation& quantumComputation, qc::Qubit dest, qc::Qubit src1, qc::Qubit src2);                                                            // &&// -=
        bool         decreaseWithCarry(qc::QuantumComputation& quantumComputation, const std::vector<qc::Qubit>& dest, const std::vector<qc::Qubit>& src, qc::Qubit carry);
        bool         disjunction(qc::QuantumComputation& quantumComputation, qc::Qubit dest, qc::Qubit src1, qc::Qubit src2);                                                    // ||
        bool         division(qc::QuantumComputation& quantumComputation, const std::vector<qc::Qubit>& dest, const std::vector<qc::Qubit>& src1, const std::vector<qc::Qubit>& src2); // /
        bool         equals(qc::QuantumComputation& quantumComputation, qc::Qubit dest, const std::vector<qc::Qubit>& src1, const std::vector<qc::Qubit>& src2);                 // =
        bool         greaterEquals(qc::QuantumComputation& quantumComputation, qc::Qubit dest, const std::vector<qc::Qubit>& srcTwo, const std::vector<qc::Qubit>& srcOne);      // >
        bool         greaterThan(qc::QuantumComputation& quantumComputation, qc::Qubit dest, const std::vector<qc::Qubit>& src2, const std::vector<qc::Qubit>& src1);            // >// +=
        bool         increaseWithCarry(qc::QuantumComputation& quantumComputation, const std::vector<qc::Qubit>& dest, const std::vector<qc::Qubit>& src, qc::Qubit carry);
        bool         lessEquals(qc::QuantumComputation& quantumComputation, qc::Qubit dest, const std::vector<qc::Qubit>& src2, const std::vector<qc::Qubit>& src1);                   // <=
        bool         lessThan(qc::QuantumComputation& quantumComputation, qc::Qubit dest, const std::vector<qc::Qubit>& src1, const std::vector<qc::Qubit>& src2);                     // <
        bool         modulo(qc::QuantumComputation& quantumComputation, const std::vector<qc::Qubit>& dest, const std::vector<qc::Qubit>& src1, const std::vector<qc::Qubit>& src2);         // %
        bool         multiplication(qc::QuantumComputation& quantumComputation, const std::vector<qc::Qubit>& dest, const std::vector<qc::Qubit>& src1, const std::vector<qc::Qubit>& src2); // *
        bool         notEquals(qc::QuantumComputation& quantumComputation, qc::Qubit dest, const std::vector<qc::Qubit>& src1, const std::vector<qc::Qubit>& src2);                    // !=
        bool         swap(qc::QuantumComputation& quantumComputation, const std::vector<qc::Qubit>& dest1, const std::vector<qc::Qubit>& dest2);                                       // NOLINT(cppcoreguidelines-noexcept-swap, performance-noexcept-swap) <=>
        bool         decrease(qc::QuantumComputation& quantumComputation, const std::vector<qc::Qubit>& rhs, const std::vector<qc::Qubit>& lhs);
        bool         increase(qc::QuantumComputation& quantumComputation, const std::vector<qc::Qubit>& rhs, const std::vector<qc::Qubit>& lhs);
        virtual bool expressionOpInverse([[maybe_unused]] qc::QuantumComputation& quantumComputation, [[maybe_unused]] unsigned op, [[maybe_unused]] const std::vector<qc::Qubit>& expLhs, [[maybe_unused]] const std::vector<qc::Qubit>& expRhs);
        bool         checkRepeats();

        // shift operations
        bool leftShift(qc::QuantumComputation& quantumComputation, const std::vector<qc::Qubit>& dest, const std::vector<qc::Qubit>& src1, qc::Qubit src2);  // <<
        bool rightShift(qc::QuantumComputation& quantumComputation, const std::vector<qc::Qubit>& dest, const std::vector<qc::Qubit>& src1, qc::Qubit src2); // >>

        static void addVariable(qc::QuantumComputation& circ, const std::vector<qc::Qubit>& dimensions, const Variable::ptr& var, bool areVariableLinesConstants, bool areLinesOfVariableGarbageLines, const std::string& arraystr);
        void        getVariables(const VariableAccess::ptr& var, std::vector<qc::Qubit>& lines);

        qc::Qubit getConstantLine(qc::QuantumComputation& quantumComputation, bool value);
        void      getConstantLines(qc::QuantumComputation& quantumComputation, unsigned bitwidth, unsigned value, std::vector<qc::Qubit>& lines);

        static bool synthesize(SyrecSynthesis* synthesizer, qc::QuantumComputation& quantumComputation, const Program& program, const Properties::ptr& settings, const Properties::ptr& statistics);

        [[maybe_unused]] bool createAndAddNotGate(qc::QuantumComputation& quantumComputation, qc::Qubit targetQubit);
        [[maybe_unused]] bool createAndAddCnotGate(qc::QuantumComputation& quantumComputation, qc::Qubit controlQubit, qc::Qubit targetQubit);
        [[maybe_unused]] bool createAndAddToffoliGate(qc::QuantumComputation& quantumComputation, qc::Qubit controlQubitOne, qc::Qubit controlQubitTwo, qc::Qubit targetQubit);
        [[maybe_unused]] bool createAndAddMultiControlToffoliGate(qc::QuantumComputation& quantumComputation, const std::unordered_set<qc::Qubit>& controlQubitsSet, qc::Qubit targetQubit);
        [[maybe_unused]] bool createAndAddFredkinGate(qc::QuantumComputation& quantumComputation, qc::Qubit targetQubitOne, qc::Qubit targetQubitTwo);

        [[nodiscard]] static bool isQubitWithinRange(const qc::QuantumComputation& quantumComputation, qc::Qubit qubit) noexcept;
        [[nodiscard]] static bool areQubitsWithinRange(const qc::QuantumComputation& quantumComputation, const std::unordered_set<qc::Qubit>& qubitsToCheck) noexcept;

        // TODO: Rework function comments
        // TODO: Remove custom Circuit and NBitValuesContainer class

        /**
         * Activate a new control line propagation scope.
         *
         * @remarks All active control lines registered in the currently active propagation scopes
         * will be added to any future gate added to the circuit. Already existing gates are not modified.
         */
        void activateControlLinePropagationScope();

        /**
         * Deactivates the last activated control line propagation scope.
         *
         * @remarks
         * All control lines registered in the last activated control line propagation scope are removed from the aggregate of all active control lines.
         * Control lines active prior to the last activated control line propagation scope and deregistered in said scope are activated again. \n
         * \n
         * Example:
         * Assuming that the aggregate A contains the control lines (1,2,3), a propagation scope is activated and the control lines (3,4)
         * registered which will set the aggregate to (1,2,3,4). After the local scope is deactivated, only the control line 4 that is
         * local to the scope is removed from the aggregate while control line 3 will remain in the aggregate thus the aggregate is equal to (1,2,3) again.
         */
        void deactivateControlLinePropagationScope();

        /**
         * Deregister a control line from the last activated control line propagation scope.
         *
         * @remarks The control line is only removed from the aggregate if the last activated local scope registered the @p controlLine.
         * The deregistered control line is not 'inherited' by any gate added while the current scope is active. Additionally, no deregistered control line
         * is filtered from the user provided control lines when adding a new gate.
         * @param controlQubit The control line to deregister
         * @return Whether the control line exists in the circuit and whether it was deregistered from the last activated propagation scope.
         */
        [[maybe_unused]] bool deregisterControlLineFromPropagationInCurrentScope(const qc::QuantumComputation& quantumComputation, qc::Qubit controlQubit);

        /**
         * Register a control line in the last activated control line propagation scope.
         *
         * @remarks If no active local control line scope exists, a new one is created.
         * @param controlQubit The control line to register
         * @return Whether the control line exists in the circuit and whether it was registered in the last activated propagation scope.
         */
        [[maybe_unused]] bool registerControlLineForPropagationInCurrentAndNestedScopes(const qc::QuantumComputation& quantumComputation, qc::Qubit controlQubit);

        /**
         * Register or update a global gate annotation. Global gate annotations are added to all future gates added to the circuit. Existing gates are not modified.
         * @param key The key of the global gate annotation
         * @param value The value of the global gate annotation
         * @return Whether an existing annotation was updated.
         */
        [[maybe_unused]] bool setOrUpdateGlobalGateAnnotation(const std::string_view& key, const std::string& value);

        /**
         * Remove a global gate annotation. Existing annotations of the gates of the circuit are not modified.
         * @param key The key of the global gate annotation to be removed
         * @return Whether a global gate annotation was removed.
         */
        [[maybe_unused]] bool removeGlobalGateAnnotation(const std::string_view& key);

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
        // We are assuming that no operations in the qc::QuantumComputation are removed and will thus use the index of the quantum operation
        // as the search key in the container storing the annotations per quantum operation.
        std::vector<GateAnnotationsLookup> annotationsPerQuantumOperation;

    private:
        VarLinesMap                            varLines;
        std::map<bool, std::vector<qc::Qubit>> freeConstLinesMap;
    };

} // namespace syrec
