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

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>

template<class... Ts>
struct Overloaded: Ts... {
    using Ts::operator()...;
};
template<class... Ts>
Overloaded(Ts...) -> Overloaded<Ts...>;

namespace syrec {

    class Number {
    public:
        using ptr = std::shared_ptr<Number>;

        using LoopVariableMapping = std::map<std::string, unsigned int>;
        struct ConstantExpression {
            using ptr = std::shared_ptr<ConstantExpression>;

            enum class Operation : std::uint8_t {
                Addition,
                Subtraction,
                Multiplication,
                Division
            };
            Number::ptr lhsOperand;
            Operation   operation;
            Number::ptr rhsOperand;

            explicit ConstantExpression(Number::ptr lhsOperand, const Operation operation, Number::ptr rhsOperand):
                lhsOperand(std::move(lhsOperand)), operation(operation), rhsOperand(std::move(rhsOperand)) {}

            [[nodiscard]] std::optional<unsigned int> tryEvaluate(const LoopVariableMapping& loopVariableValueLookup) const {
                const std::optional<unsigned int> lhsOperandEvaluated = lhsOperand ? lhsOperand->tryEvaluate(loopVariableValueLookup) : std::nullopt;
                const std::optional<unsigned int> rhsOperandEvaluated = rhsOperand ? rhsOperand->tryEvaluate(loopVariableValueLookup) : std::nullopt;
                if ((operation == Operation::Division && rhsOperandEvaluated.has_value() && rhsOperandEvaluated.value() == 0) || (!lhsOperandEvaluated.has_value() || !rhsOperandEvaluated.has_value())) {
                    return std::nullopt;
                }

                switch (operation) {
                    case Operation::Addition:
                        return *lhsOperandEvaluated + *rhsOperandEvaluated;
                    case Operation::Subtraction:
                        return *lhsOperandEvaluated - *rhsOperandEvaluated;
                    case Operation::Multiplication:
                        return *lhsOperandEvaluated * *rhsOperandEvaluated;
                    case Operation::Division:
                        return *lhsOperandEvaluated / *rhsOperandEvaluated;
                }
                return std::nullopt;
            }
        };

        explicit Number(std::variant<unsigned, std::string, ConstantExpression> number):
            numberVar(std::move(number)) {}

        explicit Number(unsigned value):
            numberVar(value) {
        }

        explicit Number(const std::string& value):
            numberVar(value) {
        }

        explicit Number(ConstantExpression value):
            numberVar(std::move(value)) {
        }

        ~Number() = default;

        [[nodiscard]] bool isLoopVariable() const {
            return std::holds_alternative<std::string>(numberVar);
        }

        [[nodiscard]] bool isConstant() const {
            return std::holds_alternative<unsigned>(numberVar);
        }

        [[nodiscard]] bool isConstantExpression() const {
            return std::holds_alternative<ConstantExpression>(numberVar);
        }

        [[nodiscard]] const std::string& variableName() const {
            return std::get<std::string>(numberVar);
        }

        [[nodiscard]] std::optional<ConstantExpression> constantExpression() const {
            return std::get<ConstantExpression>(numberVar);
        }

        [[nodiscard]] unsigned evaluate(const LoopVariableMapping& map) const {
            return std::visit(Overloaded{
                                      [](unsigned arg) { return arg; },
                                      [&map](const std::string& value) { return map.find(value)->second; },
                                      [&map](const ConstantExpression& constantExpression) { return constantExpression.tryEvaluate(map).value(); }},
                              numberVar);
        }

        [[nodiscard]] std::optional<unsigned> tryEvaluate(const LoopVariableMapping& loopVariableValueLookup) const {
            if (isConstant()) {
                return std::get<unsigned>(numberVar);
            }
            if (isLoopVariable() && loopVariableValueLookup.count(std::get<std::string>(numberVar)) > 0) {
                return loopVariableValueLookup.at(std::get<std::string>(numberVar));
            }
            if (isConstantExpression()) {
                return std::get<ConstantExpression>(numberVar).tryEvaluate(loopVariableValueLookup);
            }
            return std::nullopt;
        }

    private:
        std::variant<unsigned, std::string, ConstantExpression> numberVar;
    };

} // namespace syrec
