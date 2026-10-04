#include "ui/Puzzles.h"
#include "core/Audio.h"
#include "raylib.h"
#include <algorithm>
#include <cmath>

namespace {
const Color kWireColors[5] = {pal::rgb(0xE24B4A), pal::rgb(0x4AA8E8), pal::rgb(0xF2C53D), pal::rgb(0x45C06A), pal::rgb(0xC77DFF)};
const float BX = 110, BY = 50, BW = 420, BH = 250;
}  // namespace

// ---------------------------------------------------------------- base

Puzzle::Puzzle(std::string title, std::string hint, float timeLimit, AudioBank& audio)
    : audio_(audio), rng_(static_cast<uint32_t>(GetTime() * 1000) + 7), timeLeft_(timeLimit), timeLimit_(timeLimit),
      title_(std::move(title)), hint_(std::move(hint)) {}

std::unique_ptr<Puzzle> Puzzle::create(PuzzleKind kind, int difficulty, float timeLimit, AudioBank& audio) {
    switch (kind) {
        case PuzzleKind::Wire: return std::make_unique<WirePuzzle>(3 + difficulty, timeLimit, audio);
        case PuzzleKind::Pipe: return std::make_unique<PipePuzzle>(4 + difficulty, 4 + difficulty / 2, timeLimit, audio);
        case PuzzleKind::Tune: return std::make_unique<TunePuzzle>(timeLimit, audio);
        case PuzzleKind::Sequence: return std::make_unique<SequencePuzzle>(3 + difficulty, timeLimit, audio);
        case PuzzleKind::Cut: return std::make_unique<CutPuzzle>(3 + difficulty, timeLimit, audio);
        default: return nullptr;
    }
}

void Puzzle::update(float dt, const Input& in) {
    if (state_ != State::Playing) return;
    shake_ = std::max(0.0f, shake_ - dt);
    timeLeft_ -= dt;   // the game clock keeps running too (FR-8)
    if (timeLeft_ <= 0) {
        fail("Out of time.");
        return;
    }
    step(dt, in);
}

void Puzzle::solve() {
    if (state_ != State::Playing) return;
    state_ = State::Solved;
    audio_.play("ok");
}

void Puzzle::mistake(float penalty) {
    timeLeft_ -= penalty;
    shake_ = 0.2f;
    audio_.play("spark");
}

void Puzzle::fail(const std::string& why) {
    if (state_ != State::Playing) return;
    state_ = State::Failed;
    failReason_ = why;
}

void Puzzle::draw(float t, HitList& hits, const Input& in) {
    Ui::rect(0, 0, 640, 360, Color{3, 5, 10, 166});
    hits.add(Rectangle{0, 0, 640, 360}, nullptr);
    float sx = shake_ > 0 ? (static_cast<int>(t * 60) % 2 ? 1.0f : -1.0f) : 0.0f;
    Ui::panel(BX + sx, BY, BW, BH, pal::Amber, pal::PanelFillDark);
    Ui::text(title_, BX + sx + 12, BY + 9, pal::Amber);
    std::string secs = std::to_string(static_cast<int>(std::ceil(std::max(0.0f, timeLeft_)))) + "s";
    Ui::textRight(secs, BX + sx + BW - 12, BY + 9, timeLeft_ < 15 ? pal::Red : pal::Ink);
    Ui::rect(BX + sx + 12, BY + 20, BW - 24, 3, pal::SegEmpty);
    Ui::rect(BX + sx + 12, BY + 20, std::round((BW - 24) * std::max(0.0f, timeLeft_) / timeLimit_), 3, timeLeft_ < 15 ? pal::Red : pal::TealHi);
    Ui::text(hint_, BX + sx + 12, BY + 28, pal::Dim);
    drawBoard(BX + sx + 12, BY + 44, BW - 24, BH - 64, t, hits, in);
    Ui::text("X cancel", BX + sx + 12, BY + BH - 14, pal::Dim2);
}

