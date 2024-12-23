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

#include "exciter.hpp"
#include <algorithm>
#include <memory>
#include "db_manager.hpp"
#include "lv2_macros.hpp"
#include "lv2_wrapper.hpp"
#include "tags_plugin_name.hpp"
#include "util.hpp"

Exciter::Exciter(const std::string& tag, pw::Manager* pipe_manager, PipelineType pipe_type, QString instance_id)
    : PluginBase(tag,
                 tags::plugin_name::BaseName::exciter,
                 tags::plugin_package::Package::calf,
                 instance_id,
                 pipe_manager,
                 pipe_type),
      settings(
          db::Manager::self().get_plugin_db<db::Exciter>(pipe_type,
                                                         tags::plugin_name::BaseName::exciter + "#" + instance_id)) {
  const auto lv2_plugin_uri = "http://calf.sourceforge.net/plugins/Exciter";

  lv2_wrapper = std::make_unique<lv2::Lv2Wrapper>(lv2_plugin_uri);

  package_installed = lv2_wrapper->found_plugin;

  if (!package_installed) {
    util::debug(log_tag + lv2_plugin_uri + " is not installed");
  }

  init_common_controls<db::Exciter>(settings);

  // specific plugin controls

  BIND_LV2_PORT("listen", listen, setListen, db::Exciter::listenChanged);
  BIND_LV2_PORT_DB("amount", amount, setAmount, db::Exciter::amountChanged, false);
  BIND_LV2_PORT("drive", harmonics, setHarmonics, db::Exciter::harmonicsChanged);
  BIND_LV2_PORT("freq", scope, setScope, db::Exciter::scopeChanged);
  BIND_LV2_PORT("ceil", ceil, setCeil, db::Exciter::ceilChanged);
  BIND_LV2_PORT("blend", blend, setBlend, db::Exciter::blendChanged);
  BIND_LV2_PORT("ceil_active", ceilActive, setCeilActive, db::Exciter::ceilActiveChanged);
}

Exciter::~Exciter() {
  if (connected_to_pw) {
    disconnect_from_pw();
  }

  util::debug(log_tag + name.toStdString() + " destroyed");
}

void Exciter::reset() {
  settings->setDefaults();
}

void Exciter::setup() {
  if (!lv2_wrapper->found_plugin) {
    return;
  }

  lv2_wrapper->set_n_samples(n_samples);

  if (lv2_wrapper->get_rate() != rate) {
    lv2_wrapper->create_instance(rate);
  }
}

void Exciter::process(std::span<float>& left_in,
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

  get_peaks(left_in, right_in, left_out, right_out);

  harmonics_port_value = util::linear_to_db(lv2_wrapper->get_control_port_value("meter_drive"));
}

void Exciter::process([[maybe_unused]] std::span<float>& left_in,
                      [[maybe_unused]] std::span<float>& right_in,
                      [[maybe_unused]] std::span<float>& left_out,
                      [[maybe_unused]] std::span<float>& right_out,
                      [[maybe_unused]] std::span<float>& probe_left,
                      [[maybe_unused]] std::span<float>& probe_right) {}

auto Exciter::get_latency_seconds() -> float {
  return 0.0F;
}

float Exciter::getHarmonicsLevel() const {
  return harmonics_port_value;
}
