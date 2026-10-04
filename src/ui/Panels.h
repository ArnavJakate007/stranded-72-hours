#pragma once
#include "core/Input.h"
#include "gfx/Ui.h"
#include "logic/DataStore.h"
#include "logic/Session.h"
#include "raylib.h"
#include <memory>
#include <string>

class Hud;

// Abstract base for the panels that open over the colony view. draw() is a template method:
// the base draws the frame and close button, each child fills in drawContent() (inheritance + polymorphism).
class Panel {
public:
    Panel(Session& s, std::string title, std::string subtitle) : session_(s), title_(std::move(title)), subtitle_(std::move(subtitle)) {}
    virtual ~Panel() = default;

    virtual void handle(Input::Act a) = 0;
    void update(float dt) { slide_ = std::min(1.0f, slide_ + dt / 0.18f); }
    void draw(float t, HitList& hits, const Hud& hud);
    bool wantsClose() const { return close_; }
    static std::unique_ptr<Panel> create(const std::string& id, Session& s, const DataStore& data, const Texture2D& minimap);

protected:
    virtual void drawContent(float x, float y, float w, float h, float t, HitList& hits, const Hud& hud) = 0;
    void footer(float x, float y, float h, const Hud& hud, const std::string& hint) const;
    Session& session_;
    std::string title_, subtitle_;
    int row_ = 0;
    bool close_ = false;
    float slide_ = 0;
};

class PowerPanel : public Panel {
public:
    explicit PowerPanel(Session& s) : Panel(s, "POWER PANEL", "") {}
    void handle(Input::Act a) override;
protected:
    void drawContent(float x, float y, float w, float h, float t, HitList& hits, const Hud& hud) override;
};

class CrewPanel : public Panel {
public:
    explicit CrewPanel(Session& s) : Panel(s, "CREW PANEL", "Or walk up to them and press Z") {}
    void handle(Input::Act a) override;
protected:
    void drawContent(float x, float y, float w, float h, float t, HitList& hits, const Hud& hud) override;
};

class MapPanel : public Panel {
public:
    MapPanel(Session& s, const Texture2D& minimap) : Panel(s, "MAP OF KEPLER-X", "Each zone you enter costs suit oxygen"), minimap_(minimap) {}
    void handle(Input::Act a) override;
protected:
    void drawContent(float x, float y, float w, float h, float t, HitList& hits, const Hud& hud) override;
private:
    const Texture2D& minimap_;
};

class JournalPanel : public Panel {
public:
    explicit JournalPanel(Session& s) : Panel(s, "JOURNAL", "") {}
    void handle(Input::Act a) override;
protected:
    void drawContent(float x, float y, float w, float h, float t, HitList& hits, const Hud& hud) override;
private:
    int tab_ = 0;
};

class CargoPanel : public Panel {
public:
    CargoPanel(Session& s, const DataStore& data) : Panel(s, "CARGO", "Parts you carry in are stored here"), data_(data) {}
    void handle(Input::Act a) override;
protected:
    void drawContent(float x, float y, float w, float h, float t, HitList& hits, const Hud& hud) override;
private:
    std::vector<std::pair<std::string, int>> list() const;
    const DataStore& data_;
    int tab_ = 0;
};
