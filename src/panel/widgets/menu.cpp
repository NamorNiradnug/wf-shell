#include <dirent.h>

#include <cassert>
#include <giomm/icon.h>
#include <glibmm/spawn.h>
#include <iostream>
#include <memory>
#include <gtk-layer-shell.h>
#include <gtk/gtk.h>
#include <gtkmm/cssprovider.h>
#include <gtkmm/stylecontext.h>

#include "menu.hpp"
#include "gtk-utils.hpp"
#include "launchers.hpp"
#include "wf-autohide-window.hpp"

const std::string default_icon = ICONDIR "/wayfire.png";

WfMenuMenuItem::WfMenuMenuItem(WayfireMenu *_menu, const AppInfo & app) :
    menu(_menu), m_app_info(app)
{
    image.set((const Glib::RefPtr<const Gio::Icon>&)app->get_icon(), Gtk::ICON_SIZE_LARGE_TOOLBAR);
    image.set_pixel_size(48);

    label.set_text(app->get_name());
    label.set_ellipsize(Pango::ELLIPSIZE_END);
    pack_start(image, false, false);
    pack_end(label, false, false);

    get_style_context()->add_class("flat");
}

void WfMenuMenuItem::activate()
{
    m_app_info->launch(std::vector<Glib::RefPtr<Gio::File>>());
    menu->hide_menu();
}

/* Fuzzy search for pattern in text. We use a greedy algorithm as follows:
 * As long as the pattern isn't matched, try to match the leftmost unmatched
 * character in pattern with the first occurence of this character after the
 * partial match. In the end, we just check if we successfully matched all
 * characters */
static bool fuzzy_match(const Glib::ustring& text, const Glib::ustring& pattern)
{
    size_t i = 0, // next character in pattern to match
        j    = 0; // the first unmatched character in text

    while (i < pattern.length() && j < text.length())
    {
        /* Found a match, advance both pointers */
        if (pattern[i] == text[j])
        {
            ++i;
            ++j;
        } else
        {
            /* Try to match current unmatched character in pattern with the next
             * character in text */
            ++j;
        }
    }

    /* If this happens, then we have already matched all characters */
    return i == pattern.length();
}

bool WfMenuMenuItem::fuzzy_match(const Glib::ustring & pattern)
{
    Glib::ustring name = m_app_info->get_name();
    Glib::ustring long_name = m_app_info->get_display_name();
    Glib::ustring progr     = m_app_info->get_executable();

    auto pattern_lower = pattern.lowercase();

    return ::fuzzy_match(progr.lowercase(), pattern) ||
           ::fuzzy_match(name.lowercase(), pattern) ||
           ::fuzzy_match(long_name.lowercase(), pattern);
}

bool WfMenuMenuItem::matches(const Glib::ustring & pattern)
{
    Glib::ustring name = m_app_info->get_name();
    Glib::ustring long_name = m_app_info->get_display_name();
    Glib::ustring progr     = m_app_info->get_executable();
    Glib::ustring descr     = m_app_info->get_description();

    Glib::ustring text = name.lowercase() + "$" +
        long_name.lowercase() + "$" + progr.lowercase() + "$" +
        descr.lowercase();

    return text.find(pattern.lowercase()) != Glib::ustring::npos;
}

bool WfMenuMenuItem::operator <(const WfMenuMenuItem& other)
{
    return Glib::ustring(m_app_info->get_name()).lowercase() <
           Glib::ustring(other.m_app_info->get_name()).lowercase();
}

void WayfireMenu::load_menu_item(const AppInfo & app_info)
{
    if (!app_info)
    {
        return;
    }

    auto desktop_app_info = Glib::RefPtr<Gio::DesktopAppInfo>::cast_dynamic(app_info);
    if (desktop_app_info && desktop_app_info->get_nodisplay())
    {
        return;
    }

    auto name = app_info->get_name();
    auto exec = app_info->get_executable();
    /* If we don't have the following, then the entry won't be useful anyway,
     * so we should skip it */
    if (name.empty() || !app_info->get_icon() || exec.empty())
    {
        return;
    }

    /* Already created such a launcher, skip */
    if (loaded_apps.count({name, exec}))
    {
        return;
    }

    loaded_apps.insert({name, exec});

    items.push_back(std::make_unique<WfMenuMenuItem>(this, app_info));
    flowbox.add(*items.back());
}

