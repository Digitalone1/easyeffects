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

#include "pw_manager.hpp"
#include <pipewire/client.h>
#include <pipewire/context.h>
#include <pipewire/core.h>
#include <pipewire/device.h>
#include <pipewire/extensions/metadata.h>
#include <pipewire/keys.h>
#include <pipewire/link.h>
#include <pipewire/module.h>
#include <pipewire/node.h>
#include <pipewire/pipewire.h>
#include <pipewire/port.h>
#include <pipewire/properties.h>
#include <pipewire/proxy.h>
#include <pipewire/thread-loop.h>
#include <pipewire/version.h>
#include <qobjectdefs.h>
#include <qqml.h>
#include <qtmetamacros.h>
#include <spa/monitor/device.h>
#include <spa/param/audio/raw-types.h>
#include <spa/param/audio/raw.h>
#include <spa/param/format.h>
#include <spa/param/param.h>
#include <spa/param/props.h>
#include <spa/param/route.h>
#include <spa/pod/builder.h>  // NOLINT
#include <spa/pod/iter.h>
#include <spa/pod/parser.h>
#include <spa/pod/pod.h>
#include <spa/pod/vararg.h>
#include <spa/utils/defs.h>
#include <spa/utils/dict.h>
#include <spa/utils/hook.h>
#include <spa/utils/json.h>
#include <spa/utils/keys.h>
#include <spa/utils/result.h>
#include <spa/utils/type.h>
#include <sys/types.h>
#include <QString>
#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <span>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
#include "config.h"
#include "db_manager.hpp"
#include "pw_model_clients.hpp"
#include "pw_model_modules.hpp"
#include "pw_model_nodes.hpp"
#include "pw_objects.hpp"
#include "tags_app.hpp"
#include "tags_pipewire.hpp"
#include "util.hpp"

namespace {

struct node_data {
  pw_proxy* proxy = nullptr;

  spa_hook proxy_listener{};

  spa_hook object_listener{};

  pw::Manager* pm = nullptr;

  pw::NodeInfo* nd_info{};
};

struct proxy_data {
  pw_proxy* proxy = nullptr;

  spa_hook proxy_listener{};

  spa_hook object_listener{};

  pw::Manager* pm = nullptr;

  uint id = SPA_ID_INVALID;

