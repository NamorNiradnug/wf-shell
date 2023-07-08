#ifndef TRAY_ITEM_HPP
#define TRAY_ITEM_HPP

#include <giomm.h>
#include <gtkmm/eventbox.h>
#include <gtkmm/hvbox.h>
#include <gtkmm/image.h>
#include <gtkmm/menu.h>

#include <optional>

class StatusNotifierItem : public Gtk::EventBox
{
    Glib::ustring dbus_name;

    Glib::RefPtr<Gio::DBus::Proxy> item_proxy;

    Gtk::Image icon;
    std::optional<Gtk::Menu> menu;

    gdouble distance_scrolled_x = 0;
    gdouble distance_scrolled_y = 0;

    template <typename T>
    T get_item_property(const Glib::ustring &name) const
    {
        Glib::VariantBase variant;
        item_proxy->get_cached_property(variant, name);
        return variant && variant.is_of_type(Glib::Variant<T>::variant_type())
                   ? Glib::VariantBase::cast_dynamic<Glib::Variant<T>>(variant).get()
                   : T{};
    }

    void init_widget();
    void init_menu();

    void handle_signal(const Glib::ustring &signal, const Glib::VariantContainerBase &params);

    void update_icon();
    void update_tooltip();

    void fetch_property(const Glib::ustring &property_name, const sigc::slot<void> &callback);

    public:
    explicit StatusNotifierItem(const Glib::ustring &service);
};

#endif