// ---------------------------------------------------------------- Wire

WirePuzzle::WirePuzzle(int pairs, float timeLimit, AudioBank& audio)
    : Puzzle("WIRE REPAIR", "Pick a wire on the left, then its colour on the right (arrows + Z, or click)", timeLimit, audio) {
    for (int i = 0; i < pairs; ++i) {
        left_.push_back(i);
        right_.push_back(i);
    }
    for (int i = pairs - 1; i > 0; --i) std::swap(left_[i], left_[rng_.range(0, i + 1)]);
    do {
        for (int i = pairs - 1; i > 0; --i) std::swap(right_[i], right_[rng_.range(0, i + 1)]);
    } while (right_ == left_);
    link_.assign(pairs, -1);
}

void WirePuzzle::pick(int side, int index) {
    if (side == 0) {
        if (link_[index] >= 0) return;
        held_ = index;
        side_ = 1;
        cursor_ = 0;
        audio_.play("click");
        return;
    }
    if (held_ < 0) return;
    if (std::find(link_.begin(), link_.end(), index) != link_.end()) return;
    if (left_[held_] == right_[index]) {
        link_[held_] = index;
        audio_.play("click");
        held_ = -1;
        side_ = 0;
        if (std::all_of(link_.begin(), link_.end(), [](int v) { return v >= 0; })) solve();
    } else {
        mistake(5);
        held_ = -1;
        side_ = 0;
    }
}

void WirePuzzle::handle(Input::Act a) {
    int n = static_cast<int>(left_.size());
    if (a == Input::Act::Up) cursor_ = (cursor_ + n - 1) % n;
    if (a == Input::Act::Down) cursor_ = (cursor_ + 1) % n;
    if (a == Input::Act::Left && side_ == 1) { side_ = 0; held_ = -1; }
    if (a == Input::Act::Accept) pick(side_, cursor_);
}

void WirePuzzle::drawBoard(float x, float y, float w, float h, float t, HitList& hits, const Input& in) {
    (void)in;
    int n = static_cast<int>(left_.size());
    float gap = h / n, lx = x + 40, rx = x + w - 40;
    Ui::rect(x, y, w, h, pal::rgb(0x0A1420));
    for (int i = 0; i < 8; ++i) Ui::rect(x + i * w / 8, y, 1, h, pal::rgb(0x132233));
    auto nodeY = [&](int i) { return y + gap * i + gap / 2; };
    for (int i = 0; i < n; ++i) {
        if (link_[i] < 0) continue;
        float y1 = nodeY(i), y2 = nodeY(link_[i]), mid = x + w / 2 + (i - n / 2.0f) * 8;
        Color c = kWireColors[left_[i]];
        Ui::rect(lx, y1 - 1, mid - lx, 3, c);
        Ui::rect(mid - 1, std::min(y1, y2) - 1, 3, std::fabs(y2 - y1) + 3, c);
        Ui::rect(mid, y2 - 1, rx - mid, 3, c);
    }
    if (held_ >= 0) {
        float y1 = nodeY(held_), y2 = nodeY(cursor_);
        Color c = withAlpha(kWireColors[left_[held_]], 0.6f + 0.4f * std::sin(t * 8));
        Ui::rect(lx, y1 - 1, (rx - lx) / 2, 2, c);
        Ui::rect(lx + (rx - lx) / 2, std::min(y1, y2), 2, std::fabs(y2 - y1) + 1, c);
        Ui::rect(lx + (rx - lx) / 2, y2 - 1, (rx - lx) / 2, 2, c);
    }
    for (int side = 0; side < 2; ++side)
        for (int i = 0; i < n; ++i) {
            float nx = side == 0 ? lx : rx, ny = nodeY(i);
            int color = side == 0 ? left_[i] : right_[i];
            bool done = side == 0 ? link_[i] >= 0 : std::find(link_.begin(), link_.end(), i) != link_.end();
            Ui::rect(nx - 9, ny - 9, 18, 18, pal::PanelEdge);
            Ui::rect(nx - 8, ny - 8, 16, 16, done ? shade(kWireColors[color], 0.5f) : pal::rgb(0x1C2840));
            Ui::disc(static_cast<int>(nx), static_cast<int>(ny), 5, kWireColors[color]);
            if (side_ == side && cursor_ == i) Ui::brackets(nx - 9, ny - 9, 18, 18, t);
            int idx = i, sd = side;
            hits.add(Rectangle{nx - 10, ny - 10, 20, 20}, [this, sd, idx] { if (sd == 1 && held_ < 0) return; pick(sd, idx); },
                     [this, sd, idx] { if (sd == side_) cursor_ = idx; });
        }
}

