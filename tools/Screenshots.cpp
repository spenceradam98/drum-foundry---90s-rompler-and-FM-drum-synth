// Developer tool: renders the editor in every theme to docs/screenshots/<theme>.png plus a 2x2
// themes.png. Build with -DDRUMFOUNDRY_BUILD_SCREENSHOTS=ON and run: DrumFoundryShots <out dir>
#include <juce_audio_utils/juce_audio_utils.h>

#include "PluginEditor.h"
#include "PluginProcessor.h"

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::File outDir (argc > 1 ? juce::File::getCurrentWorkingDirectory().getChildFile (argv[1])
                                      : juce::File::getCurrentWorkingDirectory());
    outDir.createDirectory();

    DrumFoundryProcessor proc;
    proc.setPlayConfigDetails (0, 2, 48000, 512);
    proc.prepareToPlay (48000, 512);
    proc.setCurrentProgram (1);   // Rave Metal

    std::vector<juce::Image> shots;
    const auto& themes = allThemes();
    for (size_t i = 0; i < themes.size(); ++i)
    {
        // light a couple of pads so the screenshot shows the pad LEDs
        proc.auditionPad (0);
        proc.auditionPad ((int) (3 + i) % 8);
        juce::AudioBuffer<float> buf (2, 512);
        juce::MidiBuffer midi;
        proc.processBlock (buf, midi);

        std::unique_ptr<DrumFoundryEditor> ed (static_cast<DrumFoundryEditor*> (proc.createEditor()));
        ed->applyTheme (themes[i].name);
        ed->selectPad ((int) i == 0 ? 0 : (int) (3 + i) % 8);
        ed->setVisible (true);
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 2.0f);
        shots.push_back (img);
        juce::PNGImageFormat png;
        auto f = outDir.getChildFile (themes[i].name.toLowerCase().replaceCharacters (" ", "-") + ".png");
        f.deleteFile();
        juce::FileOutputStream os (f);
        png.writeImageToStream (img, os);
        std::printf ("wrote %s\n", f.getFullPathName().toRawUTF8());
    }

    // 2x2 montage at half size
    const int w = shots[0].getWidth() / 2, h = shots[0].getHeight() / 2;
    juce::Image montage (juce::Image::RGB, w * 2 + 12, h * 2 + 12, true);
    juce::Graphics g (montage);
    g.fillAll (juce::Colour (0xff101010));
    for (size_t i = 0; i < shots.size() && i < 4; ++i)
        g.drawImage (shots[i], (int) (i % 2) * (w + 12), (int) (i / 2) * (h + 12), w, h, 0, 0, shots[i].getWidth(), shots[i].getHeight());
    auto f = outDir.getChildFile ("themes.png");
    f.deleteFile();
    juce::FileOutputStream os (f);
    juce::PNGImageFormat().writeImageToStream (montage, os);
    std::printf ("wrote %s\n", f.getFullPathName().toRawUTF8());
    return 0;
}
