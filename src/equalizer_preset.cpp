/*
 *  Copyright © 2017-2025 Wellington Wallace
 *
 *  This file is part of Easy Effects.
 *
 *  Easy Effects is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  Easy Effects is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Easy Effects. If not, see <https://www.gnu.org/licenses/>.
 */

#include "equalizer_preset.hpp"
#include <strings.h>
#include <QString>
#include <nlohmann/json_fwd.hpp>
#include <string>
#include "db_manager.hpp"
#include "easyeffects_db_equalizer.h"
#include "easyeffects_db_equalizer_channel.h"
#include "pipeline_type.hpp"
#include "plugin_preset_base.hpp"
#include "presets_macros.hpp"
#include "tags_equalizer.hpp"

using namespace tags::equalizer;

EqualizerPreset::EqualizerPreset(PipelineType pipeline_type, const std::string& instance_name)
    : PluginPresetBase(pipeline_type, instance_name) {
  settings = get_db_instance<db::Equalizer>(pipeline_type);

  if (settings != nullptr) {
    input_settings_left = db::Manager::self().get_plugin_db<db::EqualizerChannel>(
        pipeline_type, QString::fromStdString(instance_name + "#left"));
    input_settings_right = db::Manager::self().get_plugin_db<db::EqualizerChannel>(
        pipeline_type, QString::fromStdString(instance_name + "#right"));

    output_settings_left = db::Manager::self().get_plugin_db<db::EqualizerChannel>(
        pipeline_type, QString::fromStdString(instance_name + "#left"));
    output_settings_right = db::Manager::self().get_plugin_db<db::EqualizerChannel>(
        pipeline_type, QString::fromStdString(instance_name + "#right"));
  }
}

void EqualizerPreset::save(nlohmann::json& json) {
  // json[section][instance_name]["bypass"] = g_settings_get_boolean(settings, "bypass") != 0;

  // json[section][instance_name]["input-gain"] = g_settings_get_double(settings, "input-gain");

  // json[section][instance_name]["output-gain"] = g_settings_get_double(settings, "output-gain");

  // json[section][instance_name]["mode"] = util::gsettings_get_string(settings, "mode");

  // json[section][instance_name]["split-channels"] = g_settings_get_boolean(settings, "split-channels") != 0;

  // json[section][instance_name]["balance"] = g_settings_get_double(settings, "balance");

  // json[section][instance_name]["pitch-left"] = g_settings_get_double(settings, "pitch-left");

  // json[section][instance_name]["pitch-right"] = g_settings_get_double(settings, "pitch-right");

  const auto nbands = settings->numBands();

  json[section][instance_name]["num-bands"] = nbands;

  if (section == "input") {
    save_channel(json[section][instance_name]["left"], input_settings_left, nbands);
    save_channel(json[section][instance_name]["right"], input_settings_right, nbands);
  } else if (section == "output") {
    save_channel(json[section][instance_name]["left"], output_settings_left, nbands);
    save_channel(json[section][instance_name]["right"], output_settings_right, nbands);
  }
}

void EqualizerPreset::save_channel(nlohmann::json& json, db::EqualizerChannel* settings, const int& nbands) {
  // for (int n = 0; n < nbands; n++) {
  //   const auto* const bandn = band_id[n];

  //   json[bandn]["type"] = util::gsettings_get_string(settings, band_type[n].data());

  //   json[bandn]["mode"] = util::gsettings_get_string(settings, band_mode[n].data());

  //   json[bandn]["slope"] = util::gsettings_get_string(settings, band_slope[n].data());

  //   json[bandn]["solo"] = g_settings_get_boolean(settings, band_solo[n].data()) != 0;

  //   json[bandn]["mute"] = g_settings_get_boolean(settings, band_mute[n].data()) != 0;

  //   json[bandn]["gain"] = g_settings_get_double(settings, band_gain[n].data());

  //   json[bandn]["frequency"] = g_settings_get_double(settings, band_frequency[n].data());

  //   json[bandn]["q"] = g_settings_get_double(settings, band_q[n].data());

  //   json[bandn]["width"] = g_settings_get_double(settings, band_width[n].data());
  // }
}

void EqualizerPreset::load(const nlohmann::json& json) {
  // update_key<bool>(json.at(section).at(instance_name), settings, "bypass", "bypass");

  // update_key<double>(json.at(section).at(instance_name), settings, "input-gain", "input-gain");

  // update_key<double>(json.at(section).at(instance_name), settings, "output-gain", "output-gain");

  // update_key<gchar*>(json.at(section).at(instance_name), settings, "mode", "mode");

  // update_key<int>(json.at(section).at(instance_name), settings, "num-bands", "num-bands");

  // update_key<bool>(json.at(section).at(instance_name), settings, "split-channels", "split-channels");

  // update_key<double>(json.at(section).at(instance_name), settings, "balance", "balance");

  // update_key<double>(json.at(section).at(instance_name), settings, "pitch-left", "pitch-left");

  // update_key<double>(json.at(section).at(instance_name), settings, "pitch-right", "pitch-right");

  const auto nbands = settings->numBands();

  if (section == "input") {
    load_channel(json.at(section).at(instance_name).at("left"), input_settings_left, nbands);
    load_channel(json.at(section).at(instance_name).at("right"), input_settings_right, nbands);
  } else if (section == "output") {
    load_channel(json.at(section).at(instance_name).at("left"), output_settings_left, nbands);
    load_channel(json.at(section).at(instance_name).at("right"), output_settings_right, nbands);
  }
}

void EqualizerPreset::load_channel(const nlohmann::json& json, db::EqualizerChannel* settings, const int& nbands) {
  // for (int n = 0; n < nbands; n++) {
  //   const auto bandn = "band" + util::to_string(n);

  //   update_key<gchar*>(json.at(bandn), settings, band_type[n].data(), "type");

  //   update_key<gchar*>(json.at(bandn), settings, band_mode[n].data(), "mode");

  //   update_key<gchar*>(json.at(bandn), settings, band_slope[n].data(), "slope");

  //   update_key<bool>(json.at(bandn), settings, band_solo[n].data(), "solo");

  //   update_key<bool>(json.at(bandn), settings, band_mute[n].data(), "mute");

  //   update_key<double>(json.at(bandn), settings, band_gain[n].data(), "gain");

  //   update_key<double>(json.at(bandn), settings, band_frequency[n].data(), "frequency");

  //   update_key<double>(json.at(bandn), settings, band_q[n].data(), "q");

  //   update_key<double>(json.at(bandn), settings, band_width[n].data(), "width");
  // }
}
