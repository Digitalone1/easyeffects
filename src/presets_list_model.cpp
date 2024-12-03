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

#include "presets_list_model.hpp"
#include <qabstractitemmodel.h>
#include <qhash.h>
#include <qhashfunctions.h>
#include <qlist.h>
#include <qnamespace.h>
#include <qobject.h>
#include <qqml.h>
#include <qsortfilterproxymodel.h>
#include <qstringview.h>
#include <qtmetamacros.h>
#include <qtypes.h>
#include <qvariant.h>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>
#include "config.h"

ListModel::ListModel(QObject* parent, const ModelType& model_type)
    : QAbstractListModel(parent), proxy(new QSortFilterProxyModel(this)), model_type(model_type) {
  proxy->setSourceModel(this);

  switch (model_type) {
    case Local:
      proxy->setFilterRole(Roles::Name);
      proxy->setSortRole(Roles::Name);
      break;
    case Community:
      proxy->setFilterRole(Roles::Path);
      proxy->setSortRole(Roles::Path);
      break;
    case Autoloading:
      proxy->setFilterRole(Roles::DeviceDescription);
      proxy->setSortRole(Roles::DeviceDescription);
      break;
  }

  proxy->setDynamicSortFilter(true);
  proxy->sort(0);
}

int ListModel::rowCount(const QModelIndex& /*parent*/) const {
  return listPaths.size();
}

QHash<int, QByteArray> ListModel::roleNames() const {
  return {{Roles::Name, "name"},
          {Roles::Path, "path"},
          {Roles::PresetPackage, "presetPackage"},
          {Roles::DeviceName, "deviceName"},
          {Roles::DeviceDescription, "deviceDescription"},
          {Roles::DeviceProfile, "deviceProfile"},
          {Roles::DevicePreset, "devicePreset"}};
}

QVariant ListModel::data(const QModelIndex& index, int role) const {
  if (listPaths.empty()) {
    return "";
  }

  const auto it = std::next(listPaths.begin(), index.row());

  if (model_type == ModelType::Local || model_type == ModelType::Community) {
    switch (role) {
      case Roles::Name:
        return QString::fromStdString(it->stem().string());
      case Roles::Path:
        return QString::fromStdString(it->string());
      case Roles::PresetPackage:
        return QString::fromStdString(it->parent_path().stem().string());  // getting the parent folder
      default:
        return {};
    }
  }

  if (model_type == ModelType::Autoloading) {
    nlohmann::json json;

    std::ifstream is(*it);

    is >> json;

    switch (role) {
      case Roles::DeviceName:
        return QString::fromStdString(json.value("device", ""));
      case Roles::DeviceDescription:
        return QString::fromStdString(json.value("device-description", ""));
      case Roles::DeviceProfile:
        return QString::fromStdString(json.value("device-profile", ""));
      case Roles::DevicePreset:
        return QString::fromStdString(json.value("preset-name", ""));
      default:
        return {};
    }
  }

  return {};
}

void ListModel::append(const std::filesystem::path& path) {
  int pos = listPaths.empty() ? 0 : listPaths.size() - 1;

  beginInsertRows(QModelIndex(), pos, pos);

  listPaths.append(path);

  endInsertRows();

  emit dataChanged(index(0), index(listPaths.size() - 1));
}

void ListModel::remove(const QString& name) {
  qsizetype rowIndex = -1;

  for (qsizetype n = 0; n < listPaths.size(); n++) {
    if (listPaths[n].stem().string() == name.toStdString()) {
      rowIndex = n;
    }
  }

  if (rowIndex == -1) {
    return;
  }

  beginRemoveRows(QModelIndex(), rowIndex, rowIndex);

  listPaths.remove(rowIndex);

  endRemoveRows();

  emit dataChanged(index(0), index(listPaths.size() - 1));
}

void ListModel::remove(const int& rowIndex) {
  beginRemoveRows(QModelIndex(), rowIndex, rowIndex);

  listPaths.remove(rowIndex);

  endRemoveRows();

  emit dataChanged(index(0), index(listPaths.size() - 1));
}

void ListModel::remove(const std::filesystem::path& path) {
  qsizetype rowIndex = listPaths.indexOf(path);

  beginRemoveRows(QModelIndex(), rowIndex, rowIndex);

  listPaths.remove(rowIndex);

  endRemoveRows();

  emit dataChanged(index(0), index(listPaths.size() - 1));
}

void ListModel::reset() {
  beginResetModel();

  listPaths.clear();

  endResetModel();
}

void ListModel::begin_reset() {
  beginResetModel();
}

void ListModel::end_reset() {
  endResetModel();
}

auto ListModel::getList() -> QList<std::filesystem::path> {
  return listPaths;
}

QSortFilterProxyModel* ListModel::getProxy() {
  return proxy;
}

void ListModel::set_filter_role(const Roles& role) {
  proxy->setFilterRole(role);
}