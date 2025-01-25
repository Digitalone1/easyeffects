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

#include "compressor_preset.hpp"
#include <nlohmann/json_fwd.hpp>
#include <string>
#include "easyeffects_db_compressor.h"
#include "pipeline_type.hpp"
#include "plugin_preset_base.hpp"
#include "presets_macros.hpp"

CompressorPreset::CompressorPreset(PipelineType pipeline_type, const std::string& instance_name)
    : PluginPresetBase(pipeline_type, instance_name) {
  settings = get_db_instance<db::Compressor>(pipeline_type);
}

void CompressorPreset::save(nlohmann::json& json) {
  json[section][instance_name]["bypass"] = settings->bypass();

  json[section][instance_name]["input-gain"] = settings->inputGain();

  json[section][instance_name]["output-gain"] = settings->outputGain();

  json[section][instance_name]["dry"] = settings->dry();

  json[section][instance_name]["wet"] = settings->wet();

  json[section][instance_name]["mode"] = settings->defaultModeLabelsValue()[settings->mode()].toStdString();

  json[section][instance_name]["attack"] = settings->attack();

  json[section][instance_name]["release"] = settings->release();

  json[section][instance_name]["release-threshold"] = settings->releaseThreshold();

  json[section][instance_name]["threshold"] = settings->threshold();

  json[section][instance_name]["ratio"] = settings->ratio();

  json[section][instance_name]["knee"] = settings->knee();

  json[section][instance_name]["makeup"] = settings->makeup();

  json[section][instance_name]["boost-threshold"] = settings->boostThreshold();

  json[section][instance_name]["boost-amount"] = settings->boostAmount();

  json[section][instance_name]["stereo-split"] = settings->stereoSplit();

  json[section][instance_name]["sidechain"]["type"] =
      settings->defaultSidechainTypeLabelsValue()[settings->sidechainType()].toStdString();

  json[section][instance_name]["sidechain"]["mode"] =
      settings->defaultSidechainModeLabelsValue()[settings->sidechainMode()].toStdString();

  json[section][instance_name]["sidechain"]["source"] =
      settings->defaultSidechainSourceLabelsValue()[settings->sidechainSource()].toStdString();

  json[section][instance_name]["sidechain"]["stereo-split-source"] =
      settings->defaultStereoSplitSourceLabelsValue()[settings->stereoSplitSource()].toStdString();

  json[section][instance_name]["sidechain"]["preamp"] = settings->sidechainPreamp();

  json[section][instance_name]["sidechain"]["reactivity"] = settings->sidechainReactivity();

  json[section][instance_name]["sidechain"]["lookahead"] = settings->sidechainLookahead();

  json[section][instance_name]["hpf-mode"] = settings->defaultHpfModeLabelsValue()[settings->hpfMode()].toStdString();

  json[section][instance_name]["hpf-frequency"] = settings->hpfFrequency();

  json[section][instance_name]["lpf-mode"] = settings->defaultLpfModeLabelsValue()[settings->lpfMode()].toStdString();

  json[section][instance_name]["lpf-frequency"] = settings->lpfFrequency();
}

void CompressorPreset::load(const nlohmann::json& json) {
  UPDATE_PROPERTY("bypass", Bypass);
  UPDATE_PROPERTY("input-gain", InputGain);
  UPDATE_PROPERTY("output-gain", OutputGain);
  UPDATE_PROPERTY("dry", Dry);
  UPDATE_PROPERTY("wet", Wet);
  UPDATE_PROPERTY("attack", Attack);
  UPDATE_PROPERTY("release", Release);
  UPDATE_PROPERTY("release-threshold", ReleaseThreshold);
  UPDATE_PROPERTY("threshold", Threshold);
  UPDATE_PROPERTY("ratio", Ratio);
  UPDATE_PROPERTY("knee", Knee);
  UPDATE_PROPERTY("makeup", Makeup);
  UPDATE_PROPERTY("boost-threshold", BoostThreshold);
  UPDATE_PROPERTY("boost-amount", BoostAmount);
  UPDATE_PROPERTY("stereo-split", StereoSplit);
  UPDATE_PROPERTY("makeup", Makeup);
  UPDATE_PROPERTY("hpf-frequency", HpfFrequency);
  UPDATE_PROPERTY("lpf-frequency", LpfFrequency);

  UPDATE_ENUM_LIKE_PROPERTY("mode", Mode);
  UPDATE_ENUM_LIKE_PROPERTY("hpf-mode", HpfMode);
  UPDATE_ENUM_LIKE_PROPERTY("lpf-mode", LpfMode);

  UPDATE_PROPERTY_INSIDE_SUBSECTION("sidechain", "preamp", SidechainPreamp);
  UPDATE_PROPERTY_INSIDE_SUBSECTION("sidechain", "reactivity", SidechainReactivity);
  UPDATE_PROPERTY_INSIDE_SUBSECTION("sidechain", "lookahead", SidechainLookahead);

  UPDATE_ENUM_LIKE_PROPERTY_INSIDE_SUBSECTION("sidechain", "type", SidechainType);
  UPDATE_ENUM_LIKE_PROPERTY_INSIDE_SUBSECTION("sidechain", "mode", SidechainMode);
  UPDATE_ENUM_LIKE_PROPERTY_INSIDE_SUBSECTION("sidechain", "source", SidechainSource);
  UPDATE_ENUM_LIKE_PROPERTY_INSIDE_SUBSECTION("sidechain", "stereo-split-source", StereoSplitSource);
}
