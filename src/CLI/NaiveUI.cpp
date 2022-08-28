// Copyright 2020 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.

#include <Piper/Core/NaiveUI.hpp>
#include <csignal>                             // for signal, SIGABRT, SIGFPE, SIGILL, SIGINT, SIGSEGV, SIGTERM, SIGWINCH
#include <ftxui/component/component_base.hpp>  // for ComponentBase
#include <ftxui/component/receiver.hpp>        // for ReceiverImpl, MakeReceiver, Sender, SenderImpl, Receiver
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/node.hpp>         // for Node, Render
#include <ftxui/screen/terminal.hpp>  // for Size, Dimensions
#include <initializer_list>           // for initializer_list
#include <iostream>                   // for cout, ostream, basic_ostream, operator<<, endl, flush
#include <stack>                      // for stack
#include <vector>                     // for vector

#if defined(_WIN32)
#define DEFINE_CONSOLEV2_PROPERTIES
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#ifndef UNICODE
#error Must be compiled in UNICODE mode
#endif
#else
#include <termios.h>     // for tcsetattr, tcgetattr, cc_t
#endif

namespace ftxui {
    namespace {
        void flush() {
            // Emscripten doesn't implement flush. We interpret zero as flush.
            std::cout << '\0' << std::flush;
        }

        const std::string strCSI = "\x1b[";

        // DEC: Digital Equipment Corporation
        enum class DECMode {
            LineWrap = 7,
            Cursor = 25,
            MouseAnyEvent = 1003,
            MouseUtf8 = 1005,
            MouseSgrExtMode = 1006,
            AlternateScreen = 1049,
        };

        std::string serialize(const std::vector<DECMode>& parameters) {
            bool first = true;
            std::string out;
            for(DECMode parameter : parameters) {
                if(!first)
                    out += ";";
                out += std::to_string(static_cast<int>(parameter));
                first = false;
            }
            return out;
        }

        // DEC Private Mode Set (DECSET)
        std::string set(const std::vector<DECMode>& parameters) {
            return strCSI + "?" + serialize(parameters) + "h";
        }

        // DEC Private Mode Reset (DECRST)
        std::string reset(const std::vector<DECMode>& parameters) {
            return strCSI + "?" + serialize(parameters) + "l";
        }

        using SignalHandler = void(int);
        std::stack<std::function<void()>> onExitFunctions;

        void onExit(const int) {
            while(!onExitFunctions.empty()) {
                onExitFunctions.top()();
                onExitFunctions.pop();
            }
        }

        void installSignalHandler(const int sig, SignalHandler handler) {
            const auto oldSignalHandler = std::signal(sig, handler);
            onExitFunctions.push([&] { std::signal(sig, oldSignalHandler); });
        };

    }  // namespace

    NaiveUI::NaiveUI() : Screen(0, 0) {
        install();
    }

    NaiveUI::~NaiveUI() {

        uninstall();
        // Put cursor position at the end of the drawing.
        std::cout << mResetCursorPosition;
        // On final exit, keep the current drawing and reset cursor position one
        // line after it.
        std::cout << std::endl;
    }

