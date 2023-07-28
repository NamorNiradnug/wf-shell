#ifndef COMMAND_OUTPUT_HPP
#define COMMAND_OUTPUT_HPP

#include "../widget.hpp"
#include "wf-popover.hpp"

#include <gtkmm/image.h>
#include <gtkmm/scrolledwindow.h>

#include <wayfire/config/compound-option.hpp>

class WfCommandOutputButtons : public WayfireWidget
{
    struct CommandOutput : public Gtk::Button
    {
        sigc::connection timeout_connection;

        Gtk::Box box;
        Gtk::Image icon;
        Gtk::Label output;

        WfOption<int> max_chars_opt{"panel/commands_output_max_chars"};

        void init();

        CommandOutput(const std::string & name, const std::string & command,
            const std::string & icon_name, int period, int icon_size,
            const std::string& icon_position);
        CommandOutput(CommandOutput&&) = delete;
        CommandOutput(const CommandOutput&) = delete;
        CommandOutput& operator =(CommandOutput&&) = delete;
        CommandOutput& operator =(const CommandOutput&) = delete;

        ~CommandOutput() override
        {
            timeout_connection.disconnect();
        }
    };

    Gtk::HBox box;
    std::vector<std::unique_ptr<CommandOutput>> buttons;

    WfOption<wf::config::compound_list_t<std::string, std::string,
        int, int, std::string>> commands_list_opt{"panel/commands"};

  public:
    void init(Gtk::HBox *container) override;
    void update_buttons();
};

#endif /* end of include guard: COMMAND_OUTPUT_HPP */