  uint64_t serial = SPA_ID_INVALID;
};

template <typename T>
auto spa_dict_get_string(const spa_dict* props, const char* key, T& str) -> bool {
  // If we will use string views in the future, this template could be useful.
  if (const auto* s = spa_dict_lookup(props, key)) {
    str = s;

    return true;
  }

  return false;
}

template <typename T>
auto spa_dict_get_num(const spa_dict* props, const char* key, T& num) -> bool {
  if (const auto* n = spa_dict_lookup(props, key)) {
    return util::str_to_num(std::string(n), num);
  }

  return false;
}

auto spa_dict_get_bool(const spa_dict* props, const char* key, bool& b) -> bool {
  // Returning bool is for conversion success state.
  // The bool value is assigned to reference parameter.
  if (const auto* v = spa_dict_lookup(props, key)) {
    b = (std::strcmp(v, "true") == 0);

    return true;
  }

  return false;
}

void on_removed_proxy(void* data) {
  auto* const pd = static_cast<proxy_data*>(data);

  // NOLINTNEXTLINE(clang-analyzer-core.NullDereference)
  if (pd->object_listener.link.next != nullptr || pd->object_listener.link.prev != nullptr) {
    spa_hook_remove(&pd->object_listener);
  }

  pw_proxy_destroy(pd->proxy);
}

auto link_info_from_props(const spa_dict* props) -> pw::LinkInfo {
  pw::LinkInfo info;

  spa_dict_get_num(props, PW_KEY_LINK_ID, info.id);

  spa_dict_get_num(props, PW_KEY_OBJECT_SERIAL, info.serial);

  spa_dict_get_string(props, PW_KEY_OBJECT_PATH, info.path);

  spa_dict_get_num(props, PW_KEY_LINK_INPUT_NODE, info.input_node_id);

  spa_dict_get_num(props, PW_KEY_LINK_INPUT_PORT, info.input_port_id);

  spa_dict_get_num(props, PW_KEY_LINK_OUTPUT_NODE, info.output_node_id);

  spa_dict_get_num(props, PW_KEY_LINK_OUTPUT_PORT, info.output_port_id);

  spa_dict_get_bool(props, PW_KEY_LINK_PASSIVE, info.passive);

  return info;
}

auto port_info_from_props(const spa_dict* props) -> pw::PortInfo {
  pw::PortInfo info;

  spa_dict_get_num(props, PW_KEY_PORT_ID, info.port_id);

  spa_dict_get_num(props, PW_KEY_OBJECT_SERIAL, info.serial);

  spa_dict_get_string(props, PW_KEY_PORT_NAME, info.name);

  spa_dict_get_num(props, PW_KEY_NODE_ID, info.node_id);

  spa_dict_get_string(props, PW_KEY_PORT_DIRECTION, info.direction);

  spa_dict_get_string(props, PW_KEY_AUDIO_CHANNEL, info.audio_channel);

  spa_dict_get_string(props, PW_KEY_AUDIO_FORMAT, info.format_dsp);

  spa_dict_get_bool(props, PW_KEY_PORT_PHYSICAL, info.physical);

  spa_dict_get_bool(props, PW_KEY_PORT_TERMINAL, info.terminal);

  spa_dict_get_bool(props, PW_KEY_PORT_MONITOR, info.monitor);

  return info;
}

void on_removed_node_proxy(void* data) {
  auto* const nd = static_cast<node_data*>(data);

  spa_hook_remove(&nd->object_listener);

  pw_proxy_destroy(nd->proxy);
}

void on_destroy_node_proxy(void* data) {
  auto* const nd = static_cast<node_data*>(data);

  auto* const pm = nd->pm;

  pm->model_nodes.remove_by_serial(nd->nd_info->serial);

  nd->nd_info->proxy = nullptr;

  spa_hook_remove(&nd->proxy_listener);

  if (nd->nd_info->media_class == tags::pipewire::media_class::source) {
    if (db::StreamInputs::useDefaultInputDevice() && nd->nd_info->name == db::StreamInputs::inputDevice()) {
      pm->input_device.id = SPA_ID_INVALID;
      pm->input_device.serial = SPA_ID_INVALID;
    }
  } else if (nd->nd_info->media_class == tags::pipewire::media_class::sink) {
    if (db::StreamOutputs::useDefaultOutputDevice() && nd->nd_info->name == db::StreamOutputs::outputDevice()) {
      pm->output_device.id = SPA_ID_INVALID;
      pm->output_device.serial = SPA_ID_INVALID;
    }
  }

  util::debug(nd->nd_info->media_class.toStdString() + " " + util::to_string(nd->nd_info->id) + " " +
              nd->nd_info->name.toStdString() + " has been removed");

  delete nd->nd_info;
}

void on_node_info(void* object, const struct pw_node_info* info) {
  if (pw::Manager::exiting) {
    return;
  }

  auto* const nd = static_cast<node_data*>(object);

  auto* const pm = nd->pm;

  // Check if the node has to be removed

  bool ignore_node = false;

  // Exclude blocklisted App id.
  // To be checked here because PW_KEY_APP_ID is not set in on_registry_global.

  if (const auto* app_id = spa_dict_lookup(info->props, PW_KEY_APP_ID)) {
    if (std::ranges::find(pm->blocklist_app_id, app_id) != pm->blocklist_app_id.end()) {
      ignore_node = true;
    }
  }

  // Exclude capture streams.
  // Even PW_KEY_STREAM_CAPTURE_SINK is not set in on_registry_global.
  // Useful to exclude OBS recording streams.

  if (const auto* is_capture_sink = spa_dict_lookup(info->props, PW_KEY_STREAM_CAPTURE_SINK)) {
    if (std::strcmp(is_capture_sink, "true") == 0 && pw::Manager::exclude_monitor_stream) {
      ignore_node = true;
    }
  }

  if (nd->nd_info->media_class == tags::pipewire::media_class::input_stream && !pm->input_device.name.isEmpty()) {
    if (const auto* target_object = spa_dict_lookup(info->props, PW_KEY_TARGET_OBJECT)) {
      /*
        target.object can a name or serial number:
        https://gitlab.freedesktop.org/pipewire/pipewire/-/blob/master/src/pipewire/keys.h#L334
      */

      uint64_t serial = SPA_ID_INVALID;

      if (util::str_to_num(target_object, serial)) {
        if (serial != SPA_ID_INVALID && (serial != pm->input_device.serial && serial != pm->ee_source_node.serial)) {
          ignore_node = true;
        }
      } else if (target_object != pm->input_device.name && target_object != pm->ee_source_node.name) {
        ignore_node = true;
      }

      if (ignore_node) {
        util::debug("The input stream " + nd->nd_info->name.toStdString() +
                    " does not have as target the same mic used as EE input: " + pm->input_device.name.toStdString() +
                    "\n The user wants it to record from device " + target_object + ". We will ignore this stream.");
      }
    }
  }

  if (nd->nd_info->media_class == tags::pipewire::media_class::output_stream && !pm->output_device.name.isEmpty()) {
    if (const auto* target_object = spa_dict_lookup(info->props, PW_KEY_TARGET_OBJECT)) {
      /*
        target.object can a name or serial number:
        https://gitlab.freedesktop.org/pipewire/pipewire/-/blob/master/src/pipewire/keys.h#L334
      */

      uint64_t serial = SPA_ID_INVALID;

      util::str_to_num(target_object, serial);

      if (util::str_to_num(target_object, serial)) {
        if (serial != SPA_ID_INVALID && (serial != pm->output_device.serial && serial != pm->ee_sink_node.serial)) {
          ignore_node = true;
        }
      } else if (target_object != pm->output_device.name && target_object != pm->ee_sink_node.name) {
        ignore_node = true;
      }

      if (ignore_node) {
        util::debug(
            "The output stream " + nd->nd_info->name.toStdString() +
            " does not have as target the same output device used as EE: " + pm->output_device.name.toStdString() +
            "\n The user wants it to play to device " + target_object + ". We will ignore this stream.");
      }
    }
  }

  if (ignore_node) {
    return;
  }

  // Chech for node info updates

  nd->nd_info->state = info->state;
  nd->nd_info->n_input_ports = static_cast<int>(info->n_input_ports);
  nd->nd_info->n_output_ports = static_cast<int>(info->n_output_ports);

  spa_dict_get_num(info->props, PW_KEY_PRIORITY_SESSION, nd->nd_info->priority);

  if (const auto* app_id = spa_dict_lookup(info->props, PW_KEY_APP_ID)) {
    nd->nd_info->application_id = app_id;
  }

  // spa_dict_get_string(props, PW_KEY_APP_PROCESS_BINARY, app_process_binary);

  if (const auto* app_name = spa_dict_lookup(info->props, PW_KEY_APP_NAME)) {
    nd->nd_info->app_name = app_name;
  }

  if (const auto* app_process_id = spa_dict_lookup(info->props, PW_KEY_APP_PROCESS_ID)) {
    nd->nd_info->app_process_id = app_process_id;
  }

  if (const auto* app_process_binary = spa_dict_lookup(info->props, PW_KEY_APP_PROCESS_BINARY)) {
    nd->nd_info->app_process_binary = app_process_binary;
  }

  if (const auto* app_icon_name = spa_dict_lookup(info->props, PW_KEY_APP_ICON_NAME)) {
    nd->nd_info->app_icon_name = app_icon_name;
  }

  if (const auto* media_icon_name = spa_dict_lookup(info->props, PW_KEY_MEDIA_ICON_NAME)) {
    nd->nd_info->media_icon_name = media_icon_name;
  }

  spa_dict_get_string(info->props, PW_KEY_DEVICE_ICON_NAME, nd->nd_info->device_icon_name);

  if (const auto* media_name = spa_dict_lookup(info->props, PW_KEY_MEDIA_NAME)) {
    nd->nd_info->media_name = media_name;
  }

  if (const auto* node_latency = spa_dict_lookup(info->props, PW_KEY_NODE_LATENCY)) {
    const auto str = std::string(node_latency);

    const auto delimiter_pos = str.find('/');

    int rate = 1;

    if (util::str_to_num(str.substr(delimiter_pos + 1U), rate)) {
      nd->nd_info->rate = rate;
    }

    float pw_lat = 0.0F;

    if (util::str_to_num(str.substr(0U, delimiter_pos), pw_lat)) {
      nd->nd_info->latency = pw_lat / static_cast<float>(nd->nd_info->rate);
    }
  }

  spa_dict_get_num(info->props, PW_KEY_DEVICE_ID, nd->nd_info->device_id);

  // sometimes PipeWire destroys the pointer before signal_idle is called,
  // therefore we make a copy

  if (nd->nd_info->connected != pm->stream_is_connected(info->id, nd->nd_info->media_class)) {
    nd->nd_info->connected = !nd->nd_info->connected;
  }

  if (nd->nd_info->media_class == tags::pipewire::media_class::source) {
    const auto nd_info_copy = *nd->nd_info;

    if (nd_info_copy.serial == pm->ee_source_node.serial) {
      pm->ee_source_node = nd_info_copy;
    }

  } else if (nd->nd_info->media_class == tags::pipewire::media_class::sink) {
    const auto nd_info_copy = *nd->nd_info;

    if (nd_info_copy.serial == pm->ee_sink_node.serial) {
      pm->ee_sink_node = nd_info_copy;
    }
  }

  // update NodeInfo inside map or add it if it is not in the model yet

  if (!pm->model_nodes.has_serial(nd->nd_info->serial)) {
    pm->model_nodes.append(*nd->nd_info);

    auto nd_info_copy = *nd->nd_info;

    if (nd_info_copy.media_class == tags::pipewire::media_class::source &&
        nd_info_copy.name != tags::pipewire::ee_source_name) {
      Q_EMIT pm->sourceAdded(nd_info_copy);
    } else if (nd_info_copy.media_class == tags::pipewire::media_class::sink &&
               nd_info_copy.name != tags::pipewire::ee_sink_name) {
      Q_EMIT pm->sinkAdded(nd_info_copy);
    } else if (nd_info_copy.media_class == tags::pipewire::media_class::output_stream) {
      if (db::Main::processAllOutputs() && !nd->nd_info->connected && !nd->nd_info->is_blocklisted) {
        // target.node for backward compatibility with old PW session managers
        // NOLINTNEXTLINE
        pw_metadata_set_property(pm->metadata, nd->nd_info->id, "target.node", "Spa:Id",
                                 util::to_string(pm->ee_sink_node.id).c_str());

        pw_metadata_set_property(pm->metadata, nd->nd_info->id, "target.object", "Spa:Id",
                                 util::to_string(pm->ee_sink_node.serial).c_str());
      }
    } else if (nd_info_copy.media_class == tags::pipewire::media_class::input_stream) {
      if (db::Main::processAllInputs() && !nd->nd_info->connected && !nd->nd_info->is_blocklisted) {
        // target.node for backward compatibility with old PW session managers
        // NOLINTNEXTLINE
        pw_metadata_set_property(pm->metadata, nd->nd_info->id, "target.node", "Spa:Id",
                                 util::to_string(pm->ee_source_node.id).c_str());

        pw_metadata_set_property(pm->metadata, nd->nd_info->id, "target.object", "Spa:Id",
                                 util::to_string(pm->ee_source_node.serial).c_str());
      }
    }

  } else {
    pm->model_nodes.update_info(*nd->nd_info);
  }

  if ((info->change_mask & PW_NODE_CHANGE_MASK_PARAMS) != 0U) {
    auto params = std::span(info->params, info->n_params);

    for (auto param : params) {
      if ((param.flags & SPA_PARAM_INFO_READ) == 0U) {
        continue;
      }

      if (const auto id = param.id; id == SPA_PARAM_Props || id == SPA_PARAM_EnumFormat || id == SPA_PARAM_Format) {
        pw_node_enum_params((struct pw_node*)nd->proxy, 0, id, 0, -1, nullptr);  // NOLINT
      }
    }
  }

  // const struct spa_dict_item* item = nullptr;
  // spa_dict_for_each(item, info->props) printf("\t\t%s: \"%s\"\n", item->key, item->value);
}

void on_node_event_param(void* object,
                         [[maybe_unused]] int seq,
                         [[maybe_unused]] uint32_t id,
                         [[maybe_unused]] uint32_t index,
                         [[maybe_unused]] uint32_t next,
                         const struct spa_pod* param) {
  if (pw::Manager::exiting || param == nullptr) {
    return;
  }

  auto* const nd = static_cast<node_data*>(object);

  auto* const pm = nd->pm;

  const auto serial = nd->nd_info->serial;

  const auto rowIndex = pm->model_nodes.get_row_by_serial(serial);

  if (rowIndex == -1) {
    return;
  }

  spa_pod_prop* pod_prop = nullptr;
  auto* obj = (spa_pod_object*)param;

  SPA_POD_OBJECT_FOREACH(obj, pod_prop) {
    switch (pod_prop->key) {
      case SPA_FORMAT_AUDIO_format: {
        uint format = 0U;

        if (spa_pod_get_id(&pod_prop->value, &format) != 0) {
          break;
        }

        QString format_str = "unknown";

        for (const auto type_info : std::to_array(spa_type_audio_format)) {
          if (format == type_info.type) {
            if (type_info.name != nullptr) {
              QString long_name = type_info.name;

              format_str = long_name.sliced(long_name.lastIndexOf(":") + 1);
            }
          }
        }

        if (format_str != nd->nd_info->format) {
          nd->nd_info->format = format_str;

          pm->model_nodes.update_field(rowIndex, pw::models::Nodes::Roles::Format, format_str);
        }

        break;
      }
      case SPA_FORMAT_AUDIO_rate: {
        int rate = 1;

        if (spa_pod_get_int(&pod_prop->value, &rate) != 0) {
          break;
        }

        if (rate == nd->nd_info->rate) {
          break;
        }

        nd->nd_info->rate = rate;

        pm->model_nodes.update_field(rowIndex, pw::models::Nodes::Roles::Rate, rate);

        break;
      }
      case SPA_PROP_mute: {
        auto v = false;

        if (spa_pod_get_bool(&pod_prop->value, &v) != 0) {
          break;
        }

        if (v == nd->nd_info->mute) {
          break;
        }

        nd->nd_info->mute = v;

        pm->model_nodes.update_field(rowIndex, pw::models::Nodes::Roles::Mute, v);

        break;
      }
      case SPA_PROP_channelVolumes: {
        std::array<float, SPA_AUDIO_MAX_CHANNELS> volumes{};

        const auto n_volumes =
            spa_pod_copy_array(&pod_prop->value, SPA_TYPE_Float, volumes.data(), SPA_AUDIO_MAX_CHANNELS);

        float max = 0.0F;

        for (uint i = 0U; i < n_volumes; i++) {
          max = std::max(volumes.at(i), max);
        }

        if (n_volumes != nd->nd_info->n_volume_channels || max != nd->nd_info->volume) {
          nd->nd_info->n_volume_channels = n_volumes;
          nd->nd_info->volume = max;

          pm->model_nodes.update_field(rowIndex, pw::models::Nodes::NvolumeChannels, n_volumes);
          pm->model_nodes.update_field(rowIndex, pw::models::Nodes::Volume, max);
        }

        break;
      }
      default:
        break;
    }
  }

  if (nd->nd_info->serial == pm->ee_source_node.serial) {
    pm->ee_source_node = *nd->nd_info;
  }

  if (nd->nd_info->serial == pm->ee_sink_node.serial) {
    pm->ee_sink_node = *nd->nd_info;
  }
}

void on_link_info(void* object, const struct pw_link_info* info) {
  auto* const ld = static_cast<proxy_data*>(object);
  auto* const pm = ld->pm;

  pw::LinkInfo link_copy;

  for (auto& l : ld->pm->list_links) {
    if (l.serial == ld->serial) {
      l.state = info->state;

      link_copy = l;

      Q_EMIT pm->linkChanged(link_copy);

      // util::warning(pw_link_state_as_string(l.state));

      break;
    }
  }

  // const struct spa_dict_item* item = nullptr;
  // spa_dict_for_each(item, info->props) printf("\t\t%s: \"%s\"\n", item->key, item->value);
}

void on_destroy_link_proxy(void* data) {
  auto* const ld = static_cast<proxy_data*>(data);

  spa_hook_remove(&ld->proxy_listener);

  ld->pm->list_links.erase(std::remove_if(ld->pm->list_links.begin(), ld->pm->list_links.end(),
                                          [=](const auto& n) { return n.serial == ld->serial; }),
                           ld->pm->list_links.end());
}

void on_destroy_port_proxy(void* data) {
  auto* const pd = static_cast<proxy_data*>(data);

  spa_hook_remove(&pd->proxy_listener);

  pd->pm->list_ports.erase(std::remove_if(pd->pm->list_ports.begin(), pd->pm->list_ports.end(),
                                          [=](const auto& n) { return n.serial == pd->serial; }),
                           pd->pm->list_ports.end());
}

void on_module_info(void* object, const struct pw_module_info* info) {
  auto* const md = static_cast<proxy_data*>(object);

  auto list = md->pm->model_modules.get_list();

  for (int n = 0; n < list.size(); n++) {
    if (list[n].id == info->id) {
      if (info->props != nullptr) {
        QString description;

        spa_dict_get_string(info->props, PW_KEY_MODULE_DESCRIPTION, description);

        md->pm->model_modules.update_field(n, pw::models::Modules::Roles::Description, description);
      }

      if (info->filename != nullptr) {
        md->pm->model_modules.update_field(n, pw::models::Modules::Roles::Filename, info->filename);
      }

      break;
    }
  }
}

void on_destroy_module_proxy(void* data) {
  auto* const md = static_cast<proxy_data*>(data);

  spa_hook_remove(&md->proxy_listener);

  md->pm->model_modules.remove_by_id(md->id);
}

void on_client_info(void* object, const struct pw_client_info* info) {
  auto* const cd = static_cast<proxy_data*>(object);

  auto list = cd->pm->model_clients.get_list();

  for (int n = 0; n < list.size(); n++) {
    if (list[n].id == info->id && info->props != nullptr) {
      QString name;
      QString access;
      QString api;

      spa_dict_get_string(info->props, PW_KEY_APP_NAME, name);

      spa_dict_get_string(info->props, PW_KEY_ACCESS, access);

      spa_dict_get_string(info->props, PW_KEY_CLIENT_API, api);

      cd->pm->model_clients.update_field(n, pw::models::Clients::Roles::Name, name);
      cd->pm->model_clients.update_field(n, pw::models::Clients::Roles::Access, access);
      cd->pm->model_clients.update_field(n, pw::models::Clients::Roles::Api, api);

      break;
    }
  }
}

void on_destroy_client_proxy(void* data) {
  auto* const cd = static_cast<proxy_data*>(data);

  spa_hook_remove(&cd->proxy_listener);

  cd->pm->model_clients.remove_by_id(cd->id);
}

void on_device_info(void* object, const struct pw_device_info* info) {
  auto* const dd = static_cast<proxy_data*>(object);

  for (auto& device : dd->pm->list_devices) {
    if (device.id != info->id) {
      continue;
    }

    spa_dict_get_string(info->props, PW_KEY_DEVICE_NAME, device.name);

    spa_dict_get_string(info->props, PW_KEY_DEVICE_NICK, device.nick);

    spa_dict_get_string(info->props, PW_KEY_DEVICE_DESCRIPTION, device.description);

    spa_dict_get_string(info->props, PW_KEY_DEVICE_API, device.api);

    if (spa_dict_get_string(info->props, SPA_KEY_DEVICE_BUS_ID, device.bus_id)) {
      std::ranges::replace(device.bus_id, ':', '_');
      std::ranges::replace(device.bus_id, '+', '_');
    }

    if (spa_dict_get_string(info->props, PW_KEY_DEVICE_BUS_PATH, device.bus_path)) {
      std::ranges::replace(device.bus_path, ':', '_');
      std::ranges::replace(device.bus_path, '+', '_');
    }

    /*
        For some reason bluez5 devices do not define bus-path or bus-id. So as a workaround we set
       SPA_KEY_API_BLUEZ5_ADDRESS as bus_path
    */

    if (device.api == "bluez5") {
      if (spa_dict_get_string(info->props, SPA_KEY_API_BLUEZ5_ADDRESS, device.bus_path)) {
        std::replace(device.bus_path.begin(), device.bus_path.end(), ':', '_');
      }
    }

    if ((info->change_mask & PW_DEVICE_CHANGE_MASK_PARAMS) != 0U) {
      auto params = std::span(info->params, info->n_params);

      for (auto param : params) {
        if ((param.flags & SPA_PARAM_INFO_READ) == 0U) {
          continue;
        }

        if (const auto id = param.id; id == SPA_PARAM_Route) {
          pw_device_enum_params((struct pw_device*)dd->proxy, 0, id, 0, -1, nullptr);  // NOLINT
        }
      }
    }

    break;
  }
}

void on_device_event_param(void* object,
                           [[maybe_unused]] int seq,
                           [[maybe_unused]] uint32_t id,
                           [[maybe_unused]] uint32_t index,
                           [[maybe_unused]] uint32_t next,
                           const struct spa_pod* param) {
  if (id != SPA_PARAM_Route) {
    return;
  }

  auto* const dd = static_cast<proxy_data*>(object);

  const char* name = nullptr;

  enum spa_direction direction {};
  enum spa_param_availability available {};

  // NOLINTNEXTLINE
  if (spa_pod_parse_object(param, SPA_TYPE_OBJECT_ParamRoute, nullptr, SPA_PARAM_ROUTE_direction,
                           SPA_POD_Id(&direction), SPA_PARAM_ROUTE_name, SPA_POD_String(&name),
                           SPA_PARAM_ROUTE_available, SPA_POD_Id(&available)) < 0) {
    return;
  }

  if (name == nullptr) {
    return;
  }

  for (auto& device : dd->pm->list_devices) {
    if (device.id != dd->id) {
      continue;
    }

    auto* const pm = dd->pm;

    if (direction == SPA_DIRECTION_INPUT) {
      if (name != device.input_route_name || available != device.input_route_available) {
        device.input_route_name = name;
        device.input_route_available = available;

        Q_EMIT pm->deviceInputRouteChanged(device);
      }
    } else if (direction == SPA_DIRECTION_OUTPUT) {
      if (name != device.output_route_name || available != device.output_route_available) {
        device.output_route_name = name;
        device.output_route_available = available;

        Q_EMIT pm->deviceOutputRouteChanged(device);
      }
    }

    break;
  }
}

void on_destroy_device_proxy(void* data) {
  auto* const dd = static_cast<proxy_data*>(data);

  spa_hook_remove(&dd->proxy_listener);

  dd->pm->list_devices.erase(std::remove_if(dd->pm->list_devices.begin(), dd->pm->list_devices.end(),
                                            [=](const auto& n) { return n.id == dd->id; }),
                             dd->pm->list_devices.end());
}

auto on_metadata_property(void* data, uint32_t id, const char* key, const char* type, const char* value) -> int {
  auto* const pm = static_cast<pw::Manager*>(data);

  const std::string str_key = (key != nullptr) ? key : "";
  const std::string str_type = (type != nullptr) ? type : "";
  const std::string str_value = (value != nullptr) ? value : "";

  util::debug("new metadata property: " + util::to_string(id) + ", " + str_key + ", " + str_type + ", " + str_value);

  if (str_value.empty()) {
    return 0;
  }

  if (str_key == "default.audio.sink") {
    std::array<char, 1024U> v{};

    pw::Manager::json_object_find(str_value.c_str(), "name", v.data(), v.size() * sizeof(char));

    if (std::strcmp(v.data(), tags::pipewire::ee_sink_name) == 0) {
      return 0;
    }

    pm->defaultOutputDeviceName = v.data();

    util::debug("new default output device: " + pm->defaultOutputDeviceName.toStdString());

    Q_EMIT pm->newDefaultSinkName(pm->defaultOutputDeviceName);
  }

  if (str_key == "default.audio.source") {
    std::array<char, 1024U> v{};

    pw::Manager::json_object_find(str_value.c_str(), "name", v.data(), v.size() * sizeof(char));

    if (std::strcmp(v.data(), tags::pipewire::ee_source_name) == 0) {
      return 0;
    }

    pm->defaultInputDeviceName = v.data();

    util::debug("new default input device: " + pm->defaultInputDeviceName.toStdString());

    Q_EMIT pm->newDefaultSourceName(pm->defaultInputDeviceName);
  }

  return 0;
}

const struct pw_metadata_events metadata_events = {PW_VERSION_METADATA_EVENTS, on_metadata_property};

const struct pw_proxy_events link_proxy_events = {.version = 0,
                                                  .destroy = on_destroy_link_proxy,
                                                  .bound = nullptr,
                                                  .removed = on_removed_proxy,
                                                  .done = nullptr,
                                                  .error = nullptr,
                                                  .bound_props = nullptr};

const struct pw_proxy_events port_proxy_events = {.version = 0,
                                                  .destroy = on_destroy_port_proxy,
                                                  .bound = nullptr,
                                                  .removed = on_removed_proxy,
                                                  .done = nullptr,
                                                  .error = nullptr,
                                                  .bound_props = nullptr};

const struct pw_proxy_events module_proxy_events = {.version = 0,
                                                    .destroy = on_destroy_module_proxy,
                                                    .bound = nullptr,
                                                    .removed = on_removed_proxy,
                                                    .done = nullptr,
                                                    .error = nullptr,
                                                    .bound_props = nullptr};

const struct pw_proxy_events client_proxy_events = {.version = 0,
                                                    .destroy = on_destroy_client_proxy,
                                                    .bound = nullptr,
                                                    .removed = on_removed_proxy,
                                                    .done = nullptr,
                                                    .error = nullptr,
                                                    .bound_props = nullptr};

const struct pw_proxy_events device_proxy_events = {.version = 0,
                                                    .destroy = on_destroy_device_proxy,
                                                    .bound = nullptr,
                                                    .removed = on_removed_proxy,
                                                    .done = nullptr,
                                                    .error = nullptr,
                                                    .bound_props = nullptr};

const struct pw_proxy_events node_proxy_events = {.version = 0,
                                                  .destroy = on_destroy_node_proxy,
                                                  .bound = nullptr,
                                                  .removed = on_removed_node_proxy,
                                                  .done = nullptr,
                                                  .error = nullptr,
                                                  .bound_props = nullptr};

const struct pw_node_events node_events = {.version = 0, .info = on_node_info, .param = on_node_event_param};

const struct pw_link_events link_events = {
    .version = 0,
    .info = on_link_info,
};

const struct pw_module_events module_events = {
    .version = 0,
    .info = on_module_info,
};

const struct pw_client_events client_events = {.version = 0, .info = on_client_info, .permissions = nullptr};

const struct pw_device_events device_events = {.version = 0, .info = on_device_info, .param = on_device_event_param};

void on_registry_global(void* data,
                        uint32_t id,
                        [[maybe_unused]] uint32_t permissions,
                        const char* type,
                        [[maybe_unused]] uint32_t version,
                        const struct spa_dict* props) {
  if (id == SPA_ID_INVALID) {
    // If PipeWire send us a wrong id, we don't have issues
    return;
  }

  auto* const pm = static_cast<pw::Manager*>(data);

  if (std::strcmp(type, PW_TYPE_INTERFACE_Node) == 0) {
    bool is_ee_filter = false;

    if (const auto* key_media_role = spa_dict_lookup(props, PW_KEY_MEDIA_ROLE)) {
      // Exclude blocklisted media roles

      if (std::ranges::find(pm->blocklist_media_role, std::string(key_media_role)) != pm->blocklist_media_role.end()) {
        return;
      }

      if (std::strcmp(key_media_role, "DSP") == 0) {
        if (const auto* key_media_category = spa_dict_lookup(props, PW_KEY_MEDIA_CATEGORY)) {
          if (std::strcmp(key_media_category, "Filter") == 0) {
            if (const auto* key_node_name = spa_dict_lookup(props, PW_KEY_NODE_NAME)) {
              if (std::string node_name(key_node_name); node_name.size() > 3U) {
                if (node_name.starts_with("ee_")) {
                  is_ee_filter = true;
                }
              }
            }
          }
        }
      }
    }

    QString media_class;
    QString media_role;

    if (const auto* key_media_class = spa_dict_lookup(props, PW_KEY_MEDIA_CLASS)) {
      media_class = key_media_class;
    }

    if (const auto* key_media_role = spa_dict_lookup(props, PW_KEY_MEDIA_ROLE)) {
      media_role = key_media_role;
    }

    constexpr auto class_array =
        std::to_array({tags::pipewire::media_class::output_stream, tags::pipewire::media_class::input_stream,
                       tags::pipewire::media_class::sink, tags::pipewire::media_class::source,
                       tags::pipewire::media_class::virtual_source});

    if (!is_ee_filter &&
        !std::any_of(class_array.begin(), class_array.end(), [&](const auto& str) { return str == media_class; })) {
      return;
    }

    QString node_name;

    spa_dict_get_string(props, PW_KEY_NODE_NAME, node_name);

    // At least for now I do not think there is a point in showing the spectrum adn the output level filters in menus

    if (node_name.contains("output_level") || node_name.contains("spectrum")) {
      return;
    }

    // Exclude blocklisted node names

    if (std::ranges::find(pw::Manager::blocklist_node_name, node_name) != pw::Manager::blocklist_node_name.end()) {
      return;
    }

    uint64_t serial = 0U;

    if (!spa_dict_get_num(props, PW_KEY_OBJECT_SERIAL, serial)) {
      util::warning(
          "An error occurred while retrieving the object serial. The node cannot be handled by Easy Effects.");
      return;
    }

    // New node can be added into the node map

    auto* proxy = static_cast<pw_proxy*>(pw_registry_bind(pm->registry, id, type, PW_VERSION_NODE, sizeof(node_data)));

    auto* nd = static_cast<node_data*>(pw_proxy_get_user_data(proxy));

    nd->proxy = proxy;
    nd->pm = pm;

    nd->nd_info = new pw::NodeInfo();

    nd->nd_info->proxy = proxy;
    nd->nd_info->serial = serial;
    nd->nd_info->id = id;
    nd->nd_info->media_role = media_role;
    nd->nd_info->media_class = media_class;
    nd->nd_info->name = node_name;

    if (node_name == tags::pipewire::ee_source_name) {
      nd->nd_info->media_class = tags::pipewire::media_class::ee_source;
    } else if (node_name == tags::pipewire::ee_sink_name) {
      nd->nd_info->media_class = tags::pipewire::media_class::ee_sink;
    }

    spa_dict_get_string(props, PW_KEY_NODE_DESCRIPTION, nd->nd_info->description);

    spa_dict_get_num(props, PW_KEY_PRIORITY_SESSION, nd->nd_info->priority);

    spa_dict_get_num(props, PW_KEY_DEVICE_ID, nd->nd_info->device_id);

    const auto user_blocklist = (media_class == tags::pipewire::media_class::output_stream)
                                    ? db::StreamOutputs::blocklist()
                                    : db::StreamInputs::blocklist();

    nd->nd_info->is_blocklisted =
        std::ranges::find(user_blocklist, nd->nd_info->application_id) != user_blocklist.end();

    nd->nd_info->is_blocklisted =
        nd->nd_info->is_blocklisted || std::ranges::find(user_blocklist, nd->nd_info->name) != user_blocklist.end();

    const auto [node_it, success] = pm->node_map.insert({serial, *nd->nd_info});

    if (!success) {
      util::warning("Cannot insert node " + util::to_string(id) + " " + node_name.toStdString() +
                    " into the node map because there's already an existing serial " + util::to_string(serial));

      return;
    }

    pw_node_add_listener(proxy, &nd->object_listener, &node_events, nd);  // NOLINT
    pw_proxy_add_listener(proxy, &nd->proxy_listener, &node_proxy_events, nd);

    // We will have debug info about our filters later

    if (!is_ee_filter) {
      util::debug(nd->nd_info->media_class.toStdString() + " " + util::to_string(id) + " " +
                  nd->nd_info->name.toStdString() + " with serial " + util::to_string(serial) + " has been added");
    }

    return;
  }

  if (std::strcmp(type, PW_TYPE_INTERFACE_Link) == 0) {
    uint64_t serial = 0U;

    if (!spa_dict_get_num(props, PW_KEY_OBJECT_SERIAL, serial)) {
      util::warning(
          "An error occurred while retrieving the object serial. This link cannot be handled by Easy Effects.");
      return;
    }

    auto* proxy = static_cast<pw_proxy*>(pw_registry_bind(pm->registry, id, type, PW_VERSION_LINK, sizeof(proxy_data)));

    auto* const pd = static_cast<proxy_data*>(pw_proxy_get_user_data(proxy));

    pd->proxy = proxy;
    pd->pm = pm;
    pd->id = id;
    pd->serial = serial;

    pw_link_add_listener(proxy, &pd->object_listener, &link_events, pd);  // NOLINT
    pw_proxy_add_listener(proxy, &pd->proxy_listener, &link_proxy_events, pd);

    auto link_info = link_info_from_props(props);

    link_info.id = id;
    link_info.serial = serial;

    pm->list_links.push_back(link_info);

    try {
      const auto input_node = pm->node_map_at_id(link_info.input_node_id);

      const auto output_node = pm->node_map_at_id(link_info.output_node_id);

      util::debug(output_node.name.toStdString() + " port " + util::to_string(link_info.output_port_id) +
                  " is connected to " + input_node.name.toStdString() + " port " +
                  util::to_string(link_info.input_port_id));
    } catch (std::out_of_range& e) {
      util::debug(e.what());
    }

    return;
  }

  if (std::strcmp(type, PW_TYPE_INTERFACE_Port) == 0) {
    uint64_t serial = 0U;

    if (!spa_dict_get_num(props, PW_KEY_OBJECT_SERIAL, serial)) {
      util::warning(
          "An error occurred while retrieving the object serial. This port cannot be handled by Easy Effects.");
      return;
    }

    auto* proxy = static_cast<pw_proxy*>(pw_registry_bind(pm->registry, id, type, PW_VERSION_PORT, sizeof(proxy_data)));

    auto* const pd = static_cast<proxy_data*>(pw_proxy_get_user_data(proxy));

    pd->proxy = proxy;
    pd->pm = pm;
    pd->id = id;
    pd->serial = serial;

    pw_proxy_add_listener(proxy, &pd->proxy_listener, &port_proxy_events, pd);

    auto port_info = port_info_from_props(props);

    port_info.id = id;
    port_info.serial = serial;

    // std::cout << port_info.name << "\t" << port_info.audio_channel << "\t" << port_info.direction << "\t"
    //           << port_info.format_dsp << "\t" << port_info.port_id << "\t" << port_info.node_id << std::endl;

    pm->list_ports.push_back(port_info);

    return;
  }

  if (std::strcmp(type, PW_TYPE_INTERFACE_Module) == 0) {
    uint64_t serial = 0U;

    if (!spa_dict_get_num(props, PW_KEY_OBJECT_SERIAL, serial)) {
      util::warning(
          "An error occurred while retrieving the object serial. This module cannot be handled by Easy Effects.");
      return;
    }

    auto* proxy =
        static_cast<pw_proxy*>(pw_registry_bind(pm->registry, id, type, PW_VERSION_MODULE, sizeof(proxy_data)));

    auto* const pd = static_cast<proxy_data*>(pw_proxy_get_user_data(proxy));

    pd->proxy = proxy;
    pd->pm = pm;
    pd->id = id;
    pd->serial = serial;

    pw_module_add_listener(proxy, &pd->object_listener, &module_events, pd);  // NOLINT
    pw_proxy_add_listener(proxy, &pd->proxy_listener, &module_proxy_events, pd);

    pw::ModuleInfo m_info{.id = id, .serial = serial, .name = "", .description = "", .filename = ""};

    spa_dict_get_string(props, PW_KEY_MODULE_NAME, m_info.name);

    pm->model_modules.append(m_info);

    return;
  }

  if (std::strcmp(type, PW_TYPE_INTERFACE_Client) == 0) {
    uint64_t serial = 0U;

    if (!spa_dict_get_num(props, PW_KEY_OBJECT_SERIAL, serial)) {
      util::warning(
          "An error occurred while retrieving the object serial. This client cannot be handled by Easy Effects.");
      return;
    }

    auto* proxy =
        static_cast<pw_proxy*>(pw_registry_bind(pm->registry, id, type, PW_VERSION_CLIENT, sizeof(proxy_data)));

    auto* const pd = static_cast<proxy_data*>(pw_proxy_get_user_data(proxy));

    pd->proxy = proxy;
    pd->pm = pm;
    pd->id = id;
    pd->serial = serial;

    pw_client_add_listener(proxy, &pd->object_listener, &client_events, pd);  // NOLINT
    pw_proxy_add_listener(proxy, &pd->proxy_listener, &client_proxy_events, pd);

    pw::ClientInfo c_info{.id = id, .serial = serial, .name = "", .access = "", .api = ""};

    pm->model_clients.append(c_info);

    return;
  }

  if (std::strcmp(type, PW_TYPE_INTERFACE_Metadata) == 0) {
    if (const auto* name = spa_dict_lookup(props, PW_KEY_METADATA_NAME)) {
      using namespace std::string_literals;

      util::debug("found metadata: "s + name);  // NOLINT(missing-includes, misc-include-cleaner)

      if (std::strcmp(name, "default") == 0) {
        if (pm->metadata != nullptr) {
          util::debug("A new default metadata is available. We will use it");

          spa_hook_remove(&pm->metadata_listener);
        }

        pm->metadata = static_cast<pw_metadata*>(pw_registry_bind(pm->registry, id, type, PW_VERSION_METADATA, 0));

        if (pm->metadata != nullptr) {
          pw_metadata_add_listener(pm->metadata, &pm->metadata_listener, &metadata_events, pm);  // NOLINT
        } else {
          util::warning("pw_registry_bind returned a null metadata object");
        }
      }
    }

    return;
  }

  if (std::strcmp(type, PW_TYPE_INTERFACE_Device) == 0) {
    if (const auto* key_media_class = spa_dict_lookup(props, PW_KEY_MEDIA_CLASS)) {
      const std::string media_class = key_media_class;

      if (media_class == tags::pipewire::media_class::device) {
        uint64_t serial = 0U;

        if (!spa_dict_get_num(props, PW_KEY_OBJECT_SERIAL, serial)) {
          util::warning(
              "An error occurred while converting the object serial. This device cannot be handled by Easy Effects.");
          return;
        }

        auto* proxy =
            static_cast<pw_proxy*>(pw_registry_bind(pm->registry, id, type, PW_VERSION_DEVICE, sizeof(proxy_data)));

        auto* const pd = static_cast<proxy_data*>(pw_proxy_get_user_data(proxy));

        pd->proxy = proxy;
        pd->pm = pm;
        pd->id = id;
        pd->serial = serial;

        pw_device_add_listener(proxy, &pd->object_listener, &device_events, pd);  // NOLINT
        pw_proxy_add_listener(proxy, &pd->proxy_listener, &device_proxy_events, pd);

        pw::DeviceInfo d_info{.id = id,
                              .serial = serial,
                              .name = "",
                              .description = "",
                              .nick = "",
                              .media_class = media_class,
                              .api = "",
                              .input_route_name = "",
                              .output_route_name = "",
                              .bus_id = "",
                              .bus_path = "",
                              .input_route_available = SPA_PARAM_AVAILABILITY_no,
                              .output_route_available = SPA_PARAM_AVAILABILITY_no};

        pm->list_devices.push_back(d_info);
      }
    }

    return;
  }
}

void on_core_error(void* data, uint32_t id, [[maybe_unused]] int seq, int res, const char* message) {
  auto* const pm = static_cast<pw::Manager*>(data);

  using namespace std::string_literals;

  if (id == PW_ID_CORE) {
    util::warning("Remote error res: "s + spa_strerror(res));
    util::warning("Remote error message: "s + message);

    pw_thread_loop_signal(pm->thread_loop, false);
  }
}

void on_core_info(void* data, const struct pw_core_info* info) {
  using namespace std::string_literals;

  auto* const pm = static_cast<pw::Manager*>(data);

  pm->runtimeVersion = info->version;

  spa_dict_get_string(info->props, "default.clock.rate", pm->defaultClockRate);

  spa_dict_get_string(info->props, "default.clock.min-quantum", pm->defaultMinQuantum);

  spa_dict_get_string(info->props, "default.clock.max-quantum", pm->defaultMaxQuantum);

  spa_dict_get_string(info->props, "default.clock.quantum", pm->defaultQuantum);

  util::debug("core version: "s + info->version);
  util::debug("core name: "s + info->name);
}

void on_core_done(void* data, uint32_t id, [[maybe_unused]] int seq) {
  auto* const pm = static_cast<pw::Manager*>(data);

  if (id == PW_ID_CORE) {
    pw_thread_loop_signal(pm->thread_loop, false);
  }
}

const struct pw_core_events core_events = {.version = PW_VERSION_CORE_EVENTS,
                                           .info = on_core_info,
                                           .done = on_core_done,
                                           .ping = nullptr,
                                           .error = on_core_error,
                                           .remove_id = nullptr,
                                           .bound_id = nullptr,
                                           .add_mem = nullptr,
                                           .remove_mem = nullptr,
                                           .bound_props = nullptr};

const struct pw_registry_events registry_events = {.version = 0,
                                                   .global = on_registry_global,
                                                   .global_remove = nullptr};

}  // namespace

