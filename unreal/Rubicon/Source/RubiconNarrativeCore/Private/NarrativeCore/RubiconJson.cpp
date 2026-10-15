#include "RubiconFloatEnv.h"
#include "NarrativeCore/RubiconJson.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <locale>
#include <sstream>
#if __has_include(<charconv>)
	#include <charconv>
#endif

namespace RubiconCore
{
	const FJson* FJson::Find(const std::string& Key) const
	{
		if (Type != EType::Object) return nullptr;
		for (auto It = Fields.rbegin(); It != Fields.rend(); ++It)
		{
			if (It->first == Key) return &It->second;
		}
		return nullptr;
	}

	FJson& FJson::Set(const std::string& Key, FJson Value)
	{
		for (auto& F : Fields)
		{
			if (F.first == Key) { F.second = std::move(Value); return F.second; }
		}
		Fields.emplace_back(Key, std::move(Value));
		return Fields.back().second;
	}

	FJson& FJson::Push(FJson Value)
	{
		Items.push_back(std::move(Value));
		return Items.back();
	}

	namespace JsonDetail
	{
		/** Correctly rounded decimal -> double, independent of the C locale. */
		bool ParseDouble(const char* Begin, const char* End, double& Out)
		{
#if defined(__cpp_lib_to_chars) && __cpp_lib_to_chars >= 201611L && !defined(RUBICON_NO_TO_CHARS)
			const auto R = std::from_chars(Begin, End, Out);
			return R.ec == std::errc() && R.ptr == End;
#else
			std::istringstream In(std::string(Begin, End));
			In.imbue(std::locale::classic());
			In >> Out;
			return !In.fail() && In.peek() == std::char_traits<char>::eof();
#endif
		}

		/** Shortest round-trip decimal digits and exponent: V == 0.D1D2...Dk * 10^N10. */
		void ShortestDigits(double V, std::string& Digits, int& N10)
		{
			char Buf[64];
#if defined(__cpp_lib_to_chars) && __cpp_lib_to_chars >= 201611L && !defined(RUBICON_NO_TO_CHARS)
			const auto R = std::to_chars(Buf, Buf + sizeof(Buf), V, std::chars_format::scientific);
			std::string S(Buf, R.ptr);
#else
			std::string S;
			for (int P = 0; P <= 17; ++P)
			{
				std::snprintf(Buf, sizeof(Buf), "%.*e", P, V);
				for (char* C = Buf; *C; ++C) if (*C == ',') *C = '.';
				double Back = 0;
				const size_t Len = std::strlen(Buf);
				if (ParseDouble(Buf, Buf + Len, Back) && Back == V) { S.assign(Buf, Len); break; }
			}
#endif
			// S = d[.ddd]e[+-]xx
			const size_t E = S.find('e');
			std::string Mant = S.substr(0, E);
			const int Exp10 = std::atoi(S.c_str() + E + 1);
			Digits.clear();
			for (char C : Mant) if (C >= '0' && C <= '9') Digits.push_back(C);
			while (Digits.size() > 1 && Digits.back() == '0') Digits.pop_back();
			N10 = Exp10 + 1;
		}

		void AppendUtf8(std::string& Out, uint32_t Cp)
		{
			if (Cp < 0x80) Out.push_back(static_cast<char>(Cp));
			else if (Cp < 0x800) { Out.push_back(static_cast<char>(0xc0 | (Cp >> 6))); Out.push_back(static_cast<char>(0x80 | (Cp & 0x3f))); }
			else if (Cp < 0x10000) { Out.push_back(static_cast<char>(0xe0 | (Cp >> 12))); Out.push_back(static_cast<char>(0x80 | ((Cp >> 6) & 0x3f))); Out.push_back(static_cast<char>(0x80 | (Cp & 0x3f))); }
			else { Out.push_back(static_cast<char>(0xf0 | (Cp >> 18))); Out.push_back(static_cast<char>(0x80 | ((Cp >> 12) & 0x3f))); Out.push_back(static_cast<char>(0x80 | ((Cp >> 6) & 0x3f))); Out.push_back(static_cast<char>(0x80 | (Cp & 0x3f))); }
		}

		class FParser
		{
		public:
			FParser(const std::string& InText) : Text(InText) {}

