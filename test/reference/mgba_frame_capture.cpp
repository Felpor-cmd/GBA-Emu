#include <mgba/flags.h>
#include <mgba/core/core.h>
#include <mgba/core/version.h>
#include <mgba-util/vfs.h>
#include <fcntl.h>
#include <array>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

int main(int argc, char** argv) {
    if (argc != 4 && argc != 5) {
        std::cerr << "usage: mgba_frame_capture ROM FRAMES OUTPUT.ppm [ALIGNMENT_WINDOW]\n";
        return 2;
    }
    mCore* core = nullptr;
    try {
        if (std::string(projectVersion) != "0.10.5") throw std::runtime_error("mGBA 0.10.5 required");
        core = mCoreCreate(mPLATFORM_GBA);
        if (!core || !core->init(core)) throw std::runtime_error("mGBA initialization failed");
        mCoreInitConfig(core, nullptr);
        VFile* rom = VFileOpen(argv[1], O_RDONLY);
        if (!rom || !core->loadROM(core, rom)) throw std::runtime_error("Cannot load ROM");
        std::array<color_t, 240 * 160> pixels{};
        core->setVideoBuffer(core, pixels.data(), 240);
        core->opts.skipBios = true;
        core->reset(core);
        core->setKeys(core, 0);
        const unsigned frames = static_cast<unsigned>(std::stoul(argv[2]));
        const unsigned window = argc == 5 ? static_cast<unsigned>(std::stoul(argv[4])) : 0;
        for (unsigned frame = 1; frame <= frames + window; ++frame) {
            core->runFrame(core);
            if (frame + window < frames) continue;
            const std::string path = frame == frames ? argv[3]
                : std::string(argv[3]) + "." + std::to_string(frame) + ".ppm";
            std::ofstream output(path, std::ios::binary);
            output << "P6\n240 160\n255\n";
            for (auto pixel : pixels) {
                const char rgb[] = {static_cast<char>(pixel), static_cast<char>(pixel >> 8),
                                    static_cast<char>(pixel >> 16)};
                output.write(rgb, 3);
            }
            if (!output) throw std::runtime_error("Cannot write capture");
        }
        core->deinit(core);
    } catch (const std::exception& error) {
        if (core) core->deinit(core);
        std::cerr << error.what() << '\n';
        return 1;
    }
}