// ---------------------------------------------------------------- Pipe

namespace {
int rot(int m) { return ((m << 1) | (m >> 3)) & 15; }
}

PipePuzzle::PipePuzzle(int cols, int rows, float timeLimit, AudioBank& audio)
    : Puzzle("PIPE REPAIR", "Rotate tiles (Z or click) until the flow reaches the outlet on the right", timeLimit, audio), cols_(cols), rows_(rows) {
    mask_.assign(cols * rows, 0);
    wet_.assign(cols * rows, 0);
    inRow_ = rng_.range(0, rows);
    // carve a path from the inlet to the right edge, moving right or vertically without revisiting
    std::vector<IVec> path{{0, inRow_}};
    std::vector<char> used(cols * rows, 0);
    used[inRow_ * cols] = 1;
    while (path.back().x < cols - 1) {
        IVec p = path.back();
        std::vector<IVec> opts;
        if (p.y > 0 && !used[(p.y - 1) * cols + p.x]) opts.push_back({p.x, p.y - 1});
        if (p.y < rows - 1 && !used[(p.y + 1) * cols + p.x]) opts.push_back({p.x, p.y + 1});
        IVec right{p.x + 1, p.y};
        opts.push_back(right);
        opts.push_back(right);   // prefer going right so the path always finishes
        IVec n = opts[rng_.range(0, static_cast<int>(opts.size()))];
        used[n.y * cols + n.x] = 1;
        path.push_back(n);
    }
    outRow_ = path.back().y;
    auto bit = [](IVec from, IVec to) { return to.x > from.x ? 2 : to.x < from.x ? 8 : to.y > from.y ? 4 : 1; };
    for (size_t i = 0; i < path.size(); ++i) {
        int m = 0;
        if (i == 0) m |= 8;
        else m |= bit(path[i], path[i - 1]);
        if (i + 1 == path.size()) m |= 2;
        else m |= bit(path[i], path[i + 1]);
        mask_[path[i].y * cols + path[i].x] = m;
    }
    for (int& m : mask_)
        if (m == 0) m = rng_.next() < 0.5f ? 10 : 3;   // spare straight or corner pieces
    for (int& m : mask_)
        for (int r = rng_.range(1, 4); r > 0; --r) m = rot(m);
    flow();
}

void PipePuzzle::flow() {
    std::fill(wet_.begin(), wet_.end(), 0);
    int start = inRow_ * cols_;
    if (!(mask_[start] & 8)) return;
    std::vector<int> stack{start};
    wet_[start] = 1;
    const int dx[4] = {0, 1, 0, -1}, dy[4] = {-1, 0, 1, 0}, bits[4] = {1, 2, 4, 8}, opp[4] = {4, 8, 1, 2};
    while (!stack.empty()) {
        int c = stack.back();
        stack.pop_back();
        int x = c % cols_, y = c / cols_;
        for (int d = 0; d < 4; ++d) {
            if (!(mask_[c] & bits[d])) continue;
            int nx = x + dx[d], ny = y + dy[d];
            if (nx < 0 || ny < 0 || nx >= cols_ || ny >= rows_) continue;
            int ni = ny * cols_ + nx;
            if (wet_[ni] || !(mask_[ni] & opp[d])) continue;
            wet_[ni] = 1;
            stack.push_back(ni);
        }
    }
    int end = outRow_ * cols_ + cols_ - 1;
    if (wet_[end] && (mask_[end] & 2)) solve();
}