			bool Run(FJson& Out, std::string* OutError)
			{
				SkipWs();
				if (!Value(Out, 0)) { if (OutError) *OutError = Error + " at byte " + std::to_string(Pos); return false; }
				SkipWs();
				if (Pos != Text.size()) { if (OutError) *OutError = "trailing characters at byte " + std::to_string(Pos); return false; }
				return true;
			}

		private:
			const std::string& Text;
			size_t Pos = 0;
			std::string Error;

			bool Fail(const char* Msg) { Error = Msg; return false; }
			void SkipWs() { while (Pos < Text.size() && (Text[Pos] == ' ' || Text[Pos] == '\t' || Text[Pos] == '\n' || Text[Pos] == '\r')) ++Pos; }
			bool Literal(const char* L)
			{
				const size_t N = std::strlen(L);
				if (Text.compare(Pos, N, L) != 0) return false;
				Pos += N;
				return true;
			}

			bool Value(FJson& Out, int Depth)
			{
				if (Depth > 256) return Fail("nesting too deep");
				if (Pos >= Text.size()) return Fail("unexpected end");
				const char C = Text[Pos];
				if (C == '{') return Object(Out, Depth);
				if (C == '[') return Array(Out, Depth);
				if (C == '"') { Out = FJson::MakeString(""); return String(Out.StringValue); }
				if (C == 't') { if (!Literal("true")) return Fail("bad literal"); Out = FJson::MakeBool(true); return true; }
				if (C == 'f') { if (!Literal("false")) return Fail("bad literal"); Out = FJson::MakeBool(false); return true; }
				if (C == 'n') { if (!Literal("null")) return Fail("bad literal"); Out = FJson::MakeNull(); return true; }
				return Number(Out);
			}

			bool Number(FJson& Out)
			{
				const size_t Start = Pos;
				if (Pos < Text.size() && Text[Pos] == '-') ++Pos;
				if (Pos >= Text.size() || !(Text[Pos] >= '0' && Text[Pos] <= '9')) return Fail("bad number");
				while (Pos < Text.size() && ((Text[Pos] >= '0' && Text[Pos] <= '9') || Text[Pos] == '.' || Text[Pos] == 'e' || Text[Pos] == 'E' || Text[Pos] == '+' || Text[Pos] == '-')) ++Pos;
				double V = 0;
				if (!ParseDouble(Text.data() + Start, Text.data() + Pos, V)) return Fail("bad number");
				Out = FJson::MakeNumber(V);
				return true;
			}

			bool Hex4(uint32_t& Out)
			{
				if (Pos + 4 > Text.size()) return false;
				Out = 0;
				for (int I = 0; I < 4; ++I)
				{
					const char C = Text[Pos++];
					Out <<= 4;
					if (C >= '0' && C <= '9') Out |= static_cast<uint32_t>(C - '0');
					else if (C >= 'a' && C <= 'f') Out |= static_cast<uint32_t>(C - 'a' + 10);
					else if (C >= 'A' && C <= 'F') Out |= static_cast<uint32_t>(C - 'A' + 10);
					else return false;
				}
				return true;
			}

			bool String(std::string& Out)
			{
				++Pos; // opening quote
				while (true)
				{
					if (Pos >= Text.size()) return Fail("unterminated string");
					const char C = Text[Pos++];
					if (C == '"') return true;
					if (static_cast<unsigned char>(C) < 0x20) return Fail("control character in string");
					if (C != '\\') { Out.push_back(C); continue; }
					if (Pos >= Text.size()) return Fail("bad escape");
					const char E = Text[Pos++];
					switch (E)
					{
					case '"': Out.push_back('"'); break;
					case '\\': Out.push_back('\\'); break;
					case '/': Out.push_back('/'); break;
					case 'b': Out.push_back('\b'); break;
					case 'f': Out.push_back('\f'); break;
					case 'n': Out.push_back('\n'); break;
					case 'r': Out.push_back('\r'); break;
					case 't': Out.push_back('\t'); break;
					case 'u':
					{
						uint32_t Cp = 0;
						if (!Hex4(Cp)) return Fail("bad \\u escape");
						if (Cp >= 0xd800 && Cp < 0xdc00 && Pos + 6 <= Text.size() && Text[Pos] == '\\' && Text[Pos + 1] == 'u')
						{
							const size_t Save = Pos;
							Pos += 2;
							uint32_t Lo = 0;
							if (Hex4(Lo) && Lo >= 0xdc00 && Lo < 0xe000) Cp = 0x10000 + ((Cp - 0xd800) << 10) + (Lo - 0xdc00);
							else Pos = Save;
						}
						AppendUtf8(Out, Cp);
						break;
					}
					default: return Fail("bad escape");
					}
				}
			}