static bool ends_with(std::string text, std::string pattern)
{
    if (text.length() < pattern.length())
    {
        return false;
    }

    return text.substr(text.length() - pattern.length()) == pattern;
}

void WayfireMenu::load_menu_items_from_dir(std::string path)
{
    /* Expand path */
    auto dir = opendir(path.c_str());
    if (!dir)
    {
        return;
    }

    /* Iterate over all files in the directory */
    dirent *file;
    while ((file = readdir(dir)) != 0)
    {
        /* Skip hidden files and folders */
        if (file->d_name[0] == '.')
        {
            continue;
        }

        auto fullpath = path + "/" + file->d_name;

        if (ends_with(fullpath, ".desktop"))
        {
            load_menu_item(Gio::DesktopAppInfo::create_from_filename(fullpath));
        }
    }
}

void WayfireMenu::load_menu_items_all()
{
    std::string home_dir = getenv("HOME");
    auto app_list = Gio::AppInfo::get_all();
    for (auto app : app_list)
    {
        load_menu_item(app);
    }

    load_menu_items_from_dir(home_dir + "/Desktop");
}

void WayfireMenu::on_search_changed()
{
    fuzzy_filter  = false;
    count_matches = 0;
    flowbox.invalidate_filter();

    /* We got no matches, try to fuzzy-match */
    if ((count_matches <= 0) && fuzzy_search_enabled)
    {
        fuzzy_filter = true;
        flowbox.invalidate_filter();
    }
}

bool WayfireMenu::on_filter(Gtk::FlowBoxChild *child)
{
    auto button = dynamic_cast<WfMenuMenuItem*>(child->get_child());
    assert(button);

    auto text = search_entry.get_text();
    bool does_match = this->fuzzy_filter ?
        button->fuzzy_match(text) : button->matches(text);

    if (does_match)
    {
        this->count_matches++;
        return true;
    }

    return false;
}

bool WayfireMenu::on_sort(Gtk::FlowBoxChild *a, Gtk::FlowBoxChild *b)
{
    auto b1 = dynamic_cast<WfMenuMenuItem*>(a->get_child());
    auto b2 = dynamic_cast<WfMenuMenuItem*>(b->get_child());
    assert(b1 && b2);

    return *b2 < *b1;
}

void WayfireMenu::on_popover_shown()
{
    search_entry.grab_focus();
}

bool WayfireMenu::update_icon()
{
    std::string icon;
    int size = menu_size / LAUNCHERS_ICON_SCALE;
    if (((std::string)menu_icon).empty())
    {
        icon = default_icon;
    } else
    {
        icon = menu_icon;
    }

    button->set_size_request(size, 0);

    auto ptr_pbuff = load_icon_pixbuf_safe(icon,
        size * main_image.get_scale_factor());

    if (!ptr_pbuff.get())
    {
        std::cout << "Loading default icon: " << default_icon << std::endl;
        ptr_pbuff = load_icon_pixbuf_safe(default_icon,
            size * main_image.get_scale_factor());
    }

    if (!ptr_pbuff)
    {
        return false;
    }

    set_image_pixbuf(main_image, ptr_pbuff, main_image.get_scale_factor());
    return true;
}

