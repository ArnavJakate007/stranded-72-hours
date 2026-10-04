#include "screens/PlayScreen.h"
#include "core/Game.h"
#include "screens/EndScreen.h"
#include "screens/FinaleScreen.h"
#include "screens/TitleScreen.h"
#include <algorithm>
#include <cmath>

PlayScreen::PlayScreen(Game& game, const SaveData* save) : Screen(game), session_(game.data()), world_(game.sprites()) {
    if (save) session_.loadFrom(*save);
    world_.bake(session_.world());
    hud_.reset(session_);
    weather_.setStorm(session_.colony().storm);
    game.audio().playMusic(session_.colony().storm ? "music_intro" : "music_game", 0.45f);
}

void PlayScreen::debugPuzzle(PuzzleKind k) {
    puzzleKind_ = k;
    puzzle_ = Puzzle::create(k, 1, 60, game_.audio());
}

void PlayScreen::openPanel(const std::string& id) {
    if (puzzle_ || pause_.isOpen()) return;
    choice_.close();
    panel_ = Panel::create(id, session_, game_.data(), world_.minimap());
    game_.audio().play("click");
}

void PlayScreen::applyPause(PauseMenu::Choice c) {
    Settings& st = game_.settings();
    switch (c) {
        case PauseMenu::Choice::None: break;
        case PauseMenu::Choice::Resume: pause_.close(); game_.audio().play("click"); break;
        case PauseMenu::Choice::Journal: pause_.close(); openPanel("journal"); break;
        case PauseMenu::Choice::Power: pause_.close(); openPanel("power"); break;
        case PauseMenu::Choice::Crew: pause_.close(); openPanel("crew"); break;
        case PauseMenu::Choice::Map: pause_.close(); openPanel("map"); break;
        case PauseMenu::Choice::Cargo: pause_.close(); openPanel("cargo"); break;
        case PauseMenu::Choice::TextSpeed: st.textSpeed = (st.textSpeed + 1) % 3; game_.audio().play("click"); break;
        case PauseMenu::Choice::Sound: st.sound = !st.sound; game_.audio().setEnabled(st.sound); game_.audio().play("click"); break;
        case PauseMenu::Choice::ClockSpeed:   // 2x normal or 1x relaxed; 4x was removed because playtests showed it cannot be won
            st.clockMultiplier = st.clockMultiplier == 2 ? 1.0f : 2.0f;
            game_.audio().play("click");
            break;
        case PauseMenu::Choice::Quit: game_.switchTo(std::make_unique<TitleScreen>(game_)); break;
    }
}

void PlayScreen::routeInput() {
    Input& in = game_.input();
    DialogueQueue& q = session_.dialogue();
    for (Input::Act a : in.presses()) {
        if (pause_.isOpen()) { applyPause(pause_.handle(a)); continue; }
        if (puzzle_) {
            if (a == Input::Act::Back) {
                puzzle_.reset();
                session_.cancelPuzzle();
            } else {
                puzzle_->handle(a);
            }
            continue;
        }
        if (choice_.isOpen()) { choice_.handle(a); continue; }
        if (panel_) { panel_->handle(a); continue; }
        if (q.blocking()) {
            if (a == Input::Act::Accept || a == Input::Act::Back) dialogueBox_.accept(q);
            continue;
        }
        if (a == Input::Act::Pause) { pause_.open(); game_.audio().play("click"); continue; }
        if (a == Input::Act::Accept) {
            if (dialogueBox_.typing(q)) dialogueBox_.accept(q);
            else session_.pressAccept();
        }
        if (a == Input::Act::Back) {
            if (session_.holdProgress() >= 0) session_.pressBack();
            else dialogueBox_.back(q);
        }
    }
    applyPause(pause_.takeClicked());
    if (in.clicked() && !overlayOpen()) {
        in.consumeClick();
        if (q.blocking()) dialogueBox_.accept(q);
        else session_.clickTile({static_cast<int>(std::floor((in.mouse().x + cam_.x) / 16)), static_cast<int>(std::floor((in.mouse().y + cam_.y) / 16))});
    }
}

void PlayScreen::handleEvents() {
    SessionEvent e;
    while (session_.pollEvent(e)) {
        switch (e.type) {
            case EventType::Sound: game_.audio().play(e.text); break;
            case EventType::Toast: hud_.toast(e.text); break;
            case EventType::Message: hud_.message(e.text, e.tone); break;
            case EventType::OpenPanel: openPanel(e.text); break;
            case EventType::StartPuzzle: {
                puzzleKind_ = session_.pendingPuzzle();
                int d = session_.pendingDifficulty();
                const Balance& b = game_.data().balance;
                float limit = d == 0 ? b.get("puzzle_time_easy") : d == 1 ? b.get("puzzle_time_medium") : b.get("puzzle_time_hard");
                if (session_.pendingTimeLimit() > 0) limit = session_.pendingTimeLimit();
                puzzle_ = Puzzle::create(puzzleKind_, d, limit, game_.audio());
                panel_.reset();
                break;
            }
            case EventType::ActBanner: hud_.actBanner(e.text, e.text2); break;
            case EventType::ZoneBanner: hud_.zoneBanner(e.text, e.text2, e.value); break;
            case EventType::Shake: shake_ = 0.22f; break;
            case EventType::Flash:
                if (e.text == "core") flash_ = 0.25f;
                else lights_.flicker(e.text);
                break;
            case EventType::OpenTalk: {
                const CrewMember& m = session_.colony().crew()[e.value];
                choice_.open(m.id(), m.name(), session_.talkOptions(e.value));
                break;
            }
            case EventType::SaveGame: game_.saves().writeSave(session_.snapshot()); break;
            case EventType::Storm:
                if (e.text == "start") {
                    weather_.setStorm(true);
                    shake_ = 0.4f;
                    game_.audio().play("thunder");
                    game_.audio().playMusic("music_intro", 0.5f);
                } else if (e.text == "end") {
                    weather_.setStorm(false);
                    game_.audio().playMusic("music_game", 0.45f);
                } else {
                    hud_.message("STORM WARNING: brace the base before Hour 48.", Tone::Warn);
                    game_.audio().play("alarm");
                }
                break;
            case EventType::Ending: endDelay_ = session_.summary().code[0] == 'E' ? 2.0f : 1.2f; break;
        }
    }
}

