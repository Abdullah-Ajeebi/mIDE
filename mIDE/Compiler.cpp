#include "Compiler.h"
#include "Optimizer.h"
#include "MindustryBuiltins.inl" // dynamically generated

#include <cwctype>
#include <map>
#include <sstream>
#include <vector>

namespace
{
	enum class TokenKind { End, Identifier, Number, String, Int, Return, Plus, Minus, Star, Slash, Assign, Semicolon, Comma, LParen, RParen, LBrace, RBrace, Less, LessEqual, Greater, GreaterEqual, Equal, NotEqual };

	struct Token
	{
		TokenKind kind;
		std::wstring text;
		int line;
	};

	class Compiler
	{
	public:
		explicit Compiler(const std::wstring& source, bool optimize) : source_(source), optimize_(optimize)
		{
			RegisterMindustryBuiltins(functions_);
			Next();
		}

		CompileResult Run()
		{
			CompileResult result;
			try
			{
				ParseProgram();
				result.success = true;
				result.output = ResolveOutput();
			}
			catch (const std::wstring& error)
			{
				result.error = error;
				result.errorLine = current_.line;
			}
			return result;
		}

	private:
		const std::wstring& source_;
		size_t position_ = 0;
		int line_ = 1;
		Token current_{ TokenKind::End, L"", 1 };
		std::wostringstream output_;
		std::map<std::wstring, bool> variables_;
		std::map<std::wstring, std::wstring> variableNames_;
		std::map<std::wstring, std::wstring> knownConstants_;
		bool optimize_ = true;
		struct Function
		{
			std::vector<std::wstring> parameters;
			bool builtin = false;
			bool returnsValue = true;
		};
		std::map<std::wstring, Function> functions_;
		size_t temporary_ = 0;
		bool parsingMain_ = false;
		bool parsingBuiltinCall_ = false;

		std::wstring NewTemporary()
		{
			return L"__t" + std::to_wstring(temporary_++);
		}

		void Emit(const std::wstring& line)
		{
			output_ << line << L"\n";
		}

		std::wstring ResolveOutput()
		{
			std::vector<std::wstring> lines;
			std::wstring text = output_.str();
			std::wstringstream input(text);
			std::wstring line;
			std::map<std::wstring, int> labelLines;
			while (std::getline(input, line))
			{
				if (line.rfind(L"__label ", 0) == 0)
				{
					labelLines[line.substr(8)] = static_cast<int>(lines.size());
					continue;
				}
				lines.push_back(line);
			}

			for (std::wstring& outputLine : lines)
			{
				for (const auto& label : labelLines)
				{
					const std::wstring placeholder = L"@" + label.first;
					const size_t position = outputLine.find(placeholder);
					if (position != std::wstring::npos)
						outputLine.replace(position, placeholder.size(), std::to_wstring(label.second));
				}
			}

			std::wostringstream resolved;
			for (const std::wstring& outputLine : lines)
				resolved << outputLine << L"\n";
			return resolved.str();
		}

		[[noreturn]] void Fail(const std::wstring& message) const
		{
			throw message;
		}