void WayfireMenu::update_popover_layout()
{
    /* First time updating layout, need to setup everything */
    if (popover_layout_box.get_parent() == nullptr)
    {
        button->get_popover()->add(popover_layout_box);

        flowbox.set_valign(Gtk::ALIGN_START);
        flowbox.set_homogeneous(true);
        // TODO(NamorNiradnug) make children per line number configurable
        flowbox.set_min_children_per_line(4);
        flowbox.set_min_children_per_line(4);
        flowbox.set_selection_mode(Gtk::SELECTION_BROWSE);
        flowbox.signal_child_activated().connect_notify([] (Gtk::FlowBoxChild *child)
        {
            auto *menu_item = dynamic_cast<WfMenuMenuItem*>(child->get_child());
            if (menu_item != nullptr)
            {
                menu_item->activate();
            }
        });
        flowbox.set_sort_func(sigc::mem_fun(this, &WayfireMenu::on_sort));
        flowbox.set_filter_func(sigc::mem_fun(this, &WayfireMenu::on_filter));

        flowbox_container.add(bottom_pad);
        flowbox_container.add(flowbox);

        scrolled_window.set_min_content_width(int(menu_min_content_width));
        scrolled_window.set_min_content_height(int(menu_min_content_height));
        scrolled_window.add(flowbox_container);

        search_entry.property_margin().set_value(20);
        search_entry.signal_search_changed().connect_notify(
            sigc::mem_fun(this, &WayfireMenu::on_search_changed));
        popover_layout_box.signal_key_press_event().connect(sigc::mem_fun(search_entry,
            &Gtk::SearchEntry::handle_event));
    } else
    {
        /* Layout was already initialized, make sure to remove widgets before
         * adding them again */
        popover_layout_box.remove(search_entry);
        popover_layout_box.remove(scrolled_window);
    }

    if ((std::string)panel_position == WF_WINDOW_POSITION_TOP)
    {
        popover_layout_box.pack_start(search_entry);
        popover_layout_box.pack_start(scrolled_window);
    } else
    {
        popover_layout_box.pack_start(scrolled_window);
        popover_layout_box.pack_start(search_entry);
    }

    popover_layout_box.show_all();
}

void WayfireLogoutUI::on_cancel_click()
{
    ui.hide();
    bg.hide();
}

#define LOGOUT_BUTTON_SIZE  125
#define LOGOUT_BUTTON_MARGIN 10

void WayfireLogoutUI::create_logout_ui_button(WayfireLogoutUIButton& button, const char *icon,
    const char *label,
    const WfOption<std::string>& command, Gtk::Box& layout)
{
    button.set_size_request(LOGOUT_BUTTON_SIZE, LOGOUT_BUTTON_SIZE);
    button.image.set_from_icon_name(icon, Gtk::ICON_SIZE_DIALOG);
    button.label.set_text(label);
    button.layout.pack_start(button.image, true, false);
    button.layout.pack_start(button.label, true, false);
    button.add(button.layout);
    button.signal_clicked().connect([=, &command]
    {
        bg.hide();
        ui.hide();
        Glib::spawn_command_line_async(command);
    });
    layout.pack_start(button, true, false);
}

WayfireLogoutUI::WayfireLogoutUI()
{
    create_logout_ui_button(suspend, "emblem-synchronizing", "Suspend", suspend_command, top_layout);
    create_logout_ui_button(hibernate, "weather-clear-night", "Hibernate", hibernate_command, top_layout);
    create_logout_ui_button(switchuser, "system-users", "Switch User", switchuser_command, top_layout);

    create_logout_ui_button(logout, "system-log-out", "Log Out", logout_command, middle_layout);
    create_logout_ui_button(reboot, "system-reboot", "Reboot", reboot_command, middle_layout);
    create_logout_ui_button(shutdown, "system-shutdown", "Shut Down", shutdown_command, middle_layout);

    cancel.set_size_request(100, 50);
    cancel.set_label("Cancel");
    bottom_layout.pack_start(cancel, true, false);
    cancel.signal_clicked().connect_notify(
        sigc::mem_fun(this, &WayfireLogoutUI::on_cancel_click));

    top_layout.set_spacing(LOGOUT_BUTTON_MARGIN);
    middle_layout.set_spacing(LOGOUT_BUTTON_MARGIN);
    bottom_layout.set_spacing(LOGOUT_BUTTON_MARGIN);
    main_layout.set_spacing(LOGOUT_BUTTON_MARGIN);
    main_layout.add(top_layout);
    main_layout.add(middle_layout);
    main_layout.add(bottom_layout);
    /* Work around spacing bug for immediate child of window */
    hspacing_layout.pack_start(main_layout, true, false, LOGOUT_BUTTON_MARGIN);
    vspacing_layout.pack_start(hspacing_layout, true, false, LOGOUT_BUTTON_MARGIN);
    /* Make surfaces layer shell */
    gtk_layer_init_for_window(bg.gobj());
    gtk_layer_set_layer(bg.gobj(), GTK_LAYER_SHELL_LAYER_TOP);
    gtk_layer_set_exclusive_zone(bg.gobj(), -1);
    gtk_layer_init_for_window(ui.gobj());
    gtk_layer_set_layer(ui.gobj(), GTK_LAYER_SHELL_LAYER_OVERLAY);
    ui.add(vspacing_layout);
    bg.set_opacity(0.5);
    auto css_provider  = Gtk::CssProvider::create();
    auto style_context = Gtk::StyleContext::create();
    bg.set_name("logout_background");
    css_provider->load_from_data("window#logout_background { background-color: black; }");
    style_context->add_provider_for_screen(Gdk::Screen::get_default(),
        css_provider, GTK_STYLE_PROVIDER_PRIORITY_USER);
}

