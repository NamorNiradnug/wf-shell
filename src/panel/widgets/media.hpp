#ifndef WIDGETS_MEDIA_HPP
#define WIDGETS_MEDIA_HPP

#include "../widget.hpp"
#include "wf-popover.hpp"

#include <gtkmm/image.h>
#include <gtkmm/label.h>
#include <gtkmm/scale.h>
#include <gtkmm/stack.h>
#include <gtkmm/stackswitcher.h>

#include <giomm/dbusproxy.h>

class SinglePlayerWidget : public Gtk::VBox
{
    private:
    Gtk::Label title;
    Gtk::Image cover;
    Gtk::Scale time_scale;
    Gtk::HBox control;

    Glib::RefPtr<Gio::DBus::Proxy> proxy, player_proxy;

    void updateTitle();
    void updateCover();
    void playPause();
    void updatePosition();

    public:
    SinglePlayerWidget(Glib::RefPtr<Gio::DBus::Proxy> &proxy, Glib::RefPtr<Gio::DBus::Proxy> &player_proxy);
};

class WayfireMedia : public WayfireWidget
{
    std::unique_ptr<WayfireMenuButton> button;
    Gtk::Image icon;

    Gtk::HBox layout;
    Gtk::StackSwitcher switcher;
    Gtk::Stack players;
    std::vector<std::unique_ptr<SinglePlayerWidget>> players_vector;

    Glib::RefPtr<Gio::DBus::Proxy> dbus_proxy;

    void checkAndAddPlayer(const Glib::ustring &name);

    public:
    void init(Gtk::HBox *container) override;
    ~WayfireMedia() override;
};

#endif