void PipePuzzle::rotate(int index) {
    if (state() != State::Playing) return;
    mask_[index] = rot(mask_[index]);
    audio_.play("click");
    flow();
}

void PipePuzzle::handle(Input::Act a) {
    if (a == Input::Act::Up) cy_ = (cy_ + rows_ - 1) % rows_;
    if (a == Input::Act::Down) cy_ = (cy_ + 1) % rows_;
    if (a == Input::Act::Left) cx_ = (cx_ + cols_ - 1) % cols_;
    if (a == Input::Act::Right) cx_ = (cx_ + 1) % cols_;
    if (a == Input::Act::Accept) rotate(cy_ * cols_ + cx_);
}

void PipePuzzle::drawBoard(float x, float y, float w, float h, float t, HitList& hits, const Input& in) {
    (void)in;
    float cell = std::floor(std::min((w - 40) / cols_, h / rows_));
    float ox = x + std::floor((w - cell * cols_) / 2), oy = y + std::floor((h - cell * rows_) / 2);
    Ui::rect(ox - 14, oy + inRow_ * cell + cell / 2 - 3, 14, 6, pal::Blue);
    Ui::rect(ox + cols_ * cell, oy + outRow_ * cell + cell / 2 - 3, 14, 6, wet_[outRow_ * cols_ + cols_ - 1] ? pal::Blue : pal::rgb(0x3A4660));
    for (int yy = 0; yy < rows_; ++yy)
        for (int xx = 0; xx < cols_; ++xx) {
            int i = yy * cols_ + xx;
            float cx = ox + xx * cell, cy = oy + yy * cell, half = cell / 2;
            Ui::rect(cx, cy, cell - 1, cell - 1, pal::rgb(0x111C2C));
            Color pc = wet_[i] ? pal::Blue : pal::rgb(0x5E6878);
            float th = std::max(4.0f, std::floor(cell / 5));
            Ui::rect(cx + half - th / 2, cy + half - th / 2, th, th, pc);
            if (mask_[i] & 1) Ui::rect(cx + half - th / 2, cy, th, half, pc);
            if (mask_[i] & 4) Ui::rect(cx + half - th / 2, cy + half, th, half, pc);
            if (mask_[i] & 8) Ui::rect(cx, cy + half - th / 2, half, th, pc);
            if (mask_[i] & 2) Ui::rect(cx + half, cy + half - th / 2, half, th, pc);
            if (xx == cx_ && yy == cy_) Ui::brackets(cx, cy, cell - 1, cell - 1, t, pal::Amber, 5, 1);
            int idx = i;
            hits.add(Rectangle{cx, cy, cell, cell}, [this, idx] { rotate(idx); }, [this, xx, yy] { cx_ = xx; cy_ = yy; });
        }
}

// ---------------------------------------------------------------- Tune

TunePuzzle::TunePuzzle(float timeLimit, AudioBank& audio)
    : Puzzle("ALIGN THE ANTENNA", "Keep the needle on the signal: Left/Right or move the mouse", timeLimit, audio) {}

void TunePuzzle::step(float dt, const Input& in) {
    clock_ += dt;
    target_ = 0.5f + 0.32f * std::sin(clock_ * 0.7f) + 0.1f * std::sin(clock_ * 1.9f);
    if (in.leftHeld()) pos_ -= dt * 0.55f;
    if (in.rightHeld()) pos_ += dt * 0.55f;
    if (mouseX_ >= 0) {
        pos_ += (mouseX_ - pos_) * std::min(1.0f, dt * 10);
        mouseX_ = -1;
    }
    pos_ = std::clamp(pos_, 0.0f, 1.0f);
    bool inside = std::fabs(pos_ - target_) < 0.07f;
    lock_ = std::clamp(lock_ + (inside ? 0.22f : -0.12f) * dt, 0.0f, 1.0f);
    if (lock_ >= 1) solve();
}