void WayfireMenu::on_logout_click()
{
    button->get_popover()->hide();
    if (!menu_logout_command.value().empty())
    {
        g_spawn_command_line_async(std::string(menu_logout_command).c_str(), NULL);
        return;
    }

    /* If no command specified for logout, show our own logout window */
    logout_ui->ui.present();
    logout_ui->ui.show_all();
    logout_ui->bg.show_all();

    /* Set the background window to the same size of the screen */
    auto gdk_screen = Gdk::Screen::get_default();
    logout_ui->bg.set_size_request(gdk_screen->get_width(), gdk_screen->get_height());
}

void WayfireMenu::refresh()
{
    loaded_apps.clear();
    items.clear();
    for (auto child : flowbox.get_children())
    {
        gtk_widget_destroy(GTK_WIDGET(child->gobj()));
    }

    load_menu_items_all();
    flowbox.show_all();
}

static void app_info_changed(GAppInfoMonitor *gappinfomonitor, gpointer user_data)
{
    WayfireMenu *menu = (WayfireMenu*)user_data;

    menu->refresh();
}

void WayfireMenu::init(Gtk::HBox *container)
{
    menu_icon.set_callback([=] () { update_icon(); });
    menu_size.set_callback([=] () { update_icon(); });
    panel_position.set_callback([=] () { update_popover_layout(); });

    button = std::make_unique<WayfireMenuButton>("panel");
    button->add(main_image);
    button->get_popover()->set_constrain_to(Gtk::POPOVER_CONSTRAINT_NONE);
    button->get_popover()->signal_show().connect_notify(
        sigc::mem_fun(this, &WayfireMenu::on_popover_shown));
    button->get_popover()->signal_hide().connect_notify([this]
    {
        search_entry.set_text("");
    });

    if (!update_icon())
    {
        return;
    }

    button->property_scale_factor().signal_changed().connect(
        [=] () { update_icon(); });

    container->pack_start(hbox, false, false);
    hbox.pack_start(*button, false, false);

    logout_button.set_image_from_icon_name("system-shutdown", Gtk::ICON_SIZE_DIALOG);
    logout_button.signal_clicked().connect_notify(
        sigc::mem_fun(this, &WayfireMenu::on_logout_click));
    logout_button.property_margin().set_value(20);
    logout_button.set_margin_right(35);
    hbox_bottom.pack_end(logout_button, false, false);
    popover_layout_box.pack_end(hbox_bottom);
    popover_layout_box.pack_end(separator);

    logout_ui = std::make_unique<WayfireLogoutUI>();

    load_menu_items_all();
    update_popover_layout();

    app_info_monitor_changed_handler_id =
        g_signal_connect(app_info_monitor, "changed", G_CALLBACK(app_info_changed), this);

    hbox.show();
    main_image.show();
    button->show();
}

void WayfireMenu::hide_menu()
{
    button->set_active(false);
}
