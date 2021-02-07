#pragma once

#include "BaseWindow.h"

#include "IsAlive.h"

#include <gtkmm.h>

#include <functional>
#include <tuple>

namespace DV {

class RenameWindow : public BaseWindow, public Gtk::Window, public IsAlive {
public:
    using VerifyMethod = std::function<std::tuple<bool, std::string>(const std::string&)>;
    using ApplyMethod =  std::function<std::tuple<bool, std::string>(const std::string&)>;

    RenameWindow(const std::string& originalName, ApplyMethod applyNewName, VerifyMethod verifyNewName);
    ~RenameWindow();

protected:
    bool IsNewNameValid(const std::string& name);

    void _OnClose() override;

    void _OnApply();
    void _OnTextChanged();

protected:
    const std::string OriginalName;

    const ApplyMethod ApplyNew;
    const VerifyMethod Verifier;

    Gtk::Box MainBox;

    Gtk::Entry TextEntry;
    Gtk::Label ErrorMessage;

    Gtk::Button ApplyButton;
};


} // namespace DV