void TunePuzzle::drawBoard(float x, float y, float w, float h, float t, HitList& hits, const Input& in) {
    (void)h;
    float bx = x + 8, by = y + 20, bw = w - 16, bh = 60;
    if (in.mouseInside() && CheckCollisionPointRec(in.mouse(), Rectangle{bx - 20, by - 30, bw + 40, bh + 60}) && in.mouseMoved())
        mouseX_ = std::clamp((in.mouse().x - bx) / bw, 0.0f, 1.0f);
    Ui::rect(bx - 1, by - 1, bw + 2, bh + 2, pal::PanelEdge);
    Ui::rect(bx, by, bw, bh, pal::rgb(0x0A1A24));
    Rng r(static_cast<uint32_t>(t * 12));
    for (float i = 0; i < bw; i += 3) {
        float d = std::fabs(i / bw - target_);
        float amp = std::max(0.0f, 1 - d * 7) * 44 + r.next() * 8;
        Ui::rect(bx + i, by + bh - 2 - amp, 2, amp, d < 0.07f ? pal::rgb(0x5EFFA8) : pal::rgb(0x2F6F8A));
    }
    Ui::brackets(bx + std::round((target_ - 0.07f) * bw), by + 2, std::round(0.14f * bw), bh - 4, t, pal::rgb(0x5EFFA8), 5, 1, false);
    float nx = bx + std::round(pos_ * bw);
    Ui::rect(nx - 1, by - 6, 3, bh + 12, pal::Amber);
    Ui::rect(nx - 3, by - 8, 7, 3, pal::Amber);
    Ui::text("SIGNAL LOCK", bx, by + bh + 12, pal::Dim);
    Ui::rect(bx, by + bh + 22, bw, 8, pal::PanelEdge);
    Ui::rect(bx + 1, by + bh + 23, std::round((bw - 2) * lock_), 6, pal::rgb(0x5EFFA8));
    hits.add(Rectangle{bx - 20, by - 30, bw + 40, bh + 60}, nullptr);
}

// ---------------------------------------------------------------- Sequence

SequencePuzzle::SequencePuzzle(int length, float timeLimit, AudioBank& audio)
    : Puzzle("SEQUENCE RESTART", "Watch the lights, then press them in the same order", timeLimit, audio) {
    for (int i = 0; i < length; ++i) seq_.push_back(rng_.range(0, 6));
    replay();
}

void SequencePuzzle::replay() {
    showing_ = true;
    showTime_ = -0.4f;
    entered_ = 0;
}

void SequencePuzzle::step(float dt, const Input& in) {
    (void)in;
    pressFlash_ = std::max(0.0f, pressFlash_ - dt);
    if (!showing_) return;
    showTime_ += dt;
    int idx = static_cast<int>(std::floor(showTime_ / 0.7f));
    float within = showTime_ - idx * 0.7f;
    if (showTime_ < 0) flashLight_ = -1;
    else if (idx < static_cast<int>(seq_.size())) {
        int light = within < 0.5f ? seq_[idx] : -1;
        if (light >= 0 && light != flashLight_) audio_.play("blip");
        flashLight_ = light;
    } else {
        flashLight_ = -1;
        showing_ = false;
    }
}

void SequencePuzzle::press(int light) {
    if (showing_ || state() != State::Playing) return;
    flashLight_ = light;
    pressFlash_ = 0.2f;
    if (seq_[entered_] == light) {
        audio_.play("click");
        if (++entered_ == static_cast<int>(seq_.size())) solve();
    } else {
        mistake(5);
        replay();
    }
}

void SequencePuzzle::handle(Input::Act a) {
    if (a == Input::Act::Left) cursor_ = (cursor_ + 5) % 6;
    if (a == Input::Act::Right) cursor_ = (cursor_ + 1) % 6;
    if (a == Input::Act::Up || a == Input::Act::Down) cursor_ = (cursor_ + 3) % 6;
    if (a == Input::Act::Accept) press(cursor_);
}