namespace pw {

Manager::Manager() : headerVersion(pw_get_headers_version()), libraryVersion(pw_get_library_version()) {
  register_models();

  pw_init(nullptr, nullptr);

  spa_zero(core_listener);
  spa_zero(registry_listener);

  util::debug("compiled with PipeWire: " + headerVersion.toStdString());
  util::debug("linked to PipeWire: " + libraryVersion.toStdString());

  // this needs to occur after pw_init(), so putting it before pw_init() in the initializer breaks this
  // NOLINTNEXTLINE(cppcoreguidelines-prefer-member-initializer)
  thread_loop = pw_thread_loop_new("ee-pipewire-thread", nullptr);

  // NOLINTNEXTLINE(clang-analyzer-core.NullDereference)
  if (thread_loop == nullptr) {
    util::fatal("could not create PipeWire loop");
  }

  if (pw_thread_loop_start(thread_loop) != 0) {
    util::fatal("could not start the loop");
  }

  lock();

  pw_properties* props_context = pw_properties_new(nullptr, nullptr);

  pw_properties_set(props_context, PW_KEY_CONFIG_NAME, "client-rt.conf");
  pw_properties_set(props_context, PW_KEY_MEDIA_TYPE, "Audio");
  pw_properties_set(props_context, PW_KEY_MEDIA_CATEGORY, "Manager");
  pw_properties_set(props_context, PW_KEY_MEDIA_ROLE, "Music");

  context = pw_context_new(pw_thread_loop_get_loop(thread_loop), props_context, 0);

  if (context == nullptr) {
    util::fatal("could not create PipeWire context");
  }

  core = pw_context_connect(context, nullptr, 0);

  if (core == nullptr) {
    util::fatal("context connection failed");
  }

  registry = pw_core_get_registry(core, PW_VERSION_REGISTRY, 0);

  if (registry == nullptr) {
    util::fatal("could not get the registry");
  }

  pw_registry_add_listener(registry, &registry_listener, &registry_events, this);  // NOLINT

  pw_core_add_listener(core, &core_listener, &core_events, this);  // NOLINT

  if (ee_sink_node.id == SPA_ID_INVALID || ee_source_node.id == SPA_ID_INVALID) {
    load_virtual_devices();
  }

  sync_wait_unlock();

  using namespace std::string_literals;

  do {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));

