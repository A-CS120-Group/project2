#include "Part3.h"
#include <JuceHeader.h>

class Application : public juce::JUCEApplication {
public:
    Application() = default;

    const juce::String getApplicationName() override { return "Part3"; }
    const juce::String getApplicationVersion() override { return "1.0.0"; }

    void initialise(const juce::String &) override { mainWindow = new MainWindow(getApplicationName(), new MainContentComponent, *this); }

    void shutdown() override { delete mainWindow; }

private:
    class MainWindow : public juce::DocumentWindow {
    public:
        MainWindow(const juce::String &name, juce::Component *c, JUCEApplication &a) : DocumentWindow(name, juce::Colours::darkgrey, juce::DocumentWindow::allButtons), app(a) {
            setUsingNativeTitleBar(true);
            setContentOwned(c, true);
            setResizable(false, false);
            //            setResizeLimits(300, 250, 10000, 10000);
            centreWithSize(getWidth(), getHeight());
            setVisible(true);
        }

        void closeButtonPressed() override { app.systemRequestedQuit(); }

    private:
        JUCEApplication &app;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainWindow)
    };

    MainWindow *mainWindow{nullptr};
};

START_JUCE_APPLICATION(Application)