void SequencePuzzle::drawBoard(float x, float y, float w, float h, float t, HitList& hits, const Input& in) {
    (void)in;
    const Color colors[6] = {pal::rgb(0xE24B4A), pal::rgb(0x4AA8E8), pal::rgb(0xF2C53D), pal::rgb(0x45C06A), pal::rgb(0xC77DFF), pal::rgb(0xEF9F27)};
    float size = 44, gap = 18, gw = 3 * size + 2 * gap, ox = x + (w - gw) / 2, oy = y + 10;
    Ui::textCentered(showing_ ? "Watch..." : "Your turn: " + std::to_string(entered_) + " of " + std::to_string(seq_.size()), x + w / 2, y + h - 30,
                     showing_ ? pal::Amber : pal::Ink);
    for (int i = 0; i < 6; ++i) {
        float lx = ox + (i % 3) * (size + gap), ly = oy + (i / 3) * (size + gap);
        bool lit = flashLight_ == i && (showing_ || pressFlash_ > 0);
        Ui::rect(lx, ly, size, size, pal::PanelEdge);
        Ui::rect(lx + 2, ly + 2, size - 4, size - 4, lit ? colors[i] : shade(colors[i], 0.3f));
        if (lit) Ui::glow(lx + size / 2, ly + size / 2, size, colors[i], 0.4f);
        if (!showing_ && cursor_ == i) Ui::brackets(lx, ly, size, size, t);
        int idx = i;
        hits.add(Rectangle{lx, ly, size, size}, [this, idx] { press(idx); }, [this, idx] { cursor_ = idx; });
    }
}

// ---------------------------------------------------------------- Cut

CutPuzzle::CutPuzzle(int points, float timeLimit, AudioBank& audio)
    : Puzzle("PLASMA CUTTER", "Pick a weak point, hold Z or the mouse to cut. Watch the heat!", timeLimit, audio) {
    for (int i = 0; i < points; ++i) {
        cut_.push_back(0);
        spot_.push_back((i + 0.5f) / points + (rng_.next() - 0.5f) * 0.08f);
    }
}

void CutPuzzle::handle(Input::Act a) {
    int n = static_cast<int>(cut_.size());
    if (a == Input::Act::Left) cursor_ = (cursor_ + n - 1) % n;
    if (a == Input::Act::Right) cursor_ = (cursor_ + 1) % n;
    if (a == Input::Act::Accept) cutBy(0.07f, 0.05f);   // a tap: slow but cool
}

void CutPuzzle::cutBy(float amount, float heat) {
    if (overheatLock_ > 0 || cut_[cursor_] >= 1) return;
    cut_[cursor_] = std::min(1.0f, cut_[cursor_] + amount);
    heat_ += heat;
    if (cut_[cursor_] >= 1) {
        audio_.play("pick");
        for (size_t i = 0; i < cut_.size(); ++i)   // jump to the next uncut point
            if (cut_[(cursor_ + i) % cut_.size()] < 1) {
                cursor_ = static_cast<int>((cursor_ + i) % cut_.size());
                break;
            }
    }
    if (heat_ >= 1) {
        heat_ = 0.6f;
        overheatLock_ = 1.2f;
        cut_[cursor_] = std::max(0.0f, cut_[cursor_] - 0.25f);
        mistake(4);
    }
    if (std::all_of(cut_.begin(), cut_.end(), [](float c) { return c >= 1; })) solve();
}

void CutPuzzle::step(float dt, const Input& in) {
    overheatLock_ = std::max(0.0f, overheatLock_ - dt);
    bool holding = in.acceptHeld() || mouseDown_;
    mouseDown_ = false;
    if (holding && overheatLock_ <= 0) {
        cutBy(0.45f * dt, 0.42f * dt);
        sparkTimer_ -= dt;
        if (sparkTimer_ <= 0) {
            audio_.play("blip");
            sparkTimer_ = 0.08f;
        }
    } else {
        heat_ = std::max(0.0f, heat_ - 0.35f * dt);
    }
}

