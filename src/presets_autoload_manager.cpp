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

#include "presets_autoload_manager.hpp"
#include <qcontainerfwd.h>
#include <qfilesystemwatcher.h>
#include <qtmetamacros.h>
#include <qtypes.h>
#include <exception>
#include <filesystem>
#include <format>
#include <fstream>
#include <iomanip>
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>
#include <string>
#include <vector>
#include "easyeffects_db.h"
#include "pipeline_type.hpp"
#include "presets_directory_manager.hpp"
#include "presets_list_model.hpp"
#include "util.hpp"

namespace presets {

AutoloadManager::AutoloadManager(DirectoryManager& directory_manager)
    : dir_manager(directory_manager),
      input_model(new ListModel(this, ListModel::ModelType::Autoloading)),
      output_model(new ListModel(this, ListModel::ModelType::Autoloading)) {
  refreshListModels();
  prepareFilesystemWatchers();
}

auto AutoloadManager::get_input_model() -> ListModel* {
  return input_model;
}

auto AutoloadManager::get_output_model() -> ListModel* {
  return output_model;
}

void AutoloadManager::prepareFilesystemWatchers() {
  input_watcher.addPath(QString::fromStdString(dir_manager.autoloadInputDir().string()));
  output_watcher.addPath(QString::fromStdString(dir_manager.autoloadOutputDir().string()));

  connect(&input_watcher, &QFileSystemWatcher::directoryChanged,
          [&]() { input_model->update(dir_manager.getAutoloadingProfilesPaths(PipelineType::input)); });

  connect(&output_watcher, &QFileSystemWatcher::directoryChanged,
          [&]() { output_model->update(dir_manager.getAutoloadingProfilesPaths(PipelineType::output)); });
}

void AutoloadManager::refreshListModels() {
  input_model->update(dir_manager.getAutoloadingProfilesPaths(PipelineType::input));
  output_model->update(dir_manager.getAutoloadingProfilesPaths(PipelineType::output));
}

auto AutoloadManager::getFilePath(const PipelineType& pipeline_type,
                                  const QString& device_name,
                                  const QString& device_profile) -> std::filesystem::path {
  switch (pipeline_type) {
    case PipelineType::output:
      return dir_manager.autoloadOutputDir() /
             std::filesystem::path{device_name.toStdString() + ":" + device_profile.toStdString() +
                                   DirectoryManager::json_ext};
    case PipelineType::input:
      return dir_manager.autoloadInputDir() /
             std::filesystem::path{device_name.toStdString() + ":" + device_profile.toStdString() +
                                   DirectoryManager::json_ext};
  }

  return {};
}

void AutoloadManager::add(const PipelineType& pipeline_type,
                          const QString& preset_name,
                          const QString& device_name,
                          const QString& device_description,
                          const QString& device_profile) {
  nlohmann::json json;

  auto path = getFilePath(pipeline_type, device_name, device_profile);

  bool already_exists = std::filesystem::exists(path);

  std::ofstream o(path);

  json["device"] = device_name.toStdString();
  json["device-description"] = device_description.toStdString();
  json["device-profile"] = device_profile.toStdString();
  json["preset-name"] = preset_name.toStdString();

  o << std::setw(4) << json << '\n';

  util::debug(std::format("Added autoload preset file: {}", path.string()));

  o.close();

  if (already_exists) {
    auto* model = pipeline_type == PipelineType::input ? input_model : output_model;

    model->emit_data_changed(path);
  }
}

void AutoloadManager::remove(const PipelineType& pipeline_type,
                             const QString& preset_name,
                             const QString& device_name,
                             const QString& device_profile) {
  auto path = getFilePath(pipeline_type, device_name, device_profile);

  if (!std::filesystem::is_regular_file(path)) {
    return;
  }

  nlohmann::json json;

  std::ifstream is(path);

  is >> json;

  if (preset_name.toStdString() == json.value("preset-name", "") &&
      device_profile.toStdString() == json.value("device-profile", "")) {
    std::filesystem::remove(path);

    util::debug(std::format("Removed autoload: {}", path.string()));
  }
}

auto AutoloadManager::find(const PipelineType& pipeline_type, const QString& device_name, const QString& device_profile)
    -> std::string {
  auto path = getFilePath(pipeline_type, device_name, device_profile);

  if (!std::filesystem::is_regular_file(path)) {
    return "";
  }

  nlohmann::json json;

  std::ifstream is(path);

  is >> json;

  return json.value("preset-name", "");
}

void AutoloadManager::load(const PipelineType& pipeline_type,
                           const QString& device_name,
                           const QString& device_profile) {
  const auto name = find(pipeline_type, device_name, device_profile);

  if (name.empty()) {
    QString fallback;

    switch (pipeline_type) {
      case PipelineType::input: {
        fallback = db::Main::inputAutoloadingUsesFallback() ? db::Main::inputAutoloadingFallbackPreset() : "";
        break;
      }
      case PipelineType::output: {
        fallback = db::Main::outputAutoloadingUsesFallback() ? db::Main::outputAutoloadingFallbackPreset() : "";
        break;
      }
    }

    if (!fallback.isEmpty()) {
      util::debug(std::format("Autoloading fallback preset {} for device {}", name, device_name.toStdString()));

      Q_EMIT loadFallbackPresetRequested(pipeline_type, fallback);

      return;
    }

    return;
  }

  util::debug(std::format("Autoloading local preset {} for device {}", name, device_name.toStdString()));

  Q_EMIT loadPresetRequested(pipeline_type, QString::fromStdString(name));
}

auto AutoloadManager::getProfiles(const PipelineType& pipeline_type) -> std::vector<nlohmann::json> {
  std::filesystem::path autoload_dir;
  std::vector<nlohmann::json> list;

  switch (pipeline_type) {
    case PipelineType::output:
      autoload_dir = dir_manager.autoloadOutputDir();
      break;
    case PipelineType::input:
      autoload_dir = dir_manager.autoloadInputDir();
      break;
  }

  auto it = std::filesystem::directory_iterator{autoload_dir};

  try {
    while (it != std::filesystem::directory_iterator{}) {
      if (std::filesystem::is_regular_file(it->status())) {
        if (it->path().extension().string() == DirectoryManager::json_ext) {
          nlohmann::json json;

          std::ifstream is(autoload_dir / it->path());

          is >> json;

          list.push_back(json);
        }
      }

      ++it;
    }

    return list;
  } catch (const std::exception& e) {
    util::warning(e.what());

    return list;
  }
}

void AutoloadManager::addAutoload(const PipelineType& pipelineType,
                                  const QString& presetName,
                                  const QString& deviceName,
                                  const QString& deviceDescription,
                                  const QString& deviceProfile) {
  add(pipelineType, presetName, deviceName, deviceDescription, deviceProfile);
}

void AutoloadManager::removeAutoload(const PipelineType& pipelineType,
                                     const QString& presetName,
                                     const QString& deviceName,
                                     const QString& deviceProfile) {
  remove(pipelineType, presetName, deviceName, deviceProfile);
}

}  // namespace presets
