// Copyright 2020 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.

#pragma once

#include <ftxui/component/event.hpp>  // for Event
#include <ftxui/screen/screen.hpp>    // for Screen
#include <functional>                 // for function
#include <memory>                     // for shared_ptr
#include <string>                     // for string

// ReSharper disable once CppInconsistentNaming
namespace ftxui {
    class ComponentBase;
    struct Event;

    using Component = std::shared_ptr<ComponentBase>;

    class NaiveUI final : public Screen {
        static void install();
        static void uninstall();

        void draw(const Component& component);

        std::string mSetCursorPosition;
        std::string mResetCursorPosition;
        bool mPreviousFrameResized = false;

    public:
        NaiveUI();
        ~NaiveUI();
        NaiveUI(const NaiveUI&) = delete;
        NaiveUI(NaiveUI&&) = delete;
        NaiveUI& operator=(const NaiveUI&) = delete;
        NaiveUI& operator=(NaiveUI&&) = delete;

        void render(const Component& component);
    };

}  // namespace ftxui