    for (const auto& [serial, node] : node_map) {
      if (ee_sink_node.name.isEmpty() && node.name == tags::pipewire::ee_sink_name) {
        ee_sink_node = node;

        util::debug(tags::pipewire::ee_sink_name + " node successfully retrieved with id "s + util::to_string(node.id) +
                    " and serial " + util::to_string(node.serial));
      } else if (ee_source_node.name.isEmpty() && node.name == tags::pipewire::ee_source_name) {
        ee_source_node = node;

        util::debug(tags::pipewire::ee_source_name + " node successfully retrieved with id "s +
                    util::to_string(node.id) + " and serial " + util::to_string(node.serial));
      }
    }
  } while (ee_sink_node.id == SPA_ID_INVALID || ee_source_node.id == SPA_ID_INVALID);

  /*
    By the time our virtual devices are loaded we may have already received some streams. So
    we connected them here now that our virtual devices are available.
  */

  for (const auto& [serial, node] : node_map) {
    if (node.media_class == tags::pipewire::media_class::output_stream) {
      if (db::Main::processAllOutputs() != 0 && !node.is_blocklisted) {
        connectStreamOutput(node.id);
      }
    } else if (node.media_class == tags::pipewire::media_class::input_stream) {
      if (db::Main::processAllInputs() != 0 && !node.is_blocklisted) {
        connectStreamInput(node.id);
      }
    }
  }
}