		void Next()
		{
			while (position_ < source_.size())
			{
				wchar_t c = source_[position_];
				if (c == L' ' || c == L'\t' || c == L'\r') { ++position_; continue; }
				if (c == L'\n') { ++position_; ++line_; continue; }
				if (c == L'/' && position_ + 1 < source_.size() && source_[position_ + 1] == L'/')
				{
					position_ += 2;
					while (position_ < source_.size() && source_[position_] != L'\n') ++position_;
					continue;
				}
				break;
			}

			current_.line = line_;
			if (position_ == source_.size()) { current_ = { TokenKind::End, L"", line_ }; return; }
			wchar_t c = source_[position_++];
			if (iswalpha(c) || c == L'_' || c == L'@')
			{
				std::wstring text(1, c);
				while (position_ < source_.size() && (iswalnum(source_[position_]) || source_[position_] == L'_' || source_[position_] == L'@' || source_[position_] == L'-')) text += source_[position_++];
				if (text == L"int" || text == L"double" || text == L"float" || text == L"void" || text == L"bool")
					current_ = { TokenKind::Int, text, line_ };
				else if (text == L"return") current_ = { TokenKind::Return, text, line_ };
				else current_ = { TokenKind::Identifier, text, line_ };
				return;
			}
			if (iswdigit(c))
			{
				std::wstring text(1, c);
				bool hasDot = false;
				while (position_ < source_.size())
				{
					wchar_t nextC = source_[position_];
					if (iswdigit(nextC))
					{
						text += source_[position_++];
					}
					else if (nextC == L'.' && !hasDot && position_ + 1 < source_.size() && iswdigit(source_[position_ + 1]))
					{
						hasDot = true;
						text += source_[position_++];
					}
					else
					{
						break;
					}
				}
				current_ = { TokenKind::Number, text, line_ };
				return;
			}
			if (c == L'"')
			{
				std::wstring value(1, c);
				bool escaped = false;
				while (position_ < source_.size())
				{
					wchar_t character = source_[position_++];
					value += character;
					if (escaped)
					{
						escaped = false;
						continue;
					}
					if (character == L'\\')
					{
						escaped = true;
						continue;
					}
					if (character == L'"')
					{
						current_ = { TokenKind::String, value, line_ };
						return;
					}
					if (character == L'\n')
						Fail(L"unterminated string literal");
				}
				Fail(L"unterminated string literal");
			}
			TokenKind kind = TokenKind::End;
			switch (c)
			{
			case L'+': kind = TokenKind::Plus; break; case L'-': kind = TokenKind::Minus; break;
			case L'*': kind = TokenKind::Star; break; case L'/': kind = TokenKind::Slash; break;
			case L';': kind = TokenKind::Semicolon; break;
			case L'<':
				if (position_ < source_.size() && source_[position_] == L'=') {
					++position_;
					current_ = { TokenKind::LessEqual, L"<=", line_ };
				}
				else {
					current_ = { TokenKind::Less, L"<", line_ };
				}
				return;

			case L'>':
				if (position_ < source_.size() && source_[position_] == L'=') {
					++position_;
					current_ = { TokenKind::GreaterEqual, L">=", line_ };
				}
				else {
					current_ = { TokenKind::Greater, L">", line_ };
				}
				return;

			case L'=':
				if (position_ < source_.size() && source_[position_] == L'=') {
					++position_;
					current_ = { TokenKind::Equal, L"==", line_ };
				}
				else {
					current_ = { TokenKind::Assign, L"=", line_ };
				}
				return;

			case L'!':
				if (position_ < source_.size() && source_[position_] == L'=') {
					++position_;
					current_ = { TokenKind::NotEqual, L"!=", line_ };
				}
				else {
					Fail(L"unexpected '!'");
				}
				return;
			case L',': kind = TokenKind::Comma; break;
			case L'(': kind = TokenKind::LParen; break; case L')': kind = TokenKind::RParen; break;
			case L'{': kind = TokenKind::LBrace; break; case L'}': kind = TokenKind::RBrace; break;
			default: Fail(L"unexpected character");
			}
			current_ = { kind, std::wstring(1, c), line_ };
		}

		void Expect(TokenKind kind, const wchar_t* message)
		{
			if (current_.kind != kind) Fail(message);
			Next();
		}

		void ParseProgram()
		{
			bool foundMain = false;
			Emit(L"jump @__fn_main always");
			while (current_.kind != TokenKind::End)
			{
				Function function = ParseFunctionHeader();
				if (currentFunction_ == L"main")
				{
					if (foundMain) Fail(L"duplicate 'main'");
					foundMain = true;
				}
				ParseFunctionBody(function);
			}
			if (!foundMain) Fail(L"expected 'int main()'");
			Emit(L"end");
		}

		std::wstring currentFunction_;

		Function ParseFunctionHeader()
		{
			Expect(TokenKind::Int, L"expected function return type 'int'");
			if (current_.kind != TokenKind::Identifier) Fail(L"expected function name");
			currentFunction_ = current_.text;
			Next();
			Expect(TokenKind::LParen, L"expected '('");

			Function function;
			if (current_.kind != TokenKind::RParen)
			{
				while (true)
				{
					Expect(TokenKind::Int, L"expected parameter type 'int'");
					if (current_.kind != TokenKind::Identifier) Fail(L"expected parameter name");
					function.parameters.push_back(current_.text);
					Next();
					if (current_.kind != TokenKind::Comma) break;
					Next();
				}
			}
			Expect(TokenKind::RParen, L"expected ')'");
			if (!functions_.insert({ currentFunction_, function }).second)
				Fail(L"function already declared: " + currentFunction_);
			return function;
		}

