#include "CaptureService.h"

#include <cwchar>
#include <functional>
#include <renderdoc_app.h>
#include <stdexcept>

namespace cs::renderdoc
{
	namespace
	{
		bool IsCaptureFile(const std::filesystem::directory_entry& a_entry)
		{
			return a_entry.is_regular_file() && _wcsicmp(a_entry.path().extension().c_str(), L".rdc") == 0;
		}
	}

	// FO4: capture operations stay independent of engine and UI ownership.
	void CaptureService::Attach(RENDERDOC_API_1_7_0* a_api)
	{
		_api = a_api;
		_lastCaptureCount = Count();
	}

	void CaptureService::SetDirectory(std::filesystem::path a_directory)
	{
		_directory = std::move(a_directory);
		_cacheValid = false;
	}

	bool CaptureService::HasSufficientDiskSpace(int a_frames) const
	{
		std::filesystem::create_directories(_directory);
		return std::filesystem::space(_directory).available >= RequiredSpaceBytes(a_frames);
	}

	void CaptureService::QueueComments(std::string a_comments)
	{
		_pendingComments = std::move(a_comments);
		_cacheValid = false;
	}

	void CaptureService::Poll(const std::string& a_automaticComments)
	{
		const auto count = Count();
		if (count <= _lastCaptureCount)
			return;

		for (auto index = _lastCaptureCount; index < count; ++index) {
			const auto path = PathUtf8(index);
			if (path.empty())
				break;
			const auto& comments = !_pendingComments.empty() ? _pendingComments : a_automaticComments;
			_api->SetCaptureFileComments(path.c_str(), comments.c_str());
			_pendingComments.clear();
			_lastCaptureCount = index + 1;
		}
		_cacheValid = false;
	}

	const std::vector<CaptureFile>& CaptureService::Inventory(bool a_forceRefresh)
	{
		const auto now = std::chrono::steady_clock::now();
		if (!_cacheValid || a_forceRefresh || now - _updated > std::chrono::seconds(5)) {
			_cacheValid = false;
			if (a_forceRefresh)
				_failedDeletions.clear();
			std::vector<CaptureFile> files;
			for (const auto& entry : std::filesystem::directory_iterator(_directory)) {
				if (!IsCaptureFile(entry))
					continue;
				CaptureFile file{ entry.path(), entry.file_size(), entry.last_write_time(), {} };
				if (const auto failed = _failedDeletions.find(file.path); failed != _failedDeletions.end())
					file.deletionError = failed->second;
				files.push_back(std::move(file));
			}
			std::ranges::sort(files, std::greater{}, &CaptureFile::modified);
			_files = std::move(files);
			_updated = now;
			_cacheValid = true;
		}
		return _files;
	}

	std::uint64_t CaptureService::DiskUsageBytes() const
	{
		std::uint64_t bytes{};
		for (const auto& entry : std::filesystem::directory_iterator(_directory)) {
			if (IsCaptureFile(entry))
				bytes += entry.file_size();
		}
		return bytes;
	}

	void CaptureService::ClearCaptures()
	{
		// FO4: deletion failures reach the existing host dialog while remaining in the inventory.
		_failedDeletions.clear();
		_cacheValid = false;
		for (const auto& entry : std::filesystem::directory_iterator(_directory)) {
			if (!IsCaptureFile(entry))
				continue;
			std::error_code error;
			std::filesystem::remove(entry.path(), error);
			if (error)
				_failedDeletions.emplace(entry.path(), error.message());
		}
		if (!_failedDeletions.empty())
			throw std::runtime_error("Some capture files could not be deleted; see Capture Files for details");
	}

	std::uint32_t CaptureService::Count() const
	{
		return _api ? _api->GetNumCaptures() : 0;
	}

	std::string CaptureService::PathUtf8(std::uint32_t a_index) const
	{
		std::uint32_t length{};
		if (!_api || !_api->GetCapture(a_index, nullptr, &length, nullptr) || !length)
			return {};
		std::string path(static_cast<std::size_t>(length), '\0');
		if (!_api->GetCapture(a_index, path.data(), &length, nullptr))
			return {};
		path.resize(std::char_traits<char>::length(path.c_str()));
		return path;
	}
}