Manager::~Manager() {
  exiting = true;

  lock();

  spa_hook_remove(&registry_listener);
  spa_hook_remove(&core_listener);
  spa_hook_remove(&metadata_listener);

  if (metadata != nullptr) {
    pw_proxy_destroy((struct pw_proxy*)metadata);
  }

  pw_proxy_destroy(proxy_stream_output_sink);
  pw_proxy_destroy(proxy_stream_input_source);

  util::debug("Destroying PipeWire registry...");
  pw_proxy_destroy((struct pw_proxy*)registry);

  util::debug("Disconnecting PipeWire core...");
  pw_core_disconnect(core);

  unlock();

  util::debug("Stopping PipeWire's loop...");
  pw_thread_loop_stop(thread_loop);

  util::debug("Destroying PipeWire's context...");
  pw_context_destroy(context);

  util::debug("Destroying PipeWire's loop...");
  pw_thread_loop_destroy(thread_loop);
}

void Manager::register_models() {
  // NOLINTBEGIN(clang-analyzer-cplusplus.NewDelete)
  qmlRegisterSingletonInstance<pw::Manager>("ee.pipewire", VERSION_MAJOR, VERSION_MINOR, "Manager", this);

  qmlRegisterSingletonInstance<pw::models::Nodes>("ee.pipewire", VERSION_MAJOR, VERSION_MINOR, "ModelNodes",
                                                  &model_nodes);

  qmlRegisterSingletonInstance<pw::models::Modules>("ee.pipewire", VERSION_MAJOR, VERSION_MINOR, "ModelModules",
                                                    &model_modules);

  qmlRegisterSingletonInstance<pw::models::Clients>("ee.pipewire", VERSION_MAJOR, VERSION_MINOR, "ModelClients",
                                                    &model_clients);
  // NOLINTEND(clang-analyzer-cplusplus.NewDelete)
}