			bool Array(FJson& Out, int Depth)
			{
				Out = FJson::MakeArray();
				++Pos;
				SkipWs();
				if (Pos < Text.size() && Text[Pos] == ']') { ++Pos; return true; }
				while (true)
				{
					SkipWs();
					FJson Item;
					if (!Value(Item, Depth + 1)) return false;
					Out.Items.push_back(std::move(Item));
					SkipWs();
					if (Pos >= Text.size()) return Fail("unterminated array");
					if (Text[Pos] == ',') { ++Pos; continue; }
					if (Text[Pos] == ']') { ++Pos; return true; }
					return Fail("expected , or ]");
				}
			}

			bool Object(FJson& Out, int Depth)
			{
				Out = FJson::MakeObject();
				++Pos;
				SkipWs();
				if (Pos < Text.size() && Text[Pos] == '}') { ++Pos; return true; }
				while (true)
				{
					SkipWs();
					if (Pos >= Text.size() || Text[Pos] != '"') return Fail("expected key");
					std::string Key;
					if (!String(Key)) return false;
					SkipWs();
					if (Pos >= Text.size() || Text[Pos] != ':') return Fail("expected :");
					++Pos;
					SkipWs();
					FJson Item;
					if (!Value(Item, Depth + 1)) return false;
					Out.Fields.emplace_back(std::move(Key), std::move(Item));
					SkipWs();
					if (Pos >= Text.size()) return Fail("unterminated object");
					if (Text[Pos] == ',') { ++Pos; continue; }
					if (Text[Pos] == '}') { ++Pos; return true; }
					return Fail("expected , or }");
				}
			}
		};

		/** UTF-16 code unit order (JavaScript default sort) for UTF-8 keys. */
		std::u16string ToUtf16(const std::string& S)
		{
			std::u16string Out;
			for (size_t I = 0; I < S.size();)
			{
				const unsigned char C0 = static_cast<unsigned char>(S[I]);
				uint32_t Cp = C0;
				size_t Len = 1;
				if (C0 >= 0xf0 && I + 3 < S.size()) { Cp = ((C0 & 7u) << 18) | ((S[I + 1] & 0x3fu) << 12) | ((S[I + 2] & 0x3fu) << 6) | (S[I + 3] & 0x3fu); Len = 4; }
				else if (C0 >= 0xe0 && I + 2 < S.size()) { Cp = ((C0 & 0xfu) << 12) | ((S[I + 1] & 0x3fu) << 6) | (S[I + 2] & 0x3fu); Len = 3; }
				else if (C0 >= 0xc0 && I + 1 < S.size()) { Cp = ((C0 & 0x1fu) << 6) | (S[I + 1] & 0x3fu); Len = 2; }
				I += Len;
				if (Cp >= 0x10000) { Cp -= 0x10000; Out.push_back(static_cast<char16_t>(0xd800 + (Cp >> 10))); Out.push_back(static_cast<char16_t>(0xdc00 + (Cp & 0x3ff))); }
				else Out.push_back(static_cast<char16_t>(Cp));
			}
			return Out;
		}

		void Write(const FJson& V, bool bSorted, std::string& Out)
		{
			switch (V.Type)
			{
			case FJson::EType::Null: Out += "null"; return;
			case FJson::EType::Bool: Out += V.BoolValue ? "true" : "false"; return;
			case FJson::EType::Number: Out += JsNumberToJson(V.NumberValue); return;
			case FJson::EType::String: Out += JsQuote(V.StringValue); return;
			case FJson::EType::Array:
				Out.push_back('[');
				for (size_t I = 0; I < V.Items.size(); ++I)
				{
					if (I) Out.push_back(',');
					Write(V.Items[I], bSorted, Out);
				}
				Out.push_back(']');
				return;
			case FJson::EType::Object:
			{
				// Duplicate keys collapse to the last value, as JSON.parse does.
				std::vector<const std::pair<std::string, FJson>*> Order;
				for (const auto& F : V.Fields)
				{
					bool bReplaced = false;
					for (auto& O : Order) if (O->first == F.first) { O = &F; bReplaced = true; }
					if (!bReplaced) Order.push_back(&F);
				}
				if (bSorted)
				{
					std::stable_sort(Order.begin(), Order.end(), [](const auto* A, const auto* B) { return ToUtf16(A->first) < ToUtf16(B->first); });
				}
				Out.push_back('{');
				for (size_t I = 0; I < Order.size(); ++I)
				{
					if (I) Out.push_back(',');
					Out += JsQuote(Order[I]->first);
					Out.push_back(':');
					Write(Order[I]->second, bSorted, Out);
				}
				Out.push_back('}');
				return;
			}
			}
		}
	}

