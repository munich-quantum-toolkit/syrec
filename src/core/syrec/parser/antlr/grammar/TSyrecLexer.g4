/*
 * Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
 * Copyright (c) 2025 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

lexer grammar TSyrecLexer;

/* Token definitions */
OP_INCREMENT_ASSIGN: '++=' ;
OP_DECREMENT_ASSIGN: '--=' ;
OP_INVERT_ASSIGN: '~=' ;

OP_ADD_ASSIGN: '+=' ;
OP_SUB_ASSIGN: '-=' ;
OP_XOR_ASSIGN: '^=' ;

OP_PLUS : '+' ;
OP_MINUS : '-' ;
OP_MULTIPLY: '*' ;
OP_UPPER_BIT_MULTIPLY: '*>' ;
OP_DIVISION: '/' ;
OP_MODULO: '%' ;

OP_LEFT_SHIFT: '<<' ;
OP_RIGHT_SHIFT: '>>' ;

OP_SWAP: '<=>' ;

OP_GREATER_OR_EQUAL: '>=' ;
OP_LESS_OR_EQUAL: '<=' ;
OP_GREATER_THAN: '>' ;
OP_LESS_THAN: '<' ;
OP_EQUAL: '=';
OP_NOT_EQUAL: '!=';

OP_LOGICAL_AND: '&&' ;
OP_LOGICAL_OR: '||' ;
OP_LOGICAL_NEGATION: '!' ;

OP_BITWISE_AND: '&' ;
OP_BITWISE_NEGATION: '~' ;
OP_BITWISE_OR: '|' ;
OP_BITWISE_XOR: '^' ;

OP_CALL: 'call' ;
OP_UNCALL: 'uncall' ;

VAR_TYPE_IN: 'in' ;
VAR_TYPE_OUT: 'out' ;
VAR_TYPE_INOUT: 'inout' ;
VAR_TYPE_WIRE: 'wire' ;
VAR_TYPE_STATE: 'state' ;

LOOP_VARIABLE_PREFIX: '$' ;
SIGNAL_WIDTH_PREFIX: '#' ;

STATEMENT_DELIMITER: ';' ;
PARAMETER_DELIMITER: ',' ;

OPEN_RBRACKET: '(' ;
CLOSE_RBRACKET: ')' ;
OPEN_SBRACKET: '[' ;
CLOSE_SBRACKET: ']' ;

KEYWORD_MODULE: 'module' ;
KEYWORD_FOR: 'for' ;
KEYWORD_DO: 'do' ;
KEYWORD_TO: 'to' ;
KEYWORD_STEP: 'step' ;
KEYWORD_ROF: 'rof' ;

KEYWORD_IF: 'if' ;
KEYWORD_THEN: 'then' ;
KEYWORD_ELSE: 'else' ;
KEYWORD_FI: 'fi' ;

KEYWORD_SKIP: 'skip' ;

BITRANGE_START_PREFIX: '.' ;
BITRANGE_END_PREFIX: ':' ;

/* LINE_COMMENT: '#' ~[\r\n]* -> skip ; */
/* unicode whitespace \u000B */
SKIPABLEWHITSPACES : [ \t\r\n]+ -> channel(HIDDEN) ;	// Skip newline, tabulator and carriage return symbols
LINE_COMMENT: '//' .*? ('\n'|EOF) -> channel(HIDDEN) ;
MULTI_LINE_COMMENT: '/*' .*? '*/' -> channel(HIDDEN) ;

fragment LETTER : 'a'..'z' | 'A'..'Z' ;
fragment DIGIT : '0'..'9' ;
IDENT : ( '_' | LETTER ) ( '_' | LETTER | DIGIT )* ;
INT : DIGIT+ ;
