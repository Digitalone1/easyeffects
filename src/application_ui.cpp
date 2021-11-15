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

#include "application_ui.hpp"

namespace ui::application_window {

using namespace std::string_literals;

auto constexpr log_tag = "application_ui: ";

struct _ApplicationWindow {
  AdwWindow parent_instance{};

  AdwViewStack* stack = nullptr;

  GtkMenuButton* presets_menu_button = nullptr;

  GtkToggleButton* bypass_button = nullptr;

  ui::presets_menu::PresetsMenu* presetsMenu = nullptr;

  int width = -1;
  int height = -1;
  bool maximized = false;
  bool fullscreen = false;

  GSettings* settings = nullptr;

  GApplication* gapp = nullptr;

  GtkIconTheme* icon_theme = nullptr;

  std::vector<gulong> gconnections;
};

G_DEFINE_TYPE(ApplicationWindow, application_window, ADW_TYPE_APPLICATION_WINDOW)

void init_theme_color(ApplicationWindow* self) {
  if (g_settings_get_boolean(self->settings, "use-dark-theme") == 0) {
    adw_style_manager_set_color_scheme(adw_style_manager_get_default(), ADW_COLOR_SCHEME_FORCE_LIGHT);
  } else {
    adw_style_manager_set_color_scheme(adw_style_manager_get_default(), ADW_COLOR_SCHEME_FORCE_DARK);
  }
}

auto setup_icon_theme() -> GtkIconTheme* {
  auto* icon_theme = gtk_icon_theme_get_for_display(gdk_display_get_default());

  if (icon_theme == nullptr) {
    util::warning(log_tag + "can't retrieve the icon theme in use on the system. App icons won't be shown."s);

    return nullptr;
  }

  auto* name = gtk_icon_theme_get_theme_name(icon_theme);

  if (name == nullptr) {
    util::debug(log_tag + "Icon Theme detected, but the name is empty"s);
  } else {
    util::debug(log_tag + "Icon Theme "s + name + " detected"s);
  }

  gtk_icon_theme_add_resource_path(icon_theme, "/com/github/wwmm/easyeffects/icons");

  return icon_theme;
}

void constructed(GObject* object) {
  auto* self = EE_APP_WINDOW(object);

  self->maximized = g_settings_get_boolean(self->settings, "window-maximized") != 0;
  self->fullscreen = g_settings_get_boolean(self->settings, "window-fullscreen") != 0;
  self->width = g_settings_get_int(self->settings, "window-width");
  self->height = g_settings_get_int(self->settings, "window-height");

  gtk_window_set_default_size(GTK_WINDOW(self), self->width, self->height);

  if (self->maximized) {
    gtk_window_maximize(GTK_WINDOW(self));
  }

  if (self->fullscreen) {
    gtk_window_fullscreen(GTK_WINDOW(self));
  }

  G_OBJECT_CLASS(application_window_parent_class)->constructed(object);
}

void size_allocate(GtkWidget* widget, int width, int height, int baseline) {
  auto* self = EE_APP_WINDOW(widget);

  GTK_WIDGET_CLASS(application_window_parent_class)->size_allocate(widget, width, height, baseline);

  if (!self->maximized && !self->fullscreen) {
    gtk_window_get_default_size(GTK_WINDOW(self), &self->width, &self->height);
  }
}

void surface_state_changed(GtkWidget* widget) {
  auto* self = EE_APP_WINDOW(widget);

  GdkToplevelState new_state = GDK_TOPLEVEL_STATE_MAXIMIZED;

  new_state = gdk_toplevel_get_state(GDK_TOPLEVEL(gtk_native_get_surface(GTK_NATIVE(widget))));

  self->maximized = (new_state & GDK_TOPLEVEL_STATE_MAXIMIZED) != 0;
  self->fullscreen = (new_state & GDK_TOPLEVEL_STATE_FULLSCREEN) != 0;
}

void realize(GtkWidget* widget) {
  GTK_WIDGET_CLASS(application_window_parent_class)->realize(widget);

  g_signal_connect_swapped(gtk_native_get_surface(GTK_NATIVE(widget)), "notify::state",
                           G_CALLBACK(surface_state_changed), widget);

  auto* self = EE_APP_WINDOW(widget);

  /*
    Getting the gapp pointer here because it is not defined when "init" is called
  */

  self->gapp = G_APPLICATION(gtk_window_get_application(GTK_WINDOW(widget)));

  ui::presets_menu::setup(self->presetsMenu, app::EE_APP(self->gapp));
}

void unrealize(GtkWidget* widget) {
  g_signal_handlers_disconnect_by_func(gtk_native_get_surface(GTK_NATIVE((widget))),
                                       reinterpret_cast<gpointer>(surface_state_changed), widget);

  GTK_WIDGET_CLASS(application_window_parent_class)->unrealize(widget);
}

void dispose(GObject* object) {
  auto* self = EE_APP_WINDOW(object);

  for (auto& handler_id : self->gconnections) {
    g_signal_handler_disconnect(self->settings, handler_id);
  }

  g_settings_set_int(self->settings, "window-width", self->width);
  g_settings_set_int(self->settings, "window-height", self->height);
  g_settings_set_boolean(self->settings, "window-maximized", static_cast<gboolean>(self->maximized));
  g_settings_set_boolean(self->settings, "window-fullscreen", static_cast<gboolean>(self->fullscreen));

  if (g_settings_get_boolean(self->settings, "shutdown-on-window-close") != 0 &&
      (g_application_get_flags(self->gapp) & G_APPLICATION_IS_SERVICE) != 0) {
    g_application_release(self->gapp);
  }

  util::debug(log_tag + "destroyed"s);

  G_OBJECT_CLASS(application_window_parent_class)->dispose(object);
}

void application_window_class_init(ApplicationWindowClass* klass) {
  auto* object_class = G_OBJECT_CLASS(klass);
  auto* widget_class = GTK_WIDGET_CLASS(klass);

  object_class->constructed = constructed;
  object_class->dispose = dispose;

  widget_class->size_allocate = size_allocate;
  widget_class->realize = realize;
  widget_class->unrealize = unrealize;

  gtk_widget_class_set_template_from_resource(widget_class, "/com/github/wwmm/easyeffects/ui/application_window.ui");

  gtk_widget_class_bind_template_child(widget_class, ApplicationWindow, stack);
  gtk_widget_class_bind_template_child(widget_class, ApplicationWindow, presets_menu_button);
  gtk_widget_class_bind_template_child(widget_class, ApplicationWindow, bypass_button);
}

void application_window_init(ApplicationWindow* self) {
  gtk_widget_init_template(GTK_WIDGET(self));

  self->width = -1;
  self->height = -1;
  self->maximized = false;
  self->fullscreen = false;

  self->settings = g_settings_new("com.github.wwmm.easyeffects");

  init_theme_color(self);

  self->icon_theme = setup_icon_theme();

  self->presetsMenu = presets_menu::create();

  gtk_menu_button_set_popover(self->presets_menu_button, GTK_WIDGET(self->presetsMenu));

  g_settings_bind(self->settings, "bypass", self->bypass_button, "active", G_SETTINGS_BIND_DEFAULT);

  self->gconnections.push_back(g_signal_connect(
      self->settings, "changed::use-dark-theme",
      G_CALLBACK(+[](GSettings* settings, char* key, ApplicationWindow* self) { init_theme_color(self); }), self));
}

auto create(GApplication* gapp) -> ApplicationWindow* {
  return static_cast<ApplicationWindow*>(g_object_new(EE_TYPE_APPLICATION_WINDOW, "application", gapp, nullptr));
}

}  // namespace ui::application_window

