// Minimal JSON DOM, parser and JavaScript-compatible serializer.
//
// Parsing is not IO: callers hand in text. The serializer reproduces JSON.stringify number and string
// formatting exactly, and StableStringify reproduces src/prng.ts stableStringify (sorted keys), so replay
// hashes computed here equal the TypeScript hashes.

#pragma once

#include <string>
#include <utility>
#include <vector>

namespace RubiconCore
{
	class FJson
	{
	public:
		enum class EType : unsigned char { Null, Bool, Number, String, Array, Object };

		FJson() = default;
		static FJson MakeNull() { return FJson(); }
		static FJson MakeBool(bool V) { FJson J; J.Type = EType::Bool; J.BoolValue = V; return J; }
		static FJson MakeNumber(double V) { FJson J; J.Type = EType::Number; J.NumberValue = V; return J; }
		static FJson MakeString(std::string V) { FJson J; J.Type = EType::String; J.StringValue = std::move(V); return J; }
		static FJson MakeArray() { FJson J; J.Type = EType::Array; return J; }
		static FJson MakeObject() { FJson J; J.Type = EType::Object; return J; }

		EType Type = EType::Null;
		bool BoolValue = false;
		double NumberValue = 0;
		std::string StringValue;
		std::vector<FJson> Items;                          // Array
		std::vector<std::pair<std::string, FJson>> Fields; // Object, insertion order (last duplicate wins on Find)

		bool IsNull() const { return Type == EType::Null; }
		bool IsBool() const { return Type == EType::Bool; }
		bool IsNumber() const { return Type == EType::Number; }
		bool IsString() const { return Type == EType::String; }
		bool IsArray() const { return Type == EType::Array; }
		bool IsObject() const { return Type == EType::Object; }

		/** Object member lookup; nullptr models JavaScript `undefined`. */
		const FJson* Find(const std::string& Key) const;
		FJson& Set(const std::string& Key, FJson Value);
		FJson& Push(FJson Value);
	};

	bool ParseJson(const std::string& Utf8, FJson& Out, std::string* OutError = nullptr);

	/** JSON.stringify(number): shortest round-trip digits, ECMAScript Number::toString layout. NaN/Inf -> "null". */
	std::string JsNumberToJson(double V);
	/** String(number) in JavaScript (NaN -> "NaN", Infinity -> "Infinity"). */
	std::string JsNumberToString(double V);
	/** JSON.stringify(string). */
	std::string JsQuote(const std::string& Utf8);
	/** JSON.stringify with insertion-order keys. */
	std::string Stringify(const FJson& V);
	/** src/prng.ts stableStringify: object keys sorted by UTF-16 code unit order. */
	std::string StableStringify(const FJson& V);
	/** String(value) in JavaScript, with nullptr standing for undefined. */
	std::string JsString(const FJson* V);
}
