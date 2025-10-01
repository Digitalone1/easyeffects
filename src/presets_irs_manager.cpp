/**
 * Copyright © 2017-2025 Wellington Wallace
 *
 * This file is part of Easy Effects.
 *
 * Easy Effects is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Easy Effects is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Easy Effects. If not, see <https://www.gnu.org/licenses/>.
 */

#include "presets_irs_manager.hpp"
#include <qcontainerfwd.h>
#include <qfilesystemwatcher.h>
#include <qtmetamacros.h>
#include <qtypes.h>
#include <qurl.h>
#include <filesystem>
#include <format>
#include <sndfile.hh>
#include <string>
#include "presets_directory_manager.hpp"
#include "presets_list_model.hpp"
#include "util.hpp"

namespace presets {

IrsManager::IrsManager(DirectoryManager& directory_manager)
    : dir_manager(directory_manager), model(new ListModel(this, ListModel::ModelType::IRS)) {
  model->update(dir_manager.getLocalIrsPaths());

  watcher.addPath(QString::fromStdString(dir_manager.userIrsDir().string()));

  connect(&watcher, &QFileSystemWatcher::directoryChanged, [&]() { model->update(dir_manager.getLocalIrsPaths()); });
}

auto IrsManager::get_model() -> ListModel* {
  return model;
}

auto IrsManager::import_irs_file(const std::string& file_path) -> ImportState {
  std::filesystem::path p{file_path};

  if (!std::filesystem::is_regular_file(p)) {
    util::warning(std::format("{} is not a file!", p.string()));

    return ImportState::no_regular_file;
  }

  auto file = SndfileHandle(file_path);

  if (file.frames() == 0) {
    util::warning("Cannot import the impulse response! The format may be corrupted or unsupported.");
    util::warning(std::format("{} loading failed", file_path));

    return ImportState::no_frame;
  }

  if (file.channels() != 2) {
    util::warning("Only stereo impulse files are supported!");
    util::warning(std::format("{} loading failed", file_path));

    return ImportState::no_stereo;
  }

  auto out_path = dir_manager.userIrsDir() / p.filename();

  out_path.replace_extension(DirectoryManager::irs_ext);

  std::filesystem::copy_file(p, out_path, std::filesystem::copy_options::overwrite_existing);

  util::debug(std::format("Irs file successfully imported to: {}", out_path.string()));

  return ImportState::success;
}

int IrsManager::import_impulses(const QList<QString>& url_list) {
  for (const auto& u : url_list) {
    auto url = QUrl(u);

    if (url.isLocalFile()) {
      auto path = std::filesystem::path{url.toLocalFile().toStdString()};

      if (auto import_state = import_irs_file(path); import_state != ImportState::success) {
        return static_cast<int>(import_state);
      }
    }
  }

  return static_cast<int>(ImportState::success);
}

bool IrsManager::remove_impulse_file(const QString& filePath) {
  bool result = false;

  if (std::filesystem::exists(filePath.toStdString())) {
    result = std::filesystem::remove(filePath.toStdString());
  }

  if (result) {
    util::debug(std::format("removed irs file: {}", filePath.toStdString()));
  } else {
    util::warning(std::format("failed to removed the irs file: {}", filePath.toStdString()));
  }

  return result;
}

}  // namespace presets