void Manager::load_virtual_devices() {
  // loading Easy Effects sink

  pw_properties* props_sink = pw_properties_new(nullptr, nullptr);

  pw_properties_set(props_sink, PW_KEY_APP_ID, tags::app::id);
  pw_properties_set(props_sink, PW_KEY_NODE_NAME, tags::pipewire::ee_sink_name);
  pw_properties_set(props_sink, PW_KEY_NODE_DESCRIPTION, "Easy Effects Sink");
  pw_properties_set(props_sink, PW_KEY_NODE_VIRTUAL, "true");
  pw_properties_set(props_sink, PW_KEY_NODE_PASSIVE, "out");
  pw_properties_set(props_sink, "factory.name", "support.null-audio-sink");
  pw_properties_set(props_sink, PW_KEY_MEDIA_CLASS, tags::pipewire::media_class::sink);
  pw_properties_set(props_sink, "audio.position", "FL,FR");
  pw_properties_set(props_sink, "monitor.channel-volumes", "false");
  pw_properties_set(props_sink, "monitor.passthrough", "true");
  pw_properties_set(props_sink, "priority.session", "0");

  proxy_stream_output_sink = static_cast<pw_proxy*>(
      pw_core_create_object(core, "adapter", PW_TYPE_INTERFACE_Node, PW_VERSION_NODE, &props_sink->dict, 0));

  pw_properties_free(props_sink);

  // loading our source

  pw_properties* props_source = pw_properties_new(nullptr, nullptr);

  pw_properties_set(props_source, PW_KEY_APP_ID, tags::app::id);
  pw_properties_set(props_source, PW_KEY_NODE_NAME, tags::pipewire::ee_source_name);
  pw_properties_set(props_source, PW_KEY_NODE_DESCRIPTION, "Easy Effects Source");
  pw_properties_set(props_source, PW_KEY_NODE_VIRTUAL, "true");
  pw_properties_set(props_source, "factory.name", "support.null-audio-sink");
  pw_properties_set(props_source, PW_KEY_MEDIA_CLASS, tags::pipewire::media_class::virtual_source);
  pw_properties_set(props_source, "audio.position", "FL,FR");
  pw_properties_set(props_source, "monitor.channel-volumes", "false");
  pw_properties_set(props_source, "monitor.passthrough", "true");
  pw_properties_set(props_source, "priority.session", "0");

  proxy_stream_input_source = static_cast<pw_proxy*>(
      pw_core_create_object(core, "adapter", PW_TYPE_INTERFACE_Node, PW_VERSION_NODE, &props_source->dict, 0));

  pw_properties_free(props_source);
}

