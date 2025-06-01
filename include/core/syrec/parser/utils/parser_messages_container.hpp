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

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace syrec_parser {
    struct Message {
        using ptr = std::shared_ptr<Message>;
        struct Position {
            std::size_t line;
            std::size_t column;

            explicit Position(std::size_t line, std::size_t column):
                line(line), column(column) {}
        };

        enum class Type : std::uint8_t {
            Error,
            Information,
            Warning
        };

        Type        type;
        std::string id;
        Position    position;
        std::string message;

        Message(Type type, std::string id, const Position position, std::string message):
            type(type), id(std::move(id)), position(position), message(std::move(message)) {}

        [[nodiscard]] std::string stringify() const {
            return "-- line " + std::to_string(position.line) + " col " + std::to_string(position.column) + ": " + message;
        }
    };

    class ParserMessagesContainer {
    public:
        ParserMessagesContainer() {
            messagesPerType.emplace(Message::Type::Error, std::vector<Message::ptr>());
            messagesPerType.emplace(Message::Type::Information, std::vector<Message::ptr>());
            messagesPerType.emplace(Message::Type::Warning, std::vector<Message::ptr>());
        }

        void                                    recordMessage(std::unique_ptr<Message> message);
        [[nodiscard]] std::vector<Message::ptr> getMessagesOfType(Message::Type messageType) const;
        [[maybe_unused]] bool                   setFilterForToBeRecordedMessages(const std::string& messageIdToPassFilter);
        void                                    clearFilterForToBeRecordedMessages();
        void                                    sortRecordedMessagesOfTypeInAscendingOrder(Message::Type messageType);

    protected:
        std::unordered_map<Message::Type, std::vector<Message::ptr>> messagesPerType;
        std::optional<std::string>                                   temporaryFilterForToBeRecordedMessages;
    };
} // namespace syrec_parser
