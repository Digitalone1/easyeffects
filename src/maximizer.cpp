/*
 *  Copyright © 2017-2024 Wellington Wallace
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

#include "maximizer.hpp"
#include <qtypes.h>
#include <algorithm>
#include <memory>
#include <span>
#include <string>
#include "db_manager.hpp"
#include "easyeffects_db_maximizer.h"
#include "lv2_macros.hpp"
#include "lv2_wrapper.hpp"
#include "pipeline_type.hpp"
#include "plugin_base.hpp"
#include "pw_manager.hpp"
#include "tags_plugin_name.hpp"
#include "util.hpp"

Maximizer::Maximizer(const std::string& tag, pw::Manager* pipe_manager, PipelineType pipe_type, QString instance_id)
    : PluginBase(tag,
                 tags::plugin_name::BaseName::maximizer,
                 tags::plugin_package::Package::zam,
                 instance_id,
                 pipe_manager,
                 pipe_type),
      settings(db::Manager::self().get_plugin_db<db::Maximizer>(
          pipe_type,
          tags::plugin_name::BaseName::maximizer + "#" + instance_id)) {
  const auto lv2_plugin_uri = "urn:zamaudio:ZaMaximX2";

  lv2_wrapper = std::make_unique<lv2::Lv2Wrapper>(lv2_plugin_uri);

  package_installed = lv2_wrapper->found_plugin;

  if (!package_installed) {
    util::debug(log_tag + lv2_plugin_uri + " is not installed");
  }

  init_common_controls<db::Maximizer>(settings);

  // specific plugin controls

  BIND_LV2_PORT("rel", release, setRelease, db::Maximizer::releaseChanged);
  BIND_LV2_PORT("thresh", threshold, setThreshold, db::Maximizer::thresholdChanged);
}

Maximizer::~Maximizer() {
  if (connected_to_pw) {
    disconnect_from_pw();
  }

  util::debug(log_tag + name.toStdString() + " destroyed");
}

void Maximizer::reset() {
  settings->setDefaults();
}

void Maximizer::setup() {
  if (!lv2_wrapper->found_plugin) {
    return;
  }

  lv2_wrapper->set_n_samples(n_samples);

  if (lv2_wrapper->get_rate() != rate) {
    lv2_wrapper->create_instance(rate);
  }
}

void Maximizer::process(std::span<float>& left_in,
                        std::span<float>& right_in,
                        std::span<float>& left_out,
                        std::span<float>& right_out) {
  if (!lv2_wrapper->found_plugin || !lv2_wrapper->has_instance() || bypass) {
    std::copy(left_in.begin(), left_in.end(), left_out.begin());
    std::copy(right_in.begin(), right_in.end(), right_out.begin());

    return;
  }

  if (input_gain != 1.0F) {
    apply_gain(left_in, right_in, input_gain);
  }

  lv2_wrapper->connect_data_ports(left_in, right_in, left_out, right_out);
  lv2_wrapper->run();

  if (output_gain != 1.0F) {
    apply_gain(left_out, right_out, output_gain);
  }

  // This plugin gives the latency in number of samples

  const auto lv = static_cast<uint>(lv2_wrapper->get_control_port_value("lv2_latency"));

  if (latency_n_frames != lv) {
    latency_n_frames = lv;

    latency_value = static_cast<float>(latency_n_frames) / static_cast<float>(rate);

    util::debug(log_tag + name.toStdString() + " latency: " + util::to_string(latency_value, "") + " s");

    update_filter_params();
  }

  get_peaks(left_in, right_in, left_out, right_out);

  reduction_port_value = lv2_wrapper->get_control_port_value("gr");
}

void Maximizer::process([[maybe_unused]] std::span<float>& left_in,
                        [[maybe_unused]] std::span<float>& right_in,
                        [[maybe_unused]] std::span<float>& left_out,
                        [[maybe_unused]] std::span<float>& right_out,
                        [[maybe_unused]] std::span<float>& probe_left,
                        [[maybe_unused]] std::span<float>& probe_right) {}

auto Maximizer::get_latency_seconds() -> float {
  return this->latency_value;
}

float Maximizer::getReductionLevel() const {
  return this->reduction_port_value;
}
