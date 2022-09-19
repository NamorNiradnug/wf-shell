#include "media.hpp"

#include <iostream>
#include <utility>

#define MPRIS_PATH "/org/mpris/MediaPlayer2"
#define MPRIS_IFACE "org.mpris.MediaPlayer2"

static bool begins_with(const Glib::ustring &str, const Glib::ustring &prefix)
{
    return prefix.size() <= str.size() && str.substr(0, prefix.size()) == prefix;
}

SinglePlayerWidget::SinglePlayerWidget(Glib::RefPtr<Gio::DBus::Proxy> &proxy,
                                       Glib::RefPtr<Gio::DBus::Proxy> &player_proxy)
    : proxy(proxy), player_proxy(player_proxy)
{
    std::cout << proxy->property_g_name() << " " << proxy->property_g_interface_name() << '\n';
}

void WayfireMedia::checkAndAddPlayer(const Glib::ustring &name)
{
    if (!begins_with(name, "org.mpris.MediaPlayer2"))
        return;
    Gio::DBus::Proxy::create_for_bus(
        Gio::DBus::BUS_TYPE_SESSION, name, "/org/mpris/MediaPlayer2", MPRIS_IFACE,
        [=](Glib::RefPtr<Gio::AsyncResult> &result) {
            auto proxy = Gio::DBus::Proxy::create_finish(result);
            if (!proxy)
                return;
            Gio::DBus::Proxy::create_for_bus(
                Gio::DBus::BUS_TYPE_SESSION, name, "/org/mpris/MediaPlayer2", MPRIS_IFACE ".Player",
                [=, &proxy](Glib::RefPtr<Gio::AsyncResult> &result) {
                    auto player_proxy = Gio::DBus::Proxy::create_finish(result);
                    if (!player_proxy)
                        return;
                    players_vector.push_back(std::make_unique<SinglePlayerWidget>(proxy, player_proxy));
                    players.add(*players_vector.back());
                });
        });
}

void WayfireMedia::init(Gtk::HBox *container)
{
    dbus_proxy = Gio::DBus::Proxy::create_for_bus_sync(Gio::DBus::BUS_TYPE_SESSION, "org.freedesktop.DBus", "/",
                                                       "org.freedesktop.DBus");
    if (!dbus_proxy)
    {
        std::cerr << "Cannot connect to org.freedesktop.DBus.\n";
        return;
        // show to user that something went wrong
    }

    std::cout << dbus_proxy->call_sync("ListNames").get_type_string() << '\n';
    const auto &session_dbus_names =
        std::get<0>(Glib::VariantBase::cast_dynamic<Glib::Variant<std::tuple<std::vector<Glib::ustring>>>>(
                        dbus_proxy->call_sync("ListNames"))
                        .get());
    for (const auto &name : session_dbus_names)
    {
        checkAndAddPlayer(name);
    }

    dbus_proxy->signal_signal().connect([=](const Glib::ustring & /*sender*/, const Glib::ustring &signal,
                                            const Glib::VariantContainerBase &params) {
        if (signal == "NameAcqired")
        {
            auto name =
                std::get<0>(Glib::VariantBase::cast_dynamic<Glib::Variant<std::tuple<Glib::ustring>>>(params).get());
            checkAndAddPlayer(name);
        }
    });

    icon.set_from_icon_name("mediacontrol", Gtk::ICON_SIZE_LARGE_TOOLBAR);
    button = std::make_unique<WayfireMenuButton>("panel");
    button->add(icon);
    auto *popover = button->get_popover();
    popover->add(layout);
    container->add(*button);
}

WayfireMedia::~WayfireMedia() = default;