ApplicationUi::ApplicationUi(BaseObjectType* cobject,
                             const Glib::RefPtr<Gtk::Builder>& builder,
                             Application* application)
    : Gtk::ApplicationWindow(cobject), app(application), settings(app->settings) {
  apply_css_style("custom.css");

  // loading builder widgets

  stack = builder->get_widget<Gtk::Stack>("stack");
  presets_menu_button = builder->get_widget<Gtk::MenuButton>("presets_menu_button");
  toggle_output = builder->get_widget<Gtk::ToggleButton>("toggle_output");
  toggle_input = builder->get_widget<Gtk::ToggleButton>("toggle_input");
  toggle_pipe_info = builder->get_widget<Gtk::ToggleButton>("toggle_pipe_info");

  auto icon_theme = setup_icon_theme();

  soe_ui = StreamOutputEffectsUi::add_to_stack(stack, app->soe.get(), icon_theme);
  sie_ui = StreamInputEffectsUi::add_to_stack(stack, app->sie.get(), icon_theme);
  // pipe_info_ui = PipeInfoUi::add_to_stack(stack, app->pm.get(), app->presets_manager.get());

  soe_ui->set_transient_window(this);
  sie_ui->set_transient_window(this);

  // signals

  toggle_output->signal_toggled().connect([&, this]() {
    if (toggle_output->get_active()) {
      stack->get_pages()->select_item(0, true);
    }
  });

  toggle_input->signal_toggled().connect([&, this]() {
    if (toggle_input->get_active()) {
      stack->get_pages()->select_item(1, true);
    }
  });

  toggle_pipe_info->signal_toggled().connect([&, this]() {
    if (toggle_pipe_info->get_active()) {
      stack->get_pages()->select_item(2, true);
    }
  });
}

ApplicationUi::~ApplicationUi() {
  for (auto& c : connections) {
    c.disconnect();
  }

  util::debug(log_tag + "destroyed");
}

auto ApplicationUi::create(Application* app_this) -> ApplicationUi* {
  const auto builder = Gtk::Builder::create_from_resource("/com/github/wwmm/easyeffects/ui/application_window.ui");

  return Gtk::Builder::get_widget_derived<ApplicationUi>(builder, "ApplicationUi", app_this);
}

void ApplicationUi::apply_css_style(const std::string& css_file_name) {
  auto provider = Gtk::CssProvider::create();

  provider->load_from_resource("/com/github/wwmm/easyeffects/ui/" + css_file_name);

  const auto display = Gdk::Display::get_default();
  const auto priority = GTK_STYLE_PROVIDER_PRIORITY_APPLICATION;

  Gtk::StyleContext::add_provider_for_display(display, provider, priority);
}

auto ApplicationUi::setup_icon_theme() -> Glib::RefPtr<Gtk::IconTheme> {
  try {
    Glib::RefPtr<Gtk::IconTheme> ic_theme = Gtk::IconTheme::get_for_display(Gdk::Display::get_default());

    const auto icon_theme_name = ic_theme->get_theme_name();

    if (icon_theme_name.empty()) {
      util::debug("application_ui: Icon Theme detected, but the name is empty");
    } else {
      util::debug("application_ui: Icon Theme " + icon_theme_name.raw() + " detected");
    }

    ic_theme->add_resource_path("/com/github/wwmm/easyeffects/icons");

    return ic_theme;
  } catch (...) {
    util::warning("application_ui: can't retrieve the icon theme in use on the system. App icons won't be shown.");

    return nullptr;
  }
}