    void NaiveUI::install() {
        // After uninstalling the new configuration, flush it to the terminal to
        // ensure it is fully applied:
        onExitFunctions.push([] { flush(); });

        // Install signal handlers to restore the terminal state on exit. The default
        // signal handlers are restored on exit.
        for(const int signal : { SIGTERM, SIGSEGV, SIGINT, SIGILL, SIGABRT, SIGFPE })
            installSignalHandler(signal, onExit);

            // Save the old terminal configuration and restore it on exit.
#if defined(_WIN32)
        // Enable VT processing on stdout and stdin
        const auto stdoutHandle = GetStdHandle(STD_OUTPUT_HANDLE);
        const auto stdinHandle = GetStdHandle(STD_INPUT_HANDLE);

        DWORD outMode = 0;
        DWORD inMode = 0;
        GetConsoleMode(stdoutHandle, &outMode);
        GetConsoleMode(stdinHandle, &inMode);
        onExitFunctions.push([=] { SetConsoleMode(stdoutHandle, outMode); });
        onExitFunctions.push([=] { SetConsoleMode(stdinHandle, inMode); });

        // https://docs.microsoft.com/en-us/windows/console/setconsolemode
        constexpr int enableVirtualTerminalProcessing = 0x0004;
        constexpr int disableNewlineAutoReturn = 0x0008;
        outMode |= enableVirtualTerminalProcessing;
        outMode |= disableNewlineAutoReturn;

        // https://docs.microsoft.com/en-us/windows/console/setconsolemode
        constexpr int enableLineInput = 0x0002;
        constexpr int enableEchoInput = 0x0004;
        constexpr int enableVirtualTerminalInput = 0x0200;
        constexpr int enableWindowInput = 0x0008;
        inMode &= ~enableEchoInput;
        inMode &= ~enableLineInput;
        inMode |= enableVirtualTerminalInput;
        inMode |= enableWindowInput;

        SetConsoleMode(stdinHandle, inMode);
        SetConsoleMode(stdoutHandle, outMode);
#else
        struct termios terminal;
        tcgetattr(STDIN_FILENO, &terminal);
        onExitFunctions.push([=] { tcsetattr(STDIN_FILENO, TCSANOW, &terminal); });

        terminal.c_lflag &= ~ICANON;  // Non canonique terminal.
        terminal.c_lflag &= ~ECHO;    // Do not print after a key press.
        terminal.c_cc[VMIN] = 0;
        terminal.c_cc[VTIME] = 0;

        tcsetattr(STDIN_FILENO, TCSANOW, &terminal);
#endif

        auto enable = [&](const std::vector<DECMode>& parameters) {
            std::cout << set(parameters);
            onExitFunctions.push([=] { std::cout << reset(parameters); });
        };

        auto disable = [&](const std::vector<DECMode>& parameters) {
            std::cout << reset(parameters);
            onExitFunctions.push([=] { std::cout << set(parameters); });
        };

        enable({
            DECMode::AlternateScreen,
        });

        disable({
            DECMode::Cursor,
            DECMode::LineWrap,
        });

        enable({
            DECMode::MouseAnyEvent,
            DECMode::MouseUtf8,
            DECMode::MouseSgrExtMode,
        });

        // After installing the new configuration, flush it to the terminal to ensure
        // it is fully applied:
        flush();
    }

    void NaiveUI::uninstall() {
        onExit(0);
    }

    void NaiveUI::render(const Component& component) {
        draw(component);
        std::cout << ToString() << mSetCursorPosition;
        flush();
        Clear();
    }

    void NaiveUI::draw(const Component& component) {
        const auto document = component->Render();
        const auto [dimX, dimY] = Terminal::Size();

        const auto resized = (dimX != dimx_) || (dimY != dimy_);
        std::cout << mResetCursorPosition << ResetPosition(/*clear=*/resized);

        // Resize the screen if needed.
        if(resized) {
            dimx_ = dimX;
            dimy_ = dimY;
            pixels_ = std::vector(dimY, std::vector<Pixel>(dimX));
            cursor_.x = dimx_ - 1;
            cursor_.y = dimy_ - 1;
        }

        mPreviousFrameResized = resized;

        Render(*this, document);

        // Set cursor position for user using tools to insert CJK characters.
        mSetCursorPosition = "";
        mResetCursorPosition = "";

        const int dx = dimx_ - 1 - cursor_.x;
        const int dy = dimy_ - 1 - cursor_.y;

        if(dx != 0) {
            mSetCursorPosition += "\x1B[" + std::to_string(dx) + "D";
            mResetCursorPosition += "\x1B[" + std::to_string(dx) + "C";
        }
        if(dy != 0) {
            mSetCursorPosition += "\x1B[" + std::to_string(dy) + "A";
            mResetCursorPosition += "\x1B[" + std::to_string(dy) + "B";
        }
    }

}  // namespace ftxui.
