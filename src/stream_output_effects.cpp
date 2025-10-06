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

#include "stream_output_effects.hpp"
#include <pipewire/link.h>
#include <pipewire/node.h>
#include <qcontainerfwd.h>
#include <qnamespace.h>
#include <qqml.h>
#include <qtimer.h>
#include <qtmetamacros.h>
#include <qtypes.h>
#include <spa/utils/defs.h>
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <format>
#include <ranges>
#include <set>
#include <string>
#include <thread>
#include <vector>
#include "config.h"
#include "db_manager.hpp"
#include "effects_base.hpp"
#include "pipeline_type.hpp"
#include "presets_manager.hpp"
#include "pw_manager.hpp"
#include "pw_objects.hpp"
#include "tags_pipewire.hpp"
#include "tags_plugin_name.hpp"
#include "util.hpp"

StreamOutputEffects::StreamOutputEffects(pw::Manager* pipe_manager) : EffectsBase(pipe_manager, PipelineType::output) {
  // NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDelete)
  qmlRegisterSingletonInstance<StreamOutputEffects>("ee.pipeline", VERSION_MAJOR, VERSION_MINOR, "Output", this);

  connect(
      pm, &pw::Manager::sinkAdded, this,
      [&](pw::NodeInfo node) {
        if (node.name == db::StreamOutputs::outputDevice()) {
          if (db::Main::bypass()) {
            db::Main::setBypass(false);

            return;  // filter connected through update_bypass_state
          }

          set_bypass(false);

          presets::Manager::self().autoload(PipelineType::output, node.name, node.device_profile_name);
        }
      },
      Qt::QueuedConnection);

  connect(
      pm, &pw::Manager::newDefaultSinkName, this,
      [](QString name) {
        if (db::StreamOutputs::useDefaultOutputDevice() || db::StreamOutputs::outputDevice().isEmpty()) {
          db::StreamOutputs::setOutputDevice(name);
        }
      },
      Qt::QueuedConnection);

  connect(
      db::StreamOutputs::self(), &db::StreamOutputs::useDefaultOutputDeviceChanged, this,
      [&]() {
        if (db::StreamOutputs::useDefaultOutputDevice()) {
          db::StreamOutputs::setOutputDevice(pm->defaultOutputDeviceName);
        }
      },
      Qt::QueuedConnection);

  connect(
      db::StreamOutputs::self(), &db::StreamOutputs::outputDeviceChanged, this,
      [&]() {
        const auto name = db::StreamOutputs::outputDevice();

        if (name.isEmpty()) {
          return;
        }

        if (auto node = pm->model_nodes.get_node_by_name(name); node.serial != SPA_ID_INVALID) {
          if (db::Main::bypass()) {
            db::Main::setBypass(false);

            return;  // filter connected through update_bypass_state
          }

          set_bypass(false);

          presets::Manager::self().autoload(PipelineType::output, node.name, node.device_profile_name);
        }
      },
      Qt::QueuedConnection);

  connect(
      db::StreamOutputs::self(), &db::StreamOutputs::pluginsChanged, this,
      [&]() {
        if (db::Main::bypass()) {
          db::Main::setBypass(false);

          return;  // filter connected through update_bypass_state
        }

        set_bypass(false);

        Q_EMIT pipelineChanged();
      },
      Qt::QueuedConnection);

  connect(pm, &pw::Manager::linkChanged, this, &StreamOutputEffects::on_link_changed, Qt::QueuedConnection);

  connect(
      pm, &pw::Manager::sinkProfileNameChanged, this,
      [](pw::NodeInfo node) {
        presets::Manager::self().autoload(PipelineType::output, node.name, node.device_profile_name);
      },
      Qt::QueuedConnection);

  auto* PULSE_SINK = std::getenv("PULSE_SINK");

  if (PULSE_SINK != nullptr && PULSE_SINK != tags::pipewire::ee_sink_name) {
    auto node = pm->model_nodes.get_node_by_name(PULSE_SINK);

    db::StreamOutputs::setOutputDevice(PULSE_SINK);
  }

  if (db::StreamOutputs::outputDevice().isEmpty()) {
    db::StreamOutputs::setOutputDevice(pm->defaultOutputDeviceName);
  }

  connect_filters();

  if (auto node = pm->model_nodes.get_node_by_name(db::StreamOutputs::outputDevice()); node.serial != SPA_ID_INVALID) {
    presets::Manager::self().autoload(PipelineType::output, node.name, node.device_profile_name);
  }
}

auto StreamOutputEffects::apps_want_to_play() -> bool {
  return std::ranges::any_of(pm->list_links, [&](const auto& link) {
    return (link.input_node_id == pm->ee_sink_node.id) && (link.state == PW_LINK_STATE_ACTIVE);
  });
}

void StreamOutputEffects::on_link_changed(const pw::LinkInfo link_info) {
  // We are not interested in the other link states

  if (link_info.state != PW_LINK_STATE_ACTIVE) {
    return;
  }

  if (pm->defaultOutputDeviceName == pm->ee_sink_node.name) {
    return;
  }

  // If bypass is enabled do not touch the plugin pipeline

  if (bypass) {
    return;
  }

  if (apps_want_to_play()) {
    if (list_proxies.empty()) {
      util::debug("At least one app linked to our device wants to play. Linking our filters.");

      connect_filters();
    };
  } else {
    if (db::Main::inactivityTimerEnable()) {
      // if the timer is enabled, wait for the timeout, then unlink plugin pipeline

      QTimer::singleShot(db::Main::inactivityTimeout() * 1000, this, [&]() {
        if (!apps_want_to_play() && !list_proxies.empty()) {
          util::debug("No app linked to our device wants to play. Unlinking our filters.");

          disconnect_filters();
        }
      });
    } else {
      // otherwise, do nothing
      if (!list_proxies.empty()) {
        util::debug(
            "No app linked to our device wants to play, but the inactivity timer is disabled. Leaving filters linked.");
      }
    }
  }
}

