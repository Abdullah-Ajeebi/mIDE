#include "Optimizer.h"

#include <cwctype>
#include <limits>
#include <map>
#include <set>
#include <sstream>
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
		case OptimizerOperator::Add:
			if ((rhs > 0 && lhs > std::numeric_limits<long long>::max() - rhs) ||
				(rhs < 0 && lhs < std::numeric_limits<long long>::min() - rhs))
				return {};
			value = lhs + rhs;
			break;
		case OptimizerOperator::Subtract:
			if ((rhs < 0 && lhs > std::numeric_limits<long long>::max() + rhs) ||
				(rhs > 0 && lhs < std::numeric_limits<long long>::min() + rhs))
				return {};
			value = lhs - rhs;
			break;
		case OptimizerOperator::Multiply:
			if (lhs != 0 && (rhs > 0
				? (lhs > std::numeric_limits<long long>::max() / rhs ||
					lhs < std::numeric_limits<long long>::min() / rhs)
				: (lhs == std::numeric_limits<long long>::min()
					? rhs < 1
					: lhs < std::numeric_limits<long long>::max() / rhs ||
						lhs > std::numeric_limits<long long>::min() / rhs)))
				return {};
			value = lhs * rhs;
			break;
		case OptimizerOperator::Divide:
			if (rhs == 0)
				return {};
			if (lhs == std::numeric_limits<long long>::min() && rhs == -1)
				return {};
			value = lhs / rhs;
			break;
		case OptimizerOperator::Less:         value = lhs < rhs; break;
		case OptimizerOperator::LessEqual:    value = lhs <= rhs; break;
		case OptimizerOperator::Greater:      value = lhs > rhs; break;
		case OptimizerOperator::GreaterEqual: value = lhs >= rhs; break;
		case OptimizerOperator::Equal:        value = lhs == rhs; break;
		case OptimizerOperator::NotEqual:     value = lhs != rhs; break;
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

namespace
{
	bool TryGetOperator(const std::wstring& name, OptimizerOperator& op)
	{
		if (name == L"add") op = OptimizerOperator::Add;
		else if (name == L"sub") op = OptimizerOperator::Subtract;
		else if (name == L"mul") op = OptimizerOperator::Multiply;
		else if (name == L"idiv") op = OptimizerOperator::Divide;
		else if (name == L"lessThan") op = OptimizerOperator::Less;
		else if (name == L"lessThanEq") op = OptimizerOperator::LessEqual;
		else if (name == L"greaterThan") op = OptimizerOperator::Greater;
		else if (name == L"greaterThanEq") op = OptimizerOperator::GreaterEqual;
		else if (name == L"equal") op = OptimizerOperator::Equal;
		else if (name == L"notEqual") op = OptimizerOperator::NotEqual;
		else return false;
		return true;
	}
}

std::vector<std::wstring> OptimizeMlog(const std::vector<std::wstring>& lines)
{
	std::vector<std::wstring> optimized;
	std::map<std::wstring, std::wstring> constants;

	for (const std::wstring& line : lines)
	{
		if (line.rfind(L"__label ", 0) == 0 || line.rfind(L"jump ", 0) == 0 ||
			line.rfind(L"set @counter ", 0) == 0)
			constants.clear();

		std::wstringstream parser(line);
		std::wstring instruction;
		parser >> instruction;

		if (instruction == L"set")
		{
			std::wstring destination;
			std::wstring value;
			parser >> destination >> value;
			if (!destination.empty() && !value.empty() && destination != L"@counter")
			{
				const std::wstring originalValue = value;
				auto constant = constants.find(value);
				if (constant != constants.end())
					value = constant->second;

				if (IsOptimizerInteger(value))
					constants[destination] = value;
				else
					constants.erase(destination);

				if (value != originalValue)
					optimized.push_back(L"set " + destination + L" " + value);
				else
					optimized.push_back(line);
				continue;
			}
			constants.erase(destination);
			optimized.push_back(line);
			continue;
		}

		if (instruction == L"op")
		{
			std::wstring operation;
			std::wstring destination;
			std::wstring left;
			std::wstring right;
			parser >> operation >> destination >> left >> right;
			OptimizerOperator op;
			if (!destination.empty() && TryGetOperator(operation, op))
			{
				auto leftConstant = constants.find(left);
				auto rightConstant = constants.find(right);
				if (leftConstant != constants.end()) left = leftConstant->second;
				if (rightConstant != constants.end()) right = rightConstant->second;

				std::wstring folded = TryFoldBinary(op, left, right);
				if (!folded.empty())
				{
					optimized.push_back(L"set " + destination + L" " + folded);
					constants[destination] = folded;
					continue;
				}
				constants.erase(destination);
			}
			optimized.push_back(line);
			continue;
		}

		if (instruction == L"print" || instruction == L"write")
		{
			std::wstring value;
			parser >> value;
			auto constant = constants.find(value);
			if (constant != constants.end())
			{
				const size_t valueStart = line.find(value);
				optimized.push_back(line.substr(0, valueStart) + constant->second +
					line.substr(valueStart + value.size()));
				continue;
			}
		}

		optimized.push_back(line);
	}

	return optimized;
}

namespace
{
	bool IsUserVariable(const std::wstring& value)
	{
		return !value.empty() && value[0] != L'_' && value[0] != L'@' &&
			!IsOptimizerInteger(value);
	}

	void AddRead(std::set<std::wstring>& reads, const std::wstring& value)
	{
		if (IsUserVariable(value))
			reads.insert(value);
	}
}

std::vector<std::wstring> EliminateDeadStores(const std::vector<std::wstring>& lines)
{
	std::set<std::wstring> reads;

	for (const std::wstring& line : lines)
	{
		std::wstringstream parser(line);
		std::wstring instruction;
		parser >> instruction;

		if (instruction == L"set")
		{
			std::wstring destination;
			std::wstring value;
			parser >> destination >> value;
			AddRead(reads, value);
			continue;
		}

		if (instruction == L"op")
		{
			std::wstring operation;
			std::wstring destination;
			std::wstring left;
			std::wstring right;
			parser >> operation >> destination >> left >> right;
			AddRead(reads, left);
			AddRead(reads, right);
			continue;
		}

		std::wstring operand;
		while (parser >> operand)
			AddRead(reads, operand);
	}

	std::vector<std::wstring> optimized;
	for (const std::wstring& line : lines)
	{
		std::wstringstream parser(line);
		std::wstring instruction;
		std::wstring destination;
		std::wstring value;
		parser >> instruction >> destination >> value;

		if (instruction == L"set" && IsUserVariable(destination) &&
			reads.find(destination) == reads.end())
			continue;

		optimized.push_back(line);
	}

	return optimized;
}