		void ParseFunctionBody(const Function& function)
		{
			Expect(TokenKind::LBrace, L"expected '{'");
			variables_.clear();
			variableNames_.clear();
			knownConstants_.clear();
			for (const std::wstring& parameter : function.parameters)
			{
				if (!variables_.insert({ parameter, true }).second)
					Fail(L"duplicate parameter: " + parameter);
				variableNames_[parameter] = L"__arg_" + currentFunction_ + L"_" +
					std::to_wstring(variableNames_.size());
			}

			parsingMain_ = currentFunction_ == L"main";
			Emit(L"__label __fn_" + currentFunction_);

			while (current_.kind != TokenKind::RBrace && current_.kind != TokenKind::End)
				ParseStatement();
			Expect(TokenKind::RBrace, L"expected '}'");
			if (!parsingMain_)
				Emit(L"set @counter __return_pc");
			parsingMain_ = false;
		}

		void ParseStatement()
		{
			if (current_.kind == TokenKind::Int)
			{
				Next();
				if (current_.kind != TokenKind::Identifier) Fail(L"expected variable name");
				std::wstring name = current_.text; Next();
				if (!variables_.insert({ name, true }).second) Fail(L"variable already declared: " + name);
				Expect(TokenKind::Assign, L"expected '=' after variable name");
				std::wstring value = ParseExpression(); Expect(TokenKind::Semicolon, L"expected ';'");
				Emit(L"set " + name + L" " + value);
				if (optimize_ && IsOptimizerInteger(value))
					knownConstants_[name] = value;
				else
					knownConstants_.erase(name);
				return;
			}
			if (current_.kind == TokenKind::Return)
			{
				Next();
				std::wstring value = ParseExpression(); Expect(TokenKind::Semicolon, L"expected ';'");
				Emit(L"set __return " + value);
				if (!parsingMain_)
					Emit(L"set @counter __return_pc");
				return;
			}
			if (current_.kind == TokenKind::Identifier)
			{
				std::wstring name = current_.text; Next();
				if (current_.kind == TokenKind::LParen)
				{
					auto function = functions_.find(name);
					if (function == functions_.end())
						Fail(L"unknown function: " + name);
					ParseCall(name, function->second);
					Expect(TokenKind::Semicolon, L"expected ';'");
					return;
				}
				if (!variables_.count(name)) Fail(L"unknown variable: " + name);
				Expect(TokenKind::Assign, L"expected '='");
				std::wstring value = ParseExpression(); Expect(TokenKind::Semicolon, L"expected ';'");
				Emit(L"set " + ResolveVariable(name) + L" " + value);
				if (optimize_ && IsOptimizerInteger(value))
					knownConstants_[name] = value;
				else
					knownConstants_.erase(name);
				return;
			}
			Fail(L"expected declaration, assignment, or return");
		}

		std::wstring ParseCall(const std::wstring& name, const Function& function)
		{
			Next();
			bool prevBuiltin = parsingBuiltinCall_;
			if (function.builtin) parsingBuiltinCall_ = true;

			std::vector<std::wstring> arguments;
			if (current_.kind != TokenKind::RParen)
			{
				while (true)
				{
					arguments.push_back(ParseExpression());
					if (current_.kind != TokenKind::Comma) break;
					Next();
				}
			}
			Expect(TokenKind::RParen, L"expected ')'");

			parsingBuiltinCall_ = prevBuiltin;
			if (arguments.size() != function.parameters.size())
				Fail(L"wrong number of arguments for function: " + name);

			if (function.builtin)
			{
				std::wstring builtinResult;
				if (TryEmitMindustryBuiltin(name, arguments,
					[this](const std::wstring& line) { Emit(line); },
					[this]() { return NewTemporary(); },
					builtinResult))
				{
					return builtinResult;
				}
				return L"0";
			}

			knownConstants_.clear();
			std::wstring result = NewTemporary();
			for (size_t i = 0; i < arguments.size(); ++i)
				Emit(L"set __arg_" + name + L"_" + std::to_wstring(i) + L" " + arguments[i]);
			Emit(L"op add __return_pc @counter 2");
			Emit(L"set @counter @__fn_" + name);
			Emit(L"set " + result + L" __return");
			return result;
		}

