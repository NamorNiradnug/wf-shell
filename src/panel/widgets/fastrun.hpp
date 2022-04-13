#ifndef WIDGETS_FASTRUN_HPP
#define WIDGETS_FASTRUN_HPP

#include "../widget.hpp"

#include <gtkmm/buttonbox.h>
#include <vector>
#include <gtkmm/image.h>
#include "wf-popover.hpp"

struct command_info
{
    std::string cmd, icon, name;
};

class WfFastRunCmd : public Gtk::Button
{
    Gtk::HBox box;
    Gtk::Image icon;
    Gtk::Label label;

    public:
    WfFastRunCmd(command_info cmd);
};

class WayfireFastRun : public WayfireWidget
{
    Gtk::Image icon;
    std::unique_ptr<WayfireMenuButton> button;
    std::vector<std::unique_ptr<WfFastRunCmd>> commands; 
    Gtk::VButtonBox button_box;

    public:
    void init(Gtk::HBox *container) override;
    void handle_config_reload() override;
};

#endif /* end of include guard: WIDGETS_CLOCK_HPP */
