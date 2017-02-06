#include "Window.h"
#include "Os.h"
#include "MiscUtil.h"
#include "MathUtil.h"
#include "Array.h"
#include "Rendering.h"
#include "Simd.h"
#include "Set.h"
#include "Ring.h"
#include <iostream>
#include <sstream>

static void FlushToDebugOutput(std::stringstream& stream) {
    if (stream.rdbuf()->in_avail()) {
        auto text = stream.str();
        gg::Os::PrintDebug(text.c_str());
        stream = std::stringstream();
    }
}

struct Timing {
    int64_t rawTimer = gg::Os::GetRawTimer();
    float lastFrameMilliseconds = 16.67f;
    float avgFrameMilliseconds = 16.67f;

    void advanceFrame(gg::Os const& os) {
        int64_t rawTimerOld = std::exchange(rawTimer, os.GetRawTimer());
        lastFrameMilliseconds = os.asFloatMilliseconds(rawTimer - rawTimerOld);
        avgFrameMilliseconds = gg::Lerp(avgFrameMilliseconds, lastFrameMilliseconds, 0.05f);
    }
};

int main(int argc, char* argv[]) {
    gg::Os os;
    GG_SCOPE_EXIT(std::cout << "Bye!\n"; os.Sleep(200));

    std::cout << "Loading pipeline descriptions...";
    gg::RenderPipelineDescription pipelineDef = gg::RenderPipelineDescription::LoadFromFiles("shaders/Sprite.vertex.spv", "shaders/Sprite.fragment.spv");
    std::cout << "done\n";

    std::stringstream debugPrint;

    GG_SCOPE_EXIT(std::cout << "done\n");
    std::cout << "Creating window...";
    gg::Window window("Giggity");
    std::cout << "done\n";
    GG_SCOPE_EXIT(std::cout << "Tearing down window...");

    unsigned width = 0;
    unsigned height = 0;

    std::cout << "Starting renderer...";
    gg::Rendering::Hub renderingHub(nullptr);
    std::cout << "done\n";
    {
        uint8_t rgbaData[] = {
            0x00,0xff,0xff,0xff, 0xff,0x00,0xff,0xff, 0xff,0xff,0x00,0xff, 0xff,0xff,0xff,0x00, 0x00,0xff,0xff,0xff, 0xff,0x00,0xff,0xff, 0xff,0xff,0x00,0xff, 0xff,0xff,0xff,0x00,
            0x00,0xff,0xff,0xff, 0xff,0x00,0xff,0xff, 0xff,0xff,0x00,0xff, 0xff,0xff,0xff,0x00, 0x00,0xff,0xff,0xff, 0xff,0x00,0xff,0xff, 0xff,0xff,0x00,0xff, 0xff,0xff,0xff,0x00,
            0x00,0xff,0xff,0xff, 0xff,0x00,0xff,0xff, 0xff,0xff,0x00,0xff, 0xff,0xff,0xff,0x00, 0x00,0xff,0xff,0xff, 0xff,0x00,0xff,0xff, 0xff,0xff,0x00,0xff, 0xff,0xff,0xff,0x00,
            0x00,0xff,0xff,0xff, 0xff,0x00,0xff,0xff, 0xff,0xff,0x00,0xff, 0xff,0xff,0xff,0x00, 0x00,0xff,0xff,0xff, 0xff,0x00,0xff,0xff, 0xff,0xff,0x00,0xff, 0xff,0xff,0xff,0x00,
            0x00,0xff,0xff,0xff, 0xff,0x00,0xff,0xff, 0xff,0xff,0x00,0xff, 0xff,0xff,0xff,0x00, 0x00,0xff,0xff,0xff, 0xff,0x00,0xff,0xff, 0xff,0xff,0x00,0xff, 0xff,0xff,0xff,0x00,
            0x00,0xff,0xff,0xff, 0xff,0x00,0xff,0xff, 0xff,0xff,0x00,0xff, 0xff,0xff,0xff,0x00, 0x00,0xff,0xff,0xff, 0xff,0x00,0xff,0xff, 0xff,0xff,0x00,0xff, 0xff,0xff,0xff,0x00,
            0x00,0xff,0xff,0xff, 0xff,0x00,0xff,0xff, 0xff,0xff,0x00,0xff, 0xff,0xff,0xff,0x00, 0x00,0xff,0xff,0xff, 0xff,0x00,0xff,0xff, 0xff,0xff,0x00,0xff, 0xff,0xff,0xff,0x00,
            0x00,0xff,0xff,0xff, 0xff,0x00,0xff,0xff, 0xff,0xff,0x00,0xff, 0xff,0xff,0xff,0x00, 0x00,0xff,0xff,0xff, 0xff,0x00,0xff,0xff, 0xff,0xff,0x00,0xff, 0xff,0xff,0xff,0x00,
        };
        std::cout << "Exercising renderer...";
        gg::Array<uint8_t> pixels;
        pixels.addLastCopiedSpan(rgbaData);
        gg::RenderFormat const rgba8888 = {gg::RenderFormat::Layout::cRGBA, gg::RenderFormat::BitDepth::c8, gg::RenderFormat::Type::cUnorm};
        gg::Rendering::Image image(&renderingHub, pixels, rgba8888, 8, 8);
        std::cout << "done\n";

        //std::cout << "Creating blueprint...";
        //gg::Rendering::Blueprint blueprint(&renderingHub, gg::Rendering::Blueprint::cDefaultDescription);
        //std::cout << "done\n";

        std::cout << "Creating pipeline...";
        gg::Rendering::Pipeline pipeline(&renderingHub, pipelineDef, window.hwnd());
        std::cout << "done\n";

        gg::Window::ShowConsole(false);

        float x = 0.f;
        float y = 0.f;
        float dx = 1.5f;
        float dy = 2.f;

        unsigned frames = 0;
        Timing timing;
        while (!window.isClosing()) {
            bool resized = os.GetClientSize(window.hwnd(), width, height);
            x = gg::Clamp(x, 0.f, (float)(width - 8));
            y = gg::Clamp(y, 0.f, (float)(height - 8));

            gg::Rendering rendering = renderingHub.startRendering(pipeline);
            /*if (frames & 1) */rendering.addImage(x, y, image);
            renderingHub.submitRendering(std::move(rendering));

            timing.advanceFrame(os);

            frames++;

            dx *= ((0 < x+dx) && (x+8+dx < width)) ? 1.f : -1.f;
            dy *= ((0 < y+dy) && (y+8+dy < height)) ? 1.f : -1.f;
            x += dx;
            y += dy;

            if (os.IsDebuggerPresent()) {
                if (frames % 512 == 128) {
                    debugPrint << "Frame period:\t" << timing.avgFrameMilliseconds << " milliseconds\n";
                }
                if (resized) {
                    debugPrint << "Output dimensions:\t" << width << "x" << height << " pixels\n";
                }
                FlushToDebugOutput(debugPrint);
            }
        }

        //gg::Window::ShowConsole(true);
    }

    std::cout << "Waiting for window to close...";
    window.waitForClose();
    std::cout << "done\n";
    return 0;
}
