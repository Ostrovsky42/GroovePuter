#include "cardputer_smf_player.h"
#include "cardputer_smf_player_registry.h"

#include "src/midi/smf_player_service.h"
#include "src/platform/cardputer_usb_midi_service.h"

using namespace GroovePuterMidi;

namespace {
class LazyCardputerSmfPlayer final : public ISmfPlayerService {
public:
    LazyCardputerSmfPlayer() {
        registerSmfPlayerService(this);
    }

    bool requestLoadAndPlay(const char* path) override {
        return ensureStarted() && player_.requestLoadAndPlay(path);
    }

    bool togglePlayPause() override {
        return ensureStarted() && player_.togglePlayPause();
    }

    bool restart(SmfPlayerRestartOrigin origin) override {
        return ensureStarted() && player_.restart(origin);
    }

    bool stop() override {
        return ensureStarted() && player_.stop();
    }

    bool panic() override {
        return ensureStarted() && player_.panic();
    }

    bool seekBars(int deltaBars) override {
        return ensureStarted() && player_.seekBars(deltaBars);
    }

    bool toggleRouting() override {
        return ensureStarted() && player_.toggleRouting();
    }

    SmfPlayerSnapshot snapshot() const override {
        return player_.snapshot();
    }

    bool begin() {
        return ensureStarted();
    }

private:
    bool ensureStarted() {
        if (started_) return true;
        if (!player_.begin()) return false;
        registerCardputerSmfMidiQueue(&player_.eventQueue());
        started_ = true;
        return true;
    }

    CardputerSmfPlayerService player_;
    bool started_{false};
};

LazyCardputerSmfPlayer g_smfPlayer;
}  // namespace

bool beginCardputerSmfPlayerService() {
    return g_smfPlayer.begin();
}
