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

#include "stream_input_effects.hpp"

StreamInputEffects::StreamInputEffects(PipeManager* pipe_manager)
    : EffectsBase("sie: ", "com.github.wwmm.easyeffects.streaminputs", pipe_manager) {
  pm->input_device = pm->default_input_device;

  if (settings->get_boolean("use-default-input-device")) {
    settings->set_string("input-device", pm->input_device.name);
  } else {
    auto found = false;

    const auto* input_device = settings->get_string("input-device").c_str();

    for (const auto& [id, node] : pm->node_map) {
      if (node.name == input_device) {
        pm->input_device = node;

        found = true;

        break;
      }
    }

    if (!found) {
      settings->set_string("input-device", pm->input_device.name);
    }
  }

  auto* PULSE_SOURCE = std::getenv("PULSE_SOURCE");

  if (PULSE_SOURCE != nullptr) {
    for (const auto& [id, node] : pm->node_map) {
      if (node.name == PULSE_SOURCE) {
        pm->input_device = node;

        settings->set_string("input-device", pm->input_device.name);

        break;
      }
    }
  }

  pm->stream_input_added.connect(sigc::mem_fun(*this, &StreamInputEffects::on_app_added));
  pm->link_changed.connect(sigc::mem_fun(*this, &StreamInputEffects::on_link_changed));

  connect_filters();

  settings->signal_changed("input-device").connect([&, this](const auto& key) {
    const auto name = settings->get_string(key).raw();

    if (name.empty()) {
      return;
    }

    for (const auto& [id, node] : pm->node_map) {
      if (node.name == name) {
        pm->input_device = node;

        disconnect_filters();

        connect_filters();

        break;
      }
    }
  });

  settings->signal_changed("plugins").connect([&, this](const auto& key) {
    if (global_settings->get_boolean("bypass")) {
      global_settings->set_boolean("bypass", false);

      return;  // filter connected through update_bypass_state
    }

    disconnect_filters();

    connect_filters();
  });
}

StreamInputEffects::~StreamInputEffects() {
  util::debug(log_tag + "destroyed");

  disconnect_filters();
}

void StreamInputEffects::on_app_added(const uint id, const std::string name) {
  const auto& blocklist = settings->get_string_array("blocklist");

  const auto& is_blocklisted = std::ranges::find(blocklist, name.c_str()) != blocklist.end();
  ;

  if (is_blocklisted) {
    pm->disconnect_stream_input(id);
  } else if (global_settings->get_boolean("process-all-inputs")) {
    pm->connect_stream_input(id);
  }
}

void StreamInputEffects::on_link_changed(LinkInfo link_info) {
  if (pm->default_input_device.id == pm->ee_source_node.id) {
    return;
  }

  /*
    If bypass is enabled do not touch the plugin pipeline
  */

  if (bypass) {
    return;
  }

  auto want_to_play = false;

  for (const auto& link : pm->list_links) {
    if (link.output_node_id == pm->ee_source_node.id) {
      if (link.state == PW_LINK_STATE_ACTIVE) {
        want_to_play = true;

        break;
      }
    }
  }

  if (!want_to_play) {
    disconnect_filters();

    return;
  }

  if (list_proxies.empty()) {
    connect_filters();
  }
}

void StreamInputEffects::connect_filters(const bool& bypass) {
  if (pm->input_device.id == SPA_ID_INVALID) {
    util::debug(log_tag + "Input device id is invalid. Aborting the link between filters in the microphone pipeline");

    return;
  }

  const auto& list = (bypass) ? std::vector<Glib::ustring>() : settings->get_string_array("plugins");

  auto mic_linked = false;

  uint prev_node_id = pm->input_device.id;
  uint next_node_id = 0U;

  // link plugins

  if (!list.empty()) {
    for (const auto& name : list) {
      if ((!plugins[name]->connected_to_pw) ? plugins[name]->connect_to_pw() : true) {
        next_node_id = plugins[name]->get_node_id();

        const auto& links = pm->link_nodes(prev_node_id, next_node_id);

        const auto& link_size = links.size();

        for (size_t n = 0U; n < link_size; n++) {
          list_proxies.emplace_back(links[n]);
        }

        if (mic_linked && (link_size == 2U)) {
          prev_node_id = next_node_id;
        } else if (!mic_linked && (link_size > 0U)) {
          prev_node_id = next_node_id;
          mic_linked = true;
        } else {
          util::warning(log_tag + " link from node " + std::to_string(prev_node_id) + " to node " +
                        std::to_string(next_node_id) + " failed");
        }
      }
    }

    // checking if we have to link the echo_canceller probe to the output device

    for (const auto& name : list) {
      if (name == plugin_name::echo_canceller) {
        if (plugins[name]->connected_to_pw) {
          for (const auto& link : pm->link_nodes(pm->output_device.id, plugins[name]->get_node_id(), true)) {
            list_proxies.emplace_back(link);
          }
        }

        break;
      }
    }
  }

  // link spectrum, output level meter and source node

  for (const auto& node_id : {spectrum->get_node_id(), output_level->get_node_id(), pm->ee_source_node.id}) {
    next_node_id = node_id;

    const auto& links = pm->link_nodes(prev_node_id, next_node_id);

    const auto& link_size = links.size();

    for (size_t n = 0U; n < link_size; n++) {
      list_proxies.emplace_back(links[n]);
    }

    if (mic_linked && (link_size == 2U)) {
      prev_node_id = next_node_id;
    } else if (!mic_linked && (link_size > 0U)) {
      prev_node_id = next_node_id;
      mic_linked = true;
    } else {
      util::warning(log_tag + " link from node " + std::to_string(prev_node_id) + " to node " +
                    std::to_string(next_node_id) + " failed");
    }
  }
}

void StreamInputEffects::disconnect_filters() {
  std::set<uint> list;

  for (const auto& plugin : plugins | std::views::values) {
    for (const auto& link : pm->list_links) {
      if (link.input_node_id == plugin->get_node_id() || link.output_node_id == plugin->get_node_id()) {
        list.insert(link.id);
      }
    }
  }

  for (const auto& link : pm->list_links) {
    if (link.input_node_id == spectrum->get_node_id() || link.output_node_id == spectrum->get_node_id() ||
        link.input_node_id == output_level->get_node_id() || link.output_node_id == output_level->get_node_id()) {
      list.insert(link.id);
    }
  }

  for (const auto& id : list) {
    pm->destroy_object(static_cast<int>(id));
  }

  pm->destroy_links(list_proxies);

  list_proxies.clear();
}

void StreamInputEffects::set_bypass(const bool& state) {
  bypass = state;

  disconnect_filters();

  connect_filters(state);
}

void StreamInputEffects::set_listen_to_mic(const bool& state) {
  if (state) {
    for (const auto& link : pm->link_nodes(pm->ee_source_node.id, pm->output_device.id, false, false)) {
      list_proxies_listen_mic.emplace_back(link);
    }
  } else {
    pm->destroy_links(list_proxies_listen_mic);

    list_proxies_listen_mic.clear();
  }
}
