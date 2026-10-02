#pragma once

#include <string>
#include <vector>

enum class OptimizerOperator
{
	Add,
	Subtract,
	Multiply,
	Divide,
	Less,
	LessEqual,
	Greater,
	GreaterEqual,
	Equal,
	NotEqual
};

// Returns a folded value or an empty string when the expression is not foldable.
std::wstring TryFoldBinary(OptimizerOperator op, const std::wstring& left, const std::wstring& right);

bool IsOptimizerInteger(const std::wstring& value);

// Optimizes emitted mlog without changing control-flow boundaries.
std::vector<std::wstring> OptimizeMlog(const std::vector<std::wstring>& lines);

// Removes assignments to user variables that have no reads in the program.
std::vector<std::wstring> EliminateDeadStores(const std::vector<std::wstring>& lines);