void PlayScreen::update(float dt) {
    Input& in = game_.input();
    routeInput();

    bool blocking = overlayOpen();
    std::optional<Dir> held = blocking ? std::nullopt : in.heldDirection();
    session_.update(dt, held, in.acceptHeld(), blocking, pause_.isOpen(), game_.settings().clockMultiplier);
    handleEvents();

    if (puzzle_ && !pause_.isOpen()) {
        if (puzzleKind_ == PuzzleKind::Tune && !session_.flags().contact && session_.clock().openWindow() == 0) {
            puzzle_.reset();
            session_.cancelPuzzle();
            hud_.message("The window closed. Wait for the next one.", Tone::Bad);
        } else {
            puzzle_->update(dt, in);
            if (puzzle_->state() == Puzzle::State::Solved) {
                puzzle_.reset();
                session_.puzzleFinished(true);
            } else if (puzzle_->state() == Puzzle::State::Failed) {
                puzzle_.reset();
                session_.puzzleFinished(false);
            }
        }
        handleEvents();
    }
    if (panel_) {
        panel_->update(dt);
        if (panel_->wantsClose()) {
            panel_.reset();
            game_.audio().play("click");
        }
    }
    if (!pause_.isOpen()) dialogueBox_.update(dt, session_.dialogue(), game_.settings().letterSeconds(), game_.audio());
    hud_.update(dt, session_);
    lights_.update(dt, session_);
    weather_.update(dt, game_.time());
    if (weather_.lightningStruck()) {
        flash_ = std::max(flash_, 0.12f);
        if (session_.outside()) game_.audio().play("thunder");
    }
    shake_ = std::max(0.0f, shake_ - dt);
    flash_ = std::max(0.0f, flash_ - dt);

    // the camera follows Kiran, clamped to the world edges
    const Player& p = session_.player();
    cam_ = Cam::follow(p.pixelX(), p.pixelY());
    lights_.build(session_, cam_, game_.time());   // render-to-texture happens outside the canvas pass

    if (endDelay_ >= 0) {
        endDelay_ -= dt;
        if (endDelay_ < 0) {
            game_.audio().stopMusic();
            if (session_.summary().code[0] == 'E') game_.switchTo(std::make_unique<FinaleScreen>(game_, session_.summary()));
            else game_.switchTo(std::make_unique<EndScreen>(game_, session_.summary()));
        }
    }
}

void PlayScreen::drawVignette() const {
    Color edge{2, 4, 9, 150}, clear{2, 4, 9, 0};
    DrawRectangleGradientV(0, 0, 640, 60, edge, clear);
    DrawRectangleGradientV(0, 300, 640, 60, clear, edge);
    DrawRectangleGradientH(0, 0, 80, 360, edge, clear);
    DrawRectangleGradientH(560, 0, 80, 360, clear, edge);
}

void PlayScreen::draw() {
    float t = game_.time();
    Cam view = cam_;
    if (shake_ > 0) view.x += static_cast<int>(t * 60) % 2 ? 1 : -1;
    ClearBackground(pal::Bg);
    world_.drawGround(view);
    BeginMode2D(view.camera2D());
    world_.drawEntities(session_, t);
    EndMode2D();
    lights_.apply();
    BeginMode2D(view.camera2D());
    world_.drawEmissive(session_, lights_, view, t);
    EndMode2D();
    weather_.drawSnow(session_, view);
    if (session_.windOn()) weather_.drawWind(t);
    drawVignette();
    if (weather_.storm()) weather_.drawStormShade(session_.outside());
    if (flash_ > 0) Ui::rect(0, 0, 640, 360, withAlpha(pal::rgb(0xFFECC8), flash_ * 2));

    Input& in = game_.input();
    bool mouseFree = in.mouseInside() && !overlayOpen() && !game_.hits().topAt(in.mouse());
    Vector2 mouseWorld{in.mouse().x + view.x, in.mouse().y + view.y};
    BeginMode2D(view.camera2D());
    world_.drawLabels(session_, lights_, view, t, mouseWorld, mouseFree, !overlayOpen() && !session_.dialogue().blocking());
    EndMode2D();

    HitList& hits = game_.hits();
    hud_.draw(session_, t, hits, [this](const std::string& id) { openPanel(id); }, panel_ != nullptr);
    if (panel_) panel_->draw(t, hits, hud_);
    dialogueBox_.draw(session_.dialogue(), t, hits, [this] { dialogueBox_.accept(session_.dialogue()); });
    choice_.draw(t, hits);
    if (puzzle_) puzzle_->draw(t, hits, in);
    pause_.draw(session_, t, hits, game_.settings().textSpeed, game_.settings().sound, game_.settings().clockMultiplier);
}
