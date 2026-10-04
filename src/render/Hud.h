#pragma once
#include "gfx/Ui.h"
#include "logic/Session.h"
#include <array>
#include <functional>
#include <string>

// The base HUD laid out on the 640x360 canvas (SRS UI-2): act and clocks on top, colony bars on the left,
// panel buttons and the crew list on the right, carried slots at the bottom, plus banners and toasts.
class Hud {
public:
    void reset(const Session& s);
    void update(float dt, const Session& s);
    void draw(Session& s, float t, HitList& hits, const std::function<void(const std::string&)>& openPanel, bool panelOpen);

    void toast(const std::string& text) { toast_ = text; toastTime_ = 2.6f; }
    void message(const std::string& text, Tone tone) { message_ = text; messageTone_ = tone; messageTime_ = 3.4f; }
    void zoneBanner(const std::string& name, const std::string& sub, int cost) { zoneName_ = name; zoneSub_ = sub; zoneCost_ = cost; zoneTime_ = 2.8f; }
    void actBanner(const std::string& a, const std::string& b) { actA_ = a; actB_ = b; actTime_ = 3.2f; }
    const std::string& currentMessage() const { return message_; }
    bool messageVisible() const { return messageTime_ > 0; }
    Tone messageTone() const { return messageTone_; }
    static Color toneColor(Tone t);

private:
    struct BarAnim { float shown = 0, ghost = 0, hold = 0, flash = 0; };
    void drawWaypoint(Session& s, float t) const;
    std::array<BarAnim, 4> bars_{};
    int hover_ = -1;
    std::string toast_, message_, zoneName_, zoneSub_, actA_, actB_;
    float toastTime_ = 0, messageTime_ = 0, zoneTime_ = 0, actTime_ = 0, hintTime_ = 12;
    int zoneCost_ = 0;
    float goalBottom_ = 40;
    Tone messageTone_ = Tone::Info;
};
