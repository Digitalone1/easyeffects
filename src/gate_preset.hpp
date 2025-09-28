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

#pragma once

#include <nlohmann/json_fwd.hpp>
#include <string>
#include "easyeffects_db_gate.h"
#include "pipeline_type.hpp"
#include "plugin_preset_base.hpp"

class GatePreset : public PluginPresetBase {
 public:
  explicit GatePreset(PipelineType pipeline_type, const std::string& instance_name);

 private:
  db::Gate* settings = nullptr;

  void save(nlohmann::json& json) override;

  void load(const nlohmann::json& json) override;
};