void CutPuzzle::drawBoard(float x, float y, float w, float h, float t, HitList& hits, const Input& in) {
    (void)h;
    // the metal plate with a seam across it
    float px = x + 10, py = y + 22, pw = w - 20, ph = 70;
    Ui::rect(px - 1, py - 1, pw + 2, ph + 2, pal::PanelEdge);
    Ui::rect(px, py, pw, ph, pal::rgb(0x4A5468));
    for (float i = 0; i < pw; i += 24) Ui::rect(px + i, py, 1, ph, pal::rgb(0x3E4758));
    Ui::rect(px, py + ph / 2 - 1, pw, 3, pal::rgb(0x2A303C));
    for (size_t i = 0; i < cut_.size(); ++i) {
        float cx = px + std::round(spot_[i] * pw);
        float cy = py + ph / 2;
        bool sel = static_cast<int>(i) == cursor_;
        float c = cut_[i];
        Color glow = c >= 1 ? pal::rgb(0x1A1E26) : lerpColor(pal::rgb(0x9A5E10), pal::rgb(0xFFF1C0), c);
        Ui::rect(cx - 9, cy - 9, 18, 18, c >= 1 ? pal::rgb(0x12151C) : pal::rgb(0x6A7488));
        Ui::rect(cx - 7, cy - 7, 14, 14, glow);
        if (c > 0 && c < 1) Ui::rect(cx - 7, cy + 9, std::round(14 * c), 2, pal::AmberLight);
        if (c >= 1) Ui::text("OK", cx - 5, cy - 3, pal::TealHi);
        if (sel && c < 1) {
            Ui::brackets(cx - 12, cy - 12, 24, 24, t, pal::Amber, 5, 1);
            bool hot = (in.acceptHeld() || in.mouseHeld()) && overheatLock_ <= 0;
            if (hot) {   // sparks fly from the torch tip
                Rng r(static_cast<uint32_t>(t * 40));
                for (int s = 0; s < 8; ++s) {
                    float a = r.next() * 6.28f, d = 4 + r.next() * 14;
                    Ui::rect(cx + std::cos(a) * d, cy + std::sin(a) * d, 2, 2, s % 2 ? pal::AmberLight : WHITE);
                }
            }
        }
        int idx = static_cast<int>(i);
        Rectangle box{cx - 14, cy - 14, 28, 28};
        hits.add(box, [this, idx] { cursor_ = idx; cutBy(0.07f, 0.05f); }, [this, idx] { cursor_ = idx; });
        if (sel && in.mouseHeld() && CheckCollisionPointRec(in.mouse(), box)) mouseDown_ = true;
    }
    // heat gauge
    float gy = py + ph + 18;
    Ui::text("HEAT", px, gy, pal::Dim);
    float gx = px + 34, gw = pw - 34;
    Ui::rect(gx, gy, gw, 7, pal::SegEmpty);
    Color hc = heat_ > 0.75f ? pal::Red : heat_ > 0.45f ? pal::Amber : pal::TealHi;
    if (overheatLock_ > 0) hc = (static_cast<int>(t * 10) % 2) ? pal::Red : pal::AmberDark;
    Ui::rect(gx, gy, std::round(gw * std::min(1.0f, heat_)), 7, hc);
    Ui::rect(gx + std::round(gw * 0.75f), gy - 2, 1, 11, pal::Red);
    if (overheatLock_ > 0) Ui::textCentered("OVERHEATED - COOLING", px + pw / 2, gy + 14, pal::Red);
    int done = static_cast<int>(std::count_if(cut_.begin(), cut_.end(), [](float c) { return c >= 1; }));
    Ui::text(std::to_string(done) + "/" + std::to_string(cut_.size()) + " cut", px, py - 14, pal::Ink);
}