void StreamOutputEffects::connect_filters(const bool& bypass) {
  // Checking if the output device exists.

  if (db::StreamOutputs::outputDevice().isEmpty()) {
    util::debug("No output device set. Aborting the link");

    return;
  }

  auto output_device = pm->model_nodes.get_node_by_name(db::StreamOutputs::outputDevice());

  if (output_device.serial == SPA_ID_INVALID) {
    util::debug("The output device " + db::StreamOutputs::outputDevice().toStdString() +
                " is not available. Aborting the link");

    return;
  }

  // Waiting for the output device ports information to be available.

  int timeout = 0;

  while (pm->count_node_ports(output_device.id) < 2) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));

    timeout++;

    if (timeout > 5000) {  // 5 seconds
      util::warning(
          std::format("Information about the ports of the output device {} with id {} are taking to long to be "
                      "available. Aborting the link",
                      output_device.name.toStdString(), util::to_string(output_device.id)));

      return;
    }
  }

  // Link global level meter to output device.

  uint next_node_id = output_device.id;
  uint prev_node_id = output_level->get_node_id();

  auto links = pm->link_nodes(prev_node_id, next_node_id);

  for (auto* link : links) {
    list_proxies.push_back(link);
  }

  if (links.size() < 2U) {
    util::warning(
        std::format("Link from global level meter {} to output device {} failed", prev_node_id, next_node_id));
  }

  // Link spectrum to global level meter.

  next_node_id = prev_node_id;
  prev_node_id = spectrum->get_node_id();

  links = pm->link_nodes(prev_node_id, next_node_id);

  for (auto* link : links) {
    list_proxies.push_back(link);
  }

  if (links.size() < 2U) {
    util::warning(std::format("Link from spectrum {} to global level meter {} failed", prev_node_id, next_node_id));
  }

  // Link plugins in reverse order.

  next_node_id = prev_node_id;

  const auto list = (bypass) ? QStringList() : db::StreamOutputs::plugins();

  if (!list.empty()) {
    for (const auto& name : std::ranges::reverse_view(list)) {
      if (!plugins.contains(name) || plugins[name] == nullptr) {
        continue;
      }

      if (!plugins[name]->connected_to_pw ? plugins[name]->connect_to_pw() : true) {
        prev_node_id = plugins[name]->get_node_id();

        links = pm->link_nodes(prev_node_id, next_node_id);

        for (auto* link : links) {
          list_proxies.push_back(link);
        }

        if (links.size() == 2U) {
          next_node_id = prev_node_id;
        } else {
          util::warning(std::format("Link from node {} to node {} failed", prev_node_id, next_node_id));
        }
      }
    }

    // Checking if we have to link the Echo Canceller probe to the output device.
    // Here we can loop the plugins in normal order,

    for (const auto& name : list) {
      if (!plugins.contains(name) || plugins[name] == nullptr) {
        continue;
      }

      if (name.startsWith(tags::plugin_name::BaseName::echoCanceller)) {
        if (plugins[name]->connected_to_pw) {
          for (const auto& link : pm->link_nodes(output_device.id, plugins[name]->get_node_id(), true)) {
            list_proxies.push_back(link);
          }
        }
      }

      plugins[name]->update_probe_links();
    }
  }

  // Link the EasyEffects sink to the first plugin
  // (or the spectrum in case no plugins are used).

  prev_node_id = pm->ee_sink_node.id;

  links = pm->link_nodes(prev_node_id, next_node_id);

  for (auto* link : links) {
    list_proxies.push_back(link);
  }

  if (links.size() < 2U) {
    util::warning(std::format("Link from easyeffecst sink {} to node {} failed", prev_node_id, next_node_id));
  }
}

void StreamOutputEffects::disconnect_filters() {
  std::set<uint> link_id_list;

  const auto selected_plugins_list = (bypass) ? QStringList() : db::StreamOutputs::plugins();

  for (const auto& plugin : plugins | std::views::values) {
    if (plugin == nullptr) {
      continue;
    }

    for (const auto& link : pm->list_links) {
      if (link.input_node_id == plugin->get_node_id() || link.output_node_id == plugin->get_node_id()) {
        link_id_list.insert(link.id);
      }
    }

    if (plugin->connected_to_pw) {
      if (std::ranges::find(selected_plugins_list, plugin->name) == selected_plugins_list.end()) {
        util::debug("Disconnecting the " + plugin->name.toStdString() + " filter from PipeWire");

        plugin->disconnect_from_pw();
      }
    }
  }

  for (const auto& link : pm->list_links) {
    if (link.input_node_id == spectrum->get_node_id() || link.output_node_id == spectrum->get_node_id() ||
        link.input_node_id == output_level->get_node_id() || link.output_node_id == output_level->get_node_id()) {
      link_id_list.insert(link.id);
    }
  }

  for (const auto& id : link_id_list) {
    pm->destroy_object(static_cast<int>(id));
  }

  pm->destroy_links(list_proxies);

  list_proxies.clear();

  remove_unused_filters();
}

void StreamOutputEffects::set_bypass(const bool& state) {
  bypass = state;

  disconnect_filters();

  connect_filters(state);
}
