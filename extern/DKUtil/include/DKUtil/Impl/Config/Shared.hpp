#pragma once

#include "DKUtil/Impl/pch.hpp"
#include "DKUtil/Logger.hpp"
#include "DKUtil/Utility.hpp"

namespace DKUtil::Config
{
	inline auto GetPath(const std::string_view a_file) noexcept
	{
		std::filesystem::path dir{ CONFIG_ENTRY };
		if (dir.is_relative() || dir.empty()) {
			dir = std::filesystem::current_path() / dir;
		}

		std::filesystem::path file{ a_file.data() };
		if (!a_file.empty() && file.is_relative()) {
			file = dir / file;
		}

		return file.string();
	}

	template <bool RECURSIVE = false>
	inline std::vector<std::string> GetAllFiles(std::string_view a_path = {}, std::string_view a_ext = {}, std::string_view a_prefix = {}, std::string_view a_suffix = {}) noexcept
	{
		using dir_iterator = std::conditional_t<RECURSIVE, std::filesystem::recursive_directory_iterator, std::filesystem::directory_iterator>;

		std::vector<std::string> files;
		auto                     file_iterator = [&](const std::filesystem::directory_entry& a_file) {
            if (a_file.exists() &&
                !a_file.path().empty()) {
                if (!a_ext.empty() && a_file.path().extension() != a_ext) {
                    return;
                }

                const auto path = a_file.path().string();

                if (!a_prefix.empty() && path.find(a_prefix) != std::string::npos) {
                    files.push_back(path);
                } else if (!a_suffix.empty() && path.rfind(a_suffix) != std::string::npos) {
                    files.push_back(path);
                } else if (a_prefix.empty() && a_suffix.empty()) {
                    files.push_back(path);
                }
            }
		};

		std::string dir(MAX_PATH + 1, ' ');
		auto        res = GetModuleFileNameA(nullptr, dir.data(), MAX_PATH + 1);
		if (res == 0) {
			ERROR("DKU_C: Unable to acquire valid path using default null path argument!\nExpected: Current directory\nResolved: NULL");
		}

		auto eol = dir.find_last_of("\\/");
		dir = dir.substr(0, eol);

		auto path = a_path.empty() ? std::filesystem::path{ dir } : std::filesystem::path{ a_path };
		if (!is_directory(path.parent_path())) {
			path = dir / path;
		}

		std::ranges::for_each(dir_iterator(path), file_iterator);
		std::ranges::sort(files);

		return files;
	}

	namespace detail
	{
		struct section_key_hash
		{
			size_t operator()(const std::pair<std::string_view, std::string_view>& key) const
			{
				// Hash combining algorithm taken from boost::hash_combine
				auto hash = std::hash<std::string_view>()(key.first);
				hash ^= std::hash<std::string_view>()(key.second) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
				return hash;
			}
		};

		class IData;
		// <key, section?>, data*
		using manager = std::unordered_map<std::pair<std::string_view, std::string_view>, detail::IData*, section_key_hash>;

		inline static std::uint32_t _Count{ 0 };

		class IParser
		{
		public:
			explicit IParser(std::string_view a_file, const std::uint32_t a_id, manager& a_manager) :
				_filename(Logger::detail::short_file(a_file.data())), _filepath(GetPath(a_file)), _id(a_id), _manager(a_manager)
			{}

			constexpr IParser() = delete;
			constexpr IParser(const IParser&) noexcept = default;
			constexpr IParser(IParser&&) noexcept = default;
			constexpr virtual ~IParser() = default;

			// accessor
			[[nodiscard]] constexpr auto*            data() noexcept { return _content.data(); }
			[[nodiscard]] constexpr auto&            content() const noexcept { return _content; }
			[[nodiscard]] constexpr std::string_view filename() const noexcept { return _filename; }
			[[nodiscard]] constexpr std::string_view filepath() const noexcept { return _filepath; }

			virtual void Parse(const char* = nullptr) noexcept = 0;
			virtual void Write(const std::string_view) noexcept = 0;
			virtual void Generate() noexcept = 0;

		protected:
			const std::uint32_t _id;
			const std::string   _filepath;
			const std::string   _filename;
			std::string         _content;
			const manager&      _manager;
		};
	}
}
