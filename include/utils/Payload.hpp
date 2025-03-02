#pragma once

#include <cstddef>
#include <string.h>
#include <filesystem>
#include <fstream>
#include <vector>
#include <unordered_map>

namespace utils {
	class Payload {
		public:
			Payload() = default;
			Payload(const Payload&) = default;
			Payload(Payload &&) noexcept = default;
			virtual ~Payload() = default;

			Payload& operator=(const Payload&) = default;

			virtual void send(int fd) = 0;

			virtual void append(const std::uint8_t* data, size_t size);

			virtual std::string toString() const = 0;
			virtual std::unique_ptr<Payload> clone() const = 0;

			bool isSent() const;
			std::size_t size() const;

		protected:
			std::size_t _totalBytes { 0 };
			std::size_t _bytesSent { 0 };
	};

	class CgiPayload : public Payload {
		public:
			CgiPayload() = default;
			CgiPayload(const CgiPayload&) = default;
			CgiPayload(CgiPayload &&) noexcept = default;
			~CgiPayload() = default;

			CgiPayload& operator=(const CgiPayload&) = default;

			void send(int fd) override;
			void append(const std::uint8_t* data, size_t size) override;
			std::string toString() const override;
			std::unique_ptr<Payload> clone() const override;

			const std::unordered_map<std::string, std::string>& headerFields() const;

		private:
			std::vector<std::uint8_t> _buffer;
			std::unordered_map<std::string, std::string> _headerFields;
	};

	class StringPayload : public Payload {
		public:
			StringPayload(const std::string &message);
			StringPayload(const StringPayload&) = default;
			StringPayload(StringPayload &&) noexcept = default;
			~StringPayload() = default;

			StringPayload& operator=(const StringPayload&) = default;

			void send(int fd) override;
			void append(const std::uint8_t* data, size_t size) override;
			std::string toString() const override;
			std::unique_ptr<Payload> clone() const override;

			void setMessage(const std::string &message);

		private:
			std::string _message;
	};

	class FilePayload : public Payload {
		public:
			FilePayload(const std::filesystem::path &filePath);
			FilePayload(const FilePayload& other);
			FilePayload(FilePayload &&) noexcept = default;
			~FilePayload() = default;

			FilePayload& operator=(const FilePayload& other);

			void send(int fd) override;
			std::string toString() const override;
			std::unique_ptr<Payload> clone() const override;

		private:
			std::filesystem::path _filePath;
			mutable std::ifstream _ifstream;
	};
}
