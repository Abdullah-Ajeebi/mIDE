#pragma once

#include <string>

enum class OptimizerOperator
{
	Add,
	Subtract,
	Multiply,
	Divide
};

// Returns a folded value or an empty string when the expression is not foldable.
std::wstring TryFoldBinary(OptimizerOperator op, const std::wstring& left, const std::wstring& right);

bool IsOptimizerInteger(const std::wstring& value);
