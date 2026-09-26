#include "Optimizer.h"

#include <cwctype>
#include <limits>
#include <stdexcept>

namespace
{
	bool IsInteger(const std::wstring& value)
	{
		if (value.empty())
			return false;

		size_t start = value[0] == L'-' ? 1 : 0;
		if (start == value.size())
			return false;

		for (; start < value.size(); ++start)
		{
			if (!iswdigit(value[start]))
				return false;
		}
		return true;
	}
}

bool IsOptimizerInteger(const std::wstring& value)
{
    return IsInteger(value);
}

std::wstring TryFoldBinary(OptimizerOperator op, const std::wstring& left, const std::wstring& right)
{
	if (IsInteger(left) && IsInteger(right))
	{
		long long lhs = 0;
		long long rhs = 0;
		try
		{
			lhs = std::stoll(left);
			rhs = std::stoll(right);
		}
		catch (const std::exception&)
		{
			return {};
		}

		long long value = 0;
		switch (op)
		{
		case OptimizerOperator::Add: value = lhs + rhs; break;
		case OptimizerOperator::Subtract: value = lhs - rhs; break;
		case OptimizerOperator::Multiply: value = lhs * rhs; break;
		case OptimizerOperator::Divide:
			if (rhs == 0)
				return {};
			value = lhs / rhs;
			break;
		}

		if (value < std::numeric_limits<int>::min() || value > std::numeric_limits<int>::max())
			return {};
		return std::to_wstring(value);
	}

	if (op == OptimizerOperator::Add && right == L"0") return left;
	if (op == OptimizerOperator::Subtract && right == L"0") return left;
	if (op == OptimizerOperator::Multiply && right == L"1") return left;
	if (op == OptimizerOperator::Divide && right == L"1") return left;
	if (op == OptimizerOperator::Multiply && left == L"1") return right;
	return {};
}
