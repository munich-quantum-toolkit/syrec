/*
 * Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
 * Copyright (c) 2025 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

parser grammar TSyrecParser;

options {
	tokenVocab = TSyrecLexer;
}

/* Number production */
number:
	INT								# NumberFromConstant
	| SIGNAL_WIDTH_PREFIX IDENT		# NumberFromSignalwidth
	| LOOP_VARIABLE_PREFIX IDENT	# NumberFromLoopVariable
	| ( OPEN_RBRACKET lhsOperand=number op=( OP_PLUS | OP_MINUS | OP_MULTIPLY | OP_DIVISION ) rhsOperand=number CLOSE_RBRACKET ) # NumberFromExpression
	;

/* Program and modules productions */

program: module+ EOF;

module :
	'module'
	IDENT
	OPEN_RBRACKET parameterList? CLOSE_RBRACKET
	signalList*
	statementList
	;

parameterList:
	parameter
	( PARAMETER_DELIMITER parameter )*
	;

parameter:
	( VAR_TYPE_IN
		| VAR_TYPE_OUT
		| VAR_TYPE_INOUT
	)
	signalDeclaration
	;

signalList:
	( VAR_TYPE_WIRE
		| VAR_TYPE_STATE
	)
	signalDeclaration
	( PARAMETER_DELIMITER signalDeclaration )*
	;

signalDeclaration:
IDENT
( OPEN_SBRACKET dimensionTokens+=INT CLOSE_SBRACKET )*
( OPEN_RBRACKET signalWidthToken=INT CLOSE_RBRACKET )?
;

/* Statements productions */

statementList: stmts+=statement ( STATEMENT_DELIMITER stmts+=statement )* ;

statement:
	callStatement
	| forStatement
	| ifStatement
	| unaryStatement
	| assignStatement
	| swapStatement
	| skipStatement
	;

callStatement: ( OP_CALL | OP_UNCALL ) moduleIdent=IDENT OPEN_RBRACKET callerArguments+=IDENT ( PARAMETER_DELIMITER callerArguments+=IDENT )* CLOSE_RBRACKET ;

loopVariableDefinition: LOOP_VARIABLE_PREFIX variableIdent=IDENT OP_EQUAL ;
loopStepsizeDefinition: KEYWORD_STEP OP_MINUS? number ;
forStatement: KEYWORD_FOR ( loopVariableDefinition? startValue=number KEYWORD_TO )? endValue=number loopStepsizeDefinition? KEYWORD_DO statementList KEYWORD_ROF ;

ifStatement:
	KEYWORD_IF guardCondition=expression KEYWORD_THEN
		trueBranchStmts=statementList
	KEYWORD_ELSE
		falseBranchStmts=statementList
	KEYWORD_FI matchingGuardExpression=expression ;

unaryStatement:  unaryOp=( OP_INVERT_ASSIGN | OP_INCREMENT_ASSIGN | OP_DECREMENT_ASSIGN ) signal ;

assignStatement: signal assignmentOp=( OP_ADD_ASSIGN | OP_SUB_ASSIGN | OP_XOR_ASSIGN ) expression ;

swapStatement:
	lhsOperand=signal OP_SWAP rhsOperand=signal ;

skipStatement: KEYWORD_SKIP ;

signal: IDENT ( OPEN_SBRACKET accessedDimensions+=expression CLOSE_SBRACKET )* ( BITRANGE_START_PREFIX bitStart=number ( BITRANGE_END_PREFIX bitRangeEnd=number )? )? ;

/* Expression productions */

expression:
	number						# ExpressionFromNumber
	| signal					# ExpressionFromSignal
	| binaryExpression			# ExpressionFromBinaryExpression
	| unaryExpression			# ExpressionFromUnaryExpression
	| shiftExpression			# ExpressionFromShiftExpression
	;

binaryExpression:
	OPEN_RBRACKET lhsOperand=expression
		binaryOperation=( OP_PLUS
		| OP_MINUS
		| OP_MULTIPLY
		| OP_DIVISION
		| OP_MODULO
		| OP_UPPER_BIT_MULTIPLY
		| OP_LOGICAL_AND
		| OP_LOGICAL_OR
		| OP_BITWISE_AND
		| OP_BITWISE_OR
		| OP_BITWISE_XOR
		| OP_LESS_THAN
		| OP_GREATER_THAN
		| OP_EQUAL
		| OP_NOT_EQUAL
		| OP_LESS_OR_EQUAL
		| OP_GREATER_OR_EQUAL
		)
	rhsOperand=expression  CLOSE_RBRACKET
	;

unaryExpression: unaryOperation=( OP_LOGICAL_NEGATION | OP_BITWISE_NEGATION ) expression ;

shiftExpression: OPEN_RBRACKET expression shiftOperation=( OP_RIGHT_SHIFT | OP_LEFT_SHIFT ) number CLOSE_RBRACKET ;