		std::wstring ParseAdditive()
		{
			std::wstring left = ParseTerm();
			while (current_.kind == TokenKind::Plus || current_.kind == TokenKind::Minus)
			{
				TokenKind op = current_.kind; Next();
				std::wstring right = ParseTerm();
				std::wstring folded = optimize_ ? TryFoldBinary(
					op == TokenKind::Plus ? OptimizerOperator::Add : OptimizerOperator::Subtract,
					left, right) : L"";
				if (!folded.empty())
				{
					left = folded;
					continue;
				}
				std::wstring temp = NewTemporary();
				Emit(L"op " + std::wstring(op == TokenKind::Plus ? L"add" : L"sub") + L" " + temp + L" " + left + L" " + right);
				left = temp;
			}
			return left;
		}

		std::wstring ParseTerm()
		{
			std::wstring left = ParseFactor();
			while (current_.kind == TokenKind::Star || current_.kind == TokenKind::Slash)
			{
				TokenKind op = current_.kind; Next();
				std::wstring right = ParseFactor();
				if (op == TokenKind::Slash && right == L"0")
					Fail(L"division by zero");
				std::wstring folded = optimize_ ? TryFoldBinary(
					op == TokenKind::Star ? OptimizerOperator::Multiply : OptimizerOperator::Divide,
					left, right) : L"";
				if (!folded.empty())
				{
					left = folded;
					continue;
				}
				std::wstring temp = NewTemporary();
				Emit(L"op " + std::wstring(op == TokenKind::Star ? L"mul" : L"idiv") + L" " + temp + L" " + left + L" " + right);
				left = temp;
			}
			return left;
		}

		std::wstring ParseExpression()
		{
			std::wstring left = ParseAdditive();

			while (current_.kind == TokenKind::Less || current_.kind == TokenKind::LessEqual ||
				current_.kind == TokenKind::Greater || current_.kind == TokenKind::GreaterEqual ||
				current_.kind == TokenKind::Equal || current_.kind == TokenKind::NotEqual)
			{
				TokenKind op = current_.kind;
				Next();
				std::wstring right = ParseAdditive();

				std::wstring mlogOp;
				switch (op) {
				case TokenKind::Less:         mlogOp = L"lessThan"; break;
				case TokenKind::LessEqual:    mlogOp = L"lessThanEq"; break;
				case TokenKind::Greater:      mlogOp = L"greaterThan"; break;
				case TokenKind::GreaterEqual: mlogOp = L"greaterThanEq"; break;
				case TokenKind::Equal:        mlogOp = L"equal"; break;
				case TokenKind::NotEqual:     mlogOp = L"notEqual"; break;
				default: break;
				}

				std::wstring temp = NewTemporary();
				Emit(L"op " + mlogOp + L" " + temp + L" " + left + L" " + right);
				left = temp;
			}

			return left;
		}

		std::wstring ParseFactor()
		{
			if (current_.kind == TokenKind::Number) { std::wstring value = current_.text; Next(); return value; }
			if (current_.kind == TokenKind::String) { std::wstring value = current_.text; Next(); return value; }
			if (current_.kind == TokenKind::Identifier)
			{
				std::wstring name = current_.text;
				Next();
				if (current_.kind != TokenKind::LParen)
				{
					if (!variables_.count(name)) {
						if (parsingBuiltinCall_)
							return name;
						Fail(L"unknown variable: " + name);
					}
					if (optimize_)
					{
						auto constant = knownConstants_.find(name);
						if (constant != knownConstants_.end())
							return constant->second;
					}
					return ResolveVariable(name);
				}

				auto function = functions_.find(name);
				if (function == functions_.end())
					Fail(L"unknown function: " + name);
				return ParseCall(name, function->second);
			}
			if (current_.kind == TokenKind::LParen)
			{
				Next(); std::wstring value = ParseExpression(); Expect(TokenKind::RParen, L"expected ')'"); return value;
			}
			Fail(L"expected number, variable, or expression");
		}

		std::wstring ResolveVariable(const std::wstring& name) const
		{
			auto alias = variableNames_.find(name);
			return alias == variableNames_.end() ? name : alias->second;
		}
	};
}

CompileResult CompileCToMlog(const std::wstring& source, bool optimize)
{
	return Compiler(source, optimize).Run();
}