auto Manager::node_map_at_id(const uint& id) -> NodeInfo& {
  // Helper method to access easily a node by id, same functionality as map.at()

  for (auto& [serial, node] : node_map) {
    if (node.id == id) {
      return node;
    }
  }

  throw std::out_of_range("No node with id " + util::to_string(id) + " in our node_map");
}

auto Manager::stream_is_connected(const uint& id, const QString& media_class) -> bool {
  if (media_class == tags::pipewire::media_class::output_stream) {
    for (const auto& link : list_links) {
      if (link.output_node_id == id && link.input_node_id == ee_sink_node.id) {
        return true;
      }
    }
  } else if (media_class == tags::pipewire::media_class::input_stream) {
    for (const auto& link : list_links) {
      if (link.output_node_id == ee_source_node.id && link.input_node_id == id) {
        return true;
      }
    }
  }

  return false;
}

void Manager::connectStreamOutput(const uint& id) const {
  set_metadata_target_node(id, ee_sink_node.id, ee_sink_node.serial);
}

void Manager::connectStreamInput(const uint& id) const {
  set_metadata_target_node(id, ee_source_node.id, ee_source_node.serial);
}

void Manager::set_metadata_target_node(const uint& origin_id,
                                       const uint& target_id,
                                       const uint64_t& target_serial) const {
  if (metadata == nullptr) {
    return;
  }

  lock();

  // target.node for backward compatibility with old PW session managers
  // NOLINTNEXTLINE
  pw_metadata_set_property(metadata, origin_id, "target.node", "Spa:Id", util::to_string(target_id).c_str());
  // NOLINTNEXTLINE
  pw_metadata_set_property(metadata, origin_id, "target.object", "Spa:Id", util::to_string(target_serial).c_str());

  sync_wait_unlock();
}