	bool ParseJson(const std::string& Utf8, FJson& Out, std::string* OutError)
	{
		JsonDetail::FParser P(Utf8);
		return P.Run(Out, OutError);
	}

	std::string JsNumberToString(double V)
	{
		if (std::isnan(V)) return "NaN";
		if (std::isinf(V)) return V > 0 ? "Infinity" : "-Infinity";
		if (V == 0) return "0"; // includes -0
		std::string Sign = V < 0 ? "-" : "";
		std::string D;
		int N = 0;
		JsonDetail::ShortestDigits(std::fabs(V), D, N);
		const int K = static_cast<int>(D.size());
		std::string Out = Sign;
		if (K <= N && N <= 21)
		{
			Out += D;
			Out.append(static_cast<size_t>(N - K), '0');
		}
		else if (0 < N && N <= 21)
		{
			Out += D.substr(0, static_cast<size_t>(N));
			Out.push_back('.');
			Out += D.substr(static_cast<size_t>(N));
		}
		else if (-6 < N && N <= 0)
		{
			Out += "0.";
			Out.append(static_cast<size_t>(-N), '0');
			Out += D;
		}
		else
		{
			const int E = N - 1;
			Out.push_back(D[0]);
			if (K > 1) { Out.push_back('.'); Out += D.substr(1); }
			Out.push_back('e');
			Out.push_back(E < 0 ? '-' : '+');
			Out += std::to_string(E < 0 ? -E : E);
		}
		return Out;
	}

	std::string JsNumberToJson(double V)
	{
		if (std::isnan(V) || std::isinf(V)) return "null";
		return JsNumberToString(V);
	}

	std::string JsQuote(const std::string& Utf8)
	{
		static const char Hex[] = "0123456789abcdef";
		std::string Out = "\"";
		for (char C : Utf8)
		{
			const unsigned char U = static_cast<unsigned char>(C);
			switch (C)
			{
			case '"': Out += "\\\""; break;
			case '\\': Out += "\\\\"; break;
			case '\b': Out += "\\b"; break;
			case '\f': Out += "\\f"; break;
			case '\n': Out += "\\n"; break;
			case '\r': Out += "\\r"; break;
			case '\t': Out += "\\t"; break;
			default:
				if (U < 0x20) { Out += "\\u00"; Out.push_back(Hex[U >> 4]); Out.push_back(Hex[U & 0xf]); }
				else Out.push_back(C);
			}
		}
		Out.push_back('"');
		return Out;
	}

	std::string Stringify(const FJson& V)
	{
		std::string Out;
		JsonDetail::Write(V, false, Out);
		return Out;
	}

	std::string StableStringify(const FJson& V)
	{
		std::string Out;
		JsonDetail::Write(V, true, Out);
		return Out;
	}

	std::string JsString(const FJson* V)
	{
		if (!V) return "undefined";
		switch (V->Type)
		{
		case FJson::EType::Null: return "null";
		case FJson::EType::Bool: return V->BoolValue ? "true" : "false";
		case FJson::EType::Number: return JsNumberToString(V->NumberValue);
		case FJson::EType::String: return V->StringValue;
		case FJson::EType::Array:
		{
			std::string Out;
			for (size_t I = 0; I < V->Items.size(); ++I)
			{
				if (I) Out.push_back(',');
				const FJson& E = V->Items[I];
				if (!E.IsNull()) Out += JsString(&E);
			}
			return Out;
		}
		case FJson::EType::Object: return "[object Object]";
		}
		return "";
	}
}
