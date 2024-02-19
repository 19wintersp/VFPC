// <https://www.kdab.com/jsonify-with-nlohmann-json/>

#include <nlohmann/json.hpp>

#include <memory>
#include <optional>
#include <variant>
#include <vector>

namespace nlohmann {
	template<typename T, typename... Ts>
	void variant_from_json(const nlohmann::json &j, std::variant<Ts...> &data) {
		try {
			data = j.get<T>();
		} catch (...) {}
	}

	template<typename... Ts>
	struct adl_serializer<std::variant<Ts...>> {
		static void to_json(nlohmann::json &j, const std::variant<Ts...> &data) {
			std::visit([&j](const auto &v) { j = v; }, data);
		}

		static void from_json(const nlohmann::json &j, std::variant<Ts...> &data) {
			(variant_from_json<Ts>(j, data), ...);
		}
	};

	template<typename T>
	struct adl_serializer<std::unique_ptr<T>> {
		static void from_json(const nlohmann::json &j, std::unique_ptr<T> &data) {
			T &&inner = j.get<T>();
			data = std::make_unique<T>(std::move(inner));
		}
	};

	template<typename>   constexpr bool is_optional = false;
	template<typename T> constexpr bool is_optional<std::optional<T>> = true;

	template<typename>   constexpr bool is_vector = false;
	template<typename T> constexpr bool is_vector<std::vector<T>> = true;

	template<typename T>
	void extended_from_json(const char *key, const nlohmann::json &j, T &value) {
		const auto it = j.find(key);
		if constexpr (is_optional<T>) {
			value = it == j.end() ? std::nullopt : T{it->get<typename T::value_type>()};
		} else if constexpr (is_vector<T>) {
			value = it == j.end() ? T{} : it->get<T>();
		} else if constexpr (std::is_same_v<T, bool>) {
			value = it == j.end() ? false : it->get<bool>();
		} else {
			j.at(key).get_to(value);
		}
	}
}

#define EXTEND_JSON_FROM(v1) extended_from_json(#v1, j, value.v1);

// derives a deserialize implementation, adding defaults for missing values
#define NLOHMANN_JSONIFY_DESERIALIZE_STRUCT(Type, ...)                       \
  inline void from_json(const nlohmann::json &j, Type &value) {              \
	  NLOHMANN_JSON_EXPAND(NLOHMANN_JSON_PASTE(EXTEND_JSON_FROM, __VA_ARGS__)) \
  }