void Manager::disconnectStream(const uint& stream_id) const {
  if (metadata == nullptr) {
    return;
  }

  lock();

  // target.node for backward compatibility with old PW session managers
  pw_metadata_set_property(metadata, stream_id, "target.node", nullptr, nullptr);
  pw_metadata_set_property(metadata, stream_id, "target.object", nullptr, nullptr);  // NOLINT

  sync_wait_unlock();
}

void Manager::setNodeVolume(const uint& serial, const uint& n_vol_ch, const float& value) {
  if (auto* proxy = model_nodes.get_proxy_by_serial(serial); proxy != nullptr) {
    std::array<float, SPA_AUDIO_MAX_CHANNELS> volumes{};

    std::ranges::fill(volumes, 0.0F);
    std::fill_n(volumes.begin(), n_vol_ch, value);

    std::array<char, 1024U> buffer{};

    auto builder = SPA_POD_BUILDER_INIT(buffer.data(), sizeof(buffer));  // NOLINT

    lock();

    // NOLINTNEXTLINE
    pw_node_set_param(
        (struct pw_node*)proxy, SPA_PARAM_Props, 0,
        (spa_pod*)spa_pod_builder_add_object(&builder, SPA_TYPE_OBJECT_Props, SPA_PARAM_Props, SPA_PROP_channelVolumes,
                                             SPA_POD_Array(sizeof(float), SPA_TYPE_Float, n_vol_ch, volumes.data())));

    sync_wait_unlock();
  }
}

void Manager::setNodeMute(const uint& serial, const bool& state) {
  if (auto* proxy = model_nodes.get_proxy_by_serial(serial); proxy != nullptr) {
    std::array<char, 1024U> buffer{};

    auto builder = SPA_POD_BUILDER_INIT(buffer.data(), sizeof(buffer));  // NOLINT

    lock();

    // NOLINTNEXTLINE
    pw_node_set_param((pw_node*)proxy, SPA_PARAM_Props, 0,
                      (spa_pod*)spa_pod_builder_add_object(&builder, SPA_TYPE_OBJECT_Props, SPA_PARAM_Props,
                                                           SPA_PROP_mute, SPA_POD_Bool(state)));

    sync_wait_unlock();
  }
}

auto Manager::count_node_ports(const uint& node_id) -> uint {
  uint count = 0U;

  for (const auto& port : list_ports) {
    if (port.node_id == node_id) {
      count++;
    }
  }

  return count;
}

auto Manager::link_nodes(const uint& output_node_id,
                         const uint& input_node_id,
                         const bool& probe_link,
                         const bool& link_passive) -> std::vector<pw_proxy*> {
  std::vector<pw_proxy*> list;
  std::vector<PortInfo> list_output_ports;
  std::vector<PortInfo> list_input_ports;
  auto use_audio_channel = true;

  for (const auto& port : list_ports) {
    if (port.node_id == output_node_id && port.direction == "out") {
      list_output_ports.push_back(port);

      if (!probe_link) {
        if (port.audio_channel != "FL" && port.audio_channel != "FR") {
          use_audio_channel = false;
        }
      }
    }

    if (port.node_id == input_node_id && port.direction == "in") {
      if (!probe_link) {
        list_input_ports.push_back(port);

        if (port.audio_channel != "FL" && port.audio_channel != "FR") {
          use_audio_channel = false;
        }
      } else {
        if (port.audio_channel == "PROBE_FL" || port.audio_channel == "PROBE_FR") {
          list_input_ports.push_back(port);
        }
      }
    }
  }

  if (list_input_ports.empty()) {
    util::debug("node " + util::to_string(input_node_id) + " has no input ports yet. Aborting the link");

    return list;
  }

  if (list_output_ports.empty()) {
    util::debug("node " + util::to_string(output_node_id) + " has no output ports yet. Aborting the link");

    return list;
  }

  for (const auto& outp : list_output_ports) {
    for (const auto& inp : list_input_ports) {
      bool ports_match = false;

      if (!probe_link) {
        if (use_audio_channel) {
          ports_match = outp.audio_channel == inp.audio_channel;
        } else {
          ports_match = outp.port_id == inp.port_id;
        }
      } else {
        if (outp.audio_channel == "FL" && inp.audio_channel == "PROBE_FL") {
          ports_match = true;
        }

        if (outp.audio_channel == "FR" && inp.audio_channel == "PROBE_FR") {
          ports_match = true;
        }
      }

      if (ports_match) {
        pw_properties* props = pw_properties_new(nullptr, nullptr);

        pw_properties_set(props, PW_KEY_LINK_PASSIVE, (link_passive) ? "true" : "false");
        pw_properties_set(props, PW_KEY_OBJECT_LINGER, "false");
        pw_properties_set(props, PW_KEY_LINK_OUTPUT_NODE, util::to_string(output_node_id).c_str());
        pw_properties_set(props, PW_KEY_LINK_OUTPUT_PORT, util::to_string(outp.id).c_str());
        pw_properties_set(props, PW_KEY_LINK_INPUT_NODE, util::to_string(input_node_id).c_str());
        pw_properties_set(props, PW_KEY_LINK_INPUT_PORT, util::to_string(inp.id).c_str());

        lock();

        auto* proxy = static_cast<pw_proxy*>(
            pw_core_create_object(core, "link-factory", PW_TYPE_INTERFACE_Link, PW_VERSION_LINK, &props->dict, 0));

        pw_properties_free(props);

        if (proxy == nullptr) {
          util::warning("failed to link the node " + util::to_string(output_node_id) + " to " +
                        util::to_string(input_node_id));

          unlock();

          return list;
        }

        sync_wait_unlock();

        list.push_back(proxy);
      }
    }
  }

  return list;
}

void Manager::lock() const {
  pw_thread_loop_lock(thread_loop);
}

void Manager::unlock() const {
  pw_thread_loop_unlock(thread_loop);
}

void Manager::sync_wait_unlock() const {
  pw_core_sync(core, PW_ID_CORE, 0);  // NOLINT

  pw_thread_loop_wait(thread_loop);

  pw_thread_loop_unlock(thread_loop);
}

auto Manager::wait_full() const -> int {
  timespec abstime;

  pw_thread_loop_get_time(thread_loop, &abstime, 30 * SPA_NSEC_PER_SEC);

  return pw_thread_loop_timed_wait_full(thread_loop, &abstime);
}

void Manager::destroy_object(const int& id) const {
  lock();

  pw_registry_destroy(registry, id);  // NOLINT

  sync_wait_unlock();
}

void Manager::destroy_links(const std::vector<pw_proxy*>& list) const {
  for (auto* proxy : list) {
    if (proxy != nullptr) {
      lock();

      pw_proxy_destroy(proxy);

      sync_wait_unlock();
    }
  }
}

/*
  Function inspired by code present in PipeWire's sources:
  https://gitlab.freedesktop.org/pipewire/pipewire/-/blob/master/spa/include/spa/utils/json.h#L350
*/

auto Manager::json_object_find(const char* obj, const char* key, char* value, const size_t& len) -> int {
  const char* v = nullptr;

  std::array<spa_json, 2U> sjson{};
  std::array<char, 128U> res{};

  spa_json_init(sjson.data(), obj, strlen(obj));

  if (spa_json_enter_object(sjson.data(), sjson.data() + 1) <= 0) {
    return -EINVAL;
  }

  while (spa_json_get_string(sjson.data() + 1, res.data(), res.size() * sizeof(char) - 1) > 0) {
    if (std::strcmp(res.data(), key) == 0) {
      if (spa_json_get_string(sjson.data() + 1, value, static_cast<int>(len)) <= 0) {
        continue;
      }

      return 0;
    }

    if (spa_json_next(sjson.data() + 1, &v) <= 0) {
      break;
    }
  }

  return -ENOENT;
}

}  // namespace pw