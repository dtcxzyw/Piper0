/*
    SPDX-License-Identifier: GPL-3.0-or-later

    This file is part of Piper0, a physically based renderer.
    Copyright (C) 2022 Yingwei Zheng

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

// force include
#include <oneapi/tbb/tbbmalloc_proxy.h>
//
#include <Piper/Core/Context.hpp>
#include <Piper/Core/Monitor.hpp>
#include <Piper/Core/Report.hpp>
#include <Piper/Core/StaticFactory.hpp>
#include <Piper/Core/Stats.hpp>
#include <Piper/Core/Sync.hpp>
#include <Piper/Render/Pipeline.hpp>
#include <cxxopts.hpp>
#include <fmt/chrono.h>
#include <fmt/format.h>
#include <fstream>
#include <ftxui/component/component.hpp>
#include <ftxui/component/component_base.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/color.hpp>
#include <ftxui/screen/terminal.hpp>
#include <oneapi/tbb/task_group.h>
#include <pmmintrin.h>
#include <xmmintrin.h>
using namespace Piper;

void mainGuarded(int argc, char** argv) {
    _MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);
    _MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_ON);

    std::string inputFile, outputDir, serverConfig;
    bool help = false;

    cxxopts::Options options("Piper", "A physically based renderer");
    options.add_options()("display-server", "(IP address:port) pair for tev previewing",
                          cxxopts::value<std::string>(serverConfig)->default_value(""))            //
        ("input", "input file", cxxopts::value<std::string>(inputFile))                            //
        ("output", "output directory", cxxopts::value<std::string>(outputDir)->default_value(""))  //
        ("help", "print usage", cxxopts::value<bool>(help)->default_value("false"));

    const auto result = options.parse(argc, argv);
    if(help) {
        std::cerr << options.help() << std::endl;
        return;
    }

    const fs::path inputFilePath = inputFile;
    if(!fs::exists(inputFilePath) || !fs::is_regular_file(inputFilePath))
        fatal(fmt::format("The input file \"{}\" is not exist", inputFile));

    if(outputDir.empty())
        outputDir = inputFilePath.parent_path().string();

    const fs::path outputBase = outputDir;
    fs::create_directories(outputBase);
    if(!fs::exists(outputBase))
        fatal(fmt::format("Failed to create output directory \"{}\"", outputDir));

    logFile().open(outputBase / inputFilePath.filename().replace_extension(".log"));
    if(!serverConfig.empty())
        getDisplayProvider().connect(serverConfig);

    constexpr auto getPipelineType = [](const std::string_view ext) {
        if(ext == ".json"sv)
            return "PiperPipeline"sv;
        if(ext == ".pbrt"sv)
            return "PBRTv4Pipeline"sv;
        fatal("Unrecognized input file");
    };

    constexpr auto render = [&] {
        info("Loading scene");
        const auto pipelineDesc = makeRefCount<ConfigNode>(
            "pipeline"sv, getPipelineType(inputFilePath.extension().string()),
            ConfigNode::AttrMap{ { { "inputFile"sv, makeRefCount<ConfigAttr>(inputFile) } }, context().globalAllocator });
        const auto pipeline = getStaticFactory().make<Pipeline>(*pipelineDesc);
        info("Rendering scene");
        pipeline->execute(outputBase);

        printStats();
    };

    {
        namespace ui = ftxui;
        auto screen = ui::ScreenInteractive::Fullscreen();
        auto exit = screen.ExitLoopClosure();

        tbb::task_group tg;
        bool running = true;
        tg.run([&] {
            render();
            running = false;
        });

        // force update
        bool noProgress = true;
        tg.run([&] {
            while(running || !noProgress) {  // NOLINT(bugprone-infinite-loop)
                screen.PostEvent(ui::Event::Custom);
                std::this_thread::sleep_for(200ms);
            }
            exit();
        });

        auto lastUpdate = Clock::now() - 1s;
        std::optional<CurrentStatus> status;

        auto monitorView = ui::Renderer([&] {
            const auto now = Clock::now();

            if(now - lastUpdate > 500ms) {
                status = getMonitor().update();
                lastUpdate = now;
            }

            if(!status)
                return ui::text("Unavailable");
            const auto& ref = status.value();

            const auto coresPerLine = std::min(4U, static_cast<uint32_t>(std::sqrt(static_cast<double>(ref.cores.size()))));

            ui::Elements lines;
            for(uint32_t idx = 0; idx < ref.cores.size(); idx += coresPerLine) {
                const auto end = idx + coresPerLine;
                const auto realEnd = std::min(end, static_cast<uint32_t>(ref.cores.size()));
                ui::Elements cores;
                cores.reserve(coresPerLine);

                for(uint32_t id = idx; id < realEnd; ++id)
                    cores.push_back(ui::hbox({ ui::text(fmt::format(" CPU {:>3}[", id)),
                                               ui::gauge(static_cast<float>(ref.cores[id].first + ref.cores[id].second)) |
                                                   ui::color(ui::Color::DarkGreen),
                                               ui::text(" ]") }) |
                                    ui::xflex_grow);

                for(uint32_t id = realEnd; id < end; ++id)
                    cores.push_back(ui::emptyElement());

                lines.push_back(ui::hbox(std::move(cores)) | ui::xflex_grow);
            }

            lines.push_back(ui::text(fmt::format(" User   Time : {:>5.1f} %", ref.userRatio * 100.0)));
            lines.push_back(ui::text(fmt::format(" Kernel Time : {:>5.1f} %", ref.kernelRatio * 100.0)));
            lines.push_back(ui::text(fmt::format(" Memory Usage: {:>5.1f} MB", static_cast<double>(ref.memoryUsage) * 1e-6)));
            lines.push_back(ui::text(fmt::format(" I/O    Ops  : {:>5}", ref.IOOps)));
            lines.push_back(ui::text(fmt::format(" Read   Speed: {:>5.1f} MB/s", ref.readSpeed * 1e-6)));
            lines.push_back(ui::text(fmt::format(" Write  Speed: {:>5.1f} MB/s", ref.writeSpeed * 1e-6)));
            lines.push_back(ui::text(fmt::format(" Active I/O  : {:>5}", ref.activeIOThread)));

            return ui::vbox(std::move(lines));
        });

        auto statusView = ui::Renderer([&] {
            auto& reports = getProgressReports();
            constexpr auto hideOffset = 5s;
            ui::Elements lines;
            const auto now = Clock::now();

            for(auto& [name, reporter] : reports) {
                if(auto end = reporter->endTime()) {
                    if(now - end.value() > hideOffset)
                        continue;
                    lines.push_back(ui::text(
                        fmt::format("{} finished in {:%H:%M:%S}", name, std::chrono::ceil<std::chrono::seconds>(reporter->elapsed()))));
                } else {
                    const auto eta = reporter->eta();
                    const auto progress = reporter->progress() * 100.0;
                    const auto elapsed = std::chrono::ceil<std::chrono::seconds>(reporter->elapsed());
                    auto text = eta ? fmt::format(" ] {:.1f}% ETA: {:%H:%M:%S} Elapsed: {:%H:%M:%S}", progress,
                                                  std::chrono::ceil<std::chrono::seconds>(eta.value()), elapsed) :
                                      fmt::format(" ]  {:.1f}% Elapsed: {:%H:%M:%S}", progress, elapsed);

                    lines.push_back(ui::hbox({ ui::text(name + " ["), ui::gauge(static_cast<float>(reporter->progress())),
                                               ui::text(std::move(text)) }) |
                                    ui::xflex_grow);
                }
            }
            noProgress = lines.empty();

            if(lines.empty())
                return ui::text("No progress");
            return ui::vbox(std::move(lines));
        });
        auto consoleView = ui::Renderer([] {
            auto& output = consoleOutput();

            ui::Elements lines;
            for(auto& [type, msg] : output) {
                const auto& headerOfLog = header(type);
                constexpr auto selectColor = [](const LogType type) {
                    switch(type) {
                        case LogType::Info:
                            return ui::Color::Green;
                        case LogType::Warning:
                            return ui::Color::Yellow;
                        case LogType::Error:
                            [[fallthrough]];
                        case LogType::Fatal:
                            return ui::Color::Red;
                    }
                    PIPER_UNREACHABLE();
                };
                const auto headerText = ui::text(headerOfLog) | ui::color(selectColor(type));

                if(static_cast<int32_t>(msg.size()) + 10 < ui::Terminal::Size().dimx) {
                    lines.push_back(ui::hbox({ headerText, ui::text(msg) }));
                } else {
                    auto elements = ui::paragraph(msg);
                    {
                        lines.push_back(headerText);
                        for(auto&& x : elements)
                            lines.push_back(std::move(x));
                    }
                }
            }

            return ui::vbox(lines);
        });
        auto container = ui::Renderer([&] {
            return ui::vbox({ monitorView->Render(), ui::separator(), statusView->Render(), ui::separator(), consoleView->Render() }) |
                ui::borderStyled(ui::BorderStyle::LIGHT);
        });

        screen.Loop(container);

        tg.wait();
    }

    logFile().flush();

    if(getDisplayProvider().isSupported())
        getDisplayProvider().disconnect();
}

int main(const int argc, char** argv) {
    try {
        mainGuarded(argc, argv);
    } catch(const std::exception& ex) {
        fatal(fmt::format("{}: {}", typeid(ex).name(), ex.what()));
    } catch(...) {
        fatal("Unknown error"s);
    }

    return EXIT_SUCCESS;
}
