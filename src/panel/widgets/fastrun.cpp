#include "fastrun.hpp"
#include <map>
#include <iostream>
#include <gtkmm/button.h>
#include <glibmm/spawn.h>

WfFastRunCmd::WfFastRunCmd(command_info cmd) : Gtk::Button()
{
    icon.set_from_icon_name(cmd.icon, Gtk::ICON_SIZE_DND);
    label.set_label(cmd.name);
    box.add(icon);
    box.add(label);
    icon.show();
    label.show();

    add(box);
    box.show();

    // Unfortunately tooltips don't work well in GTK3 popovers
    // https://gitlab.gnome.org/GNOME/gtk/issues/1708
    // set_tooltip_text(cmd.cmd);
    get_style_context()->add_class("flat");
    signal_clicked().connect_notify([=]
    {
        Glib::spawn_command_line_async(cmd.cmd);
    });
}

void WayfireFastRun::init(Gtk::HBox *container)
{
    button = std::make_unique<WayfireMenuButton>("panel");
    icon.set_from_icon_name("system-run", Gtk::ICON_SIZE_LARGE_TOOLBAR);
    icon.show();
    button->add(icon);

    auto popover = button->get_popover();
    popover->add(button_box);

    container->pack_start(*button, false, false);

    button->show_all();

    handle_config_reload();
}

static bool begins_with(const std::string & str, const std::string & prefix)
{
    return str.substr(0, prefix.size()) == prefix;
}

void WayfireFastRun::handle_config_reload()
{
    for (auto child : button_box.get_children())
    {
        button_box.remove(*child);
    }

    auto section = WayfireShellApp::get().config.get_section("panel");
    const std::string command_prefix = "fastrun_cmd_";
    const std::string icon_prefix    = "fastrun_icon_";
    const std::string name_prefix    = "fastrun_name_";

    std::map<std::string, command_info> commands_info;

    for (auto opt : section->get_registered_options())
    {
        if (begins_with(opt->get_name(), command_prefix) && !opt->get_value_str().empty())
        {
            auto cmd_name = opt->get_name().substr(command_prefix.size());
            commands_info.insert({cmd_name, {opt->get_value_str(), "system-run", opt->get_value_str()}});
        }

        if (begins_with(opt->get_name(), icon_prefix))
        {
            auto cmd_name = opt->get_name().substr(icon_prefix.size());
            if (commands_info.count(cmd_name))
            {
                commands_info[cmd_name].icon = opt->get_value_str();
            } else
            {
                std::cout << "There is no fastrun command " << cmd_name <<
                    ", the command itself should be declared before its icon.\n";
            }
        }

        if (begins_with(opt->get_name(), name_prefix))
        {
            auto cmd_name = opt->get_name().substr(name_prefix.size());
            if (commands_info.count(cmd_name))
            {
                commands_info[cmd_name].name = opt->get_value_str();
            } else
            {
                std::cout << "There is no fastrun command " << cmd_name <<
                    ", the command itself should be declared before its 'pretty name'.\n";
            }
        }
    }

    for (const auto & [_, cmd] : commands_info)
    {
        auto command = new WfFastRunCmd(cmd);
        button_box.add(*command);
    }

    button_box.show_all();
}
