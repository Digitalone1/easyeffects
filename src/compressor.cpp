/*
 *  Copyright © 2017-2022 Wellington Wallace
 *
 *  This file is part of EasyEffects.
 *
 *  EasyEffects is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  EasyEffects is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with EasyEffects.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "compressor.hpp"

Compressor::Compressor(const std::string& tag,
                       const std::string& schema,
                       const std::string& schema_path,
                       PipeManager* pipe_manager)
    : PluginBase(tag, plugin_name::compressor, schema, schema_path, pipe_manager, true),
      lv2_wrapper(std::make_unique<lv2::Lv2Wrapper>("http://lsp-plug.in/plugins/lv2/sc_compressor_stereo")) {
  if (!lv2_wrapper->found_plugin) {
    util::debug(log_tag + "http://lsp-plug.in/plugins/lv2/sc_compressor_stereo is not installed");
  }

  settings->signal_changed("sidechain-type").connect([=, this](const auto& key) {
    if (settings->get_string(key) == "External") {
      const auto* device_name = settings->get_string("sidechain-input-device").c_str();

      NodeInfo input_device = pm->ee_source_node;

      for (const auto& [id, node] : pm->node_map) {
        if (node.name == device_name) {
          input_device = node;

          break;
        }
      }

      for (const auto& link : pm->link_nodes(input_device.id, get_node_id(), true)) {
        list_proxies.emplace_back(link);
      }
    } else {
      pm->destroy_links(list_proxies);

      list_proxies.clear();
    }
  });

  settings->signal_changed("sidechain-input-device").connect([=, this](const auto& key) {
    if (settings->get_string("sidechain-type") == "External") {
      const auto* device_name = settings->get_string(key).c_str();

      NodeInfo input_device = pm->ee_source_node;

      for (const auto& [id, node] : pm->node_map) {
        if (node.name == device_name) {
          input_device = node;

          break;
        }
      }

      pm->destroy_links(list_proxies);

      list_proxies.clear();

      for (const auto& link : pm->link_nodes(input_device.id, get_node_id(), true)) {
        list_proxies.emplace_back(link);
      }
    }
  });

  lv2_wrapper->bind_key_enum(settings, "mode", "cm");

  lv2_wrapper->bind_key_enum(settings, "sidechain-type", "sct");

  lv2_wrapper->bind_key_enum(settings, "sidechain-mode", "scm");

  lv2_wrapper->bind_key_enum(settings, "sidechain-source", "scs");

  lv2_wrapper->bind_key_enum(settings, "hpf-mode", "shpm");

  lv2_wrapper->bind_key_enum(settings, "lpf-mode", "slpm");

  lv2_wrapper->bind_key_bool(settings, "sidechain-listen", "scl");

  lv2_wrapper->bind_key_double(settings, "attack", "at");

  lv2_wrapper->bind_key_double(settings, "release", "rt");

  lv2_wrapper->bind_key_double(settings, "ratio", "cr");

  lv2_wrapper->bind_key_double(settings, "sidechain-reactivity", "scr");

  lv2_wrapper->bind_key_double(settings, "sidechain-lookahead", "sla");

  lv2_wrapper->bind_key_double(settings, "hpf-frequency", "shpf");

  lv2_wrapper->bind_key_double(settings, "lpf-frequency", "slpf");

  lv2_wrapper->bind_key_double_db(settings, "release-threshold", "rrl");

  lv2_wrapper->bind_key_double_db(settings, "boost-threshold", "bth");

  lv2_wrapper->bind_key_double_db(settings, "boost-amount", "bsa");

  lv2_wrapper->bind_key_double_db(settings, "threshold", "al");

  lv2_wrapper->bind_key_double_db(settings, "knee", "kn");

  lv2_wrapper->bind_key_double_db(settings, "makeup", "mk");

  lv2_wrapper->bind_key_double_db(settings, "sidechain-preamp", "scp");

  setup_input_output_gain();
}

Compressor::~Compressor() {
  if (connected_to_pw) {
    disconnect_from_pw();
  }

  util::debug(log_tag + name + " destroyed");
}

void Compressor::setup() {
  if (!lv2_wrapper->found_plugin) {
    return;
  }

  lv2_wrapper->set_n_samples(n_samples);
  lv2_wrapper->create_instance(rate);
}

void Compressor::process(std::span<float>& left_in,
                         std::span<float>& right_in,
                         std::span<float>& left_out,
                         std::span<float>& right_out,
                         std::span<float>& probe_left,
                         std::span<float>& probe_right) {
  if (!lv2_wrapper->found_plugin || !lv2_wrapper->has_instance() || bypass) {
    std::copy(left_in.begin(), left_in.end(), left_out.begin());
    std::copy(right_in.begin(), right_in.end(), right_out.begin());

    return;
  }

  if (input_gain != 1.0F) {
    apply_gain(left_in, right_in, input_gain);
  }

  lv2_wrapper->connect_data_ports(left_in, right_in, left_out, right_out, probe_left, probe_right);
  lv2_wrapper->run();

  if (output_gain != 1.0F) {
    apply_gain(left_out, right_out, output_gain);
  }

  /*
   This plugin gives the latency in number of samples
 */

  const auto& lv = static_cast<uint>(lv2_wrapper->get_control_port_value("out_latency"));

  if (latency_n_frames != lv) {
    latency_n_frames = lv;

    latency_port_value = static_cast<float>(latency_n_frames) / static_cast<float>(rate);

    util::debug(log_tag + name + " latency: " + std::to_string(latency_port_value) + " s");

    Glib::signal_idle().connect_once([=, this] { latency.emit(latency_port_value); });

    spa_process_latency_info latency_info{};

    latency_info.ns = static_cast<uint64_t>(latency_port_value * 1000000000.0F);

    std::array<char, 1024U> buffer{};

    spa_pod_builder b{};

    spa_pod_builder_init(&b, buffer.data(), sizeof(buffer));

    const spa_pod* param = spa_process_latency_build(&b, SPA_PARAM_ProcessLatency, &latency_info);

    pw_filter_update_params(filter, nullptr, &param, 1);
  }

  if (post_messages) {
    get_peaks(left_in, right_in, left_out, right_out);

    notification_dt += sample_duration;

    if (notification_dt >= notification_time_window) {
      reduction_port_value = lv2_wrapper->get_control_port_value("rlm");
      sidechain_port_value = lv2_wrapper->get_control_port_value("slm");
      curve_port_value = lv2_wrapper->get_control_port_value("clm");
      envelope_port_value = lv2_wrapper->get_control_port_value("elm");

      Glib::signal_idle().connect_once([=, this] {
        reduction.emit(reduction_port_value);
        sidechain.emit(sidechain_port_value);
        curve.emit(curve_port_value);
        envelope.emit(envelope_port_value);
      });

      notify();

      notification_dt = 0.0F;
    }
  }
}
