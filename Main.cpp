# include <Siv3D.hpp>

//============================= 共有データ =============================
struct Shared {
	int unlocked = 1;
};

void StopAllAudio()
{
	AudioAsset(U"stage1BGM").stop();
	AudioAsset(U"heartbeat").stop();
	AudioAsset(U"monkeySE").stop();
}

//============================= プレイヤー =============================
struct Player {
	Size  size{ 28, 36 };
	Vec2  pos{ 120, 540 };
	Vec2  prevPos{ 120, 540 };
	Vec2  vel{ 0, 0 };
	bool  grounded = false;
	bool  jumpedThisFrame = false;
	double walkPhase = 0.0;

	double gravity = 1800.0;
	double moveAccel = 2400.0;
	double airAccel = 1400.0;
	double maxSpeedX = 260.0;
	double jumpSpeed = 560.0;
	double groundFric = 14.0;
	double airFric = 2.0;

	bool update(const Array<RectF>& colliders) {
		prevPos = pos;
		jumpedThisFrame = false;
		const bool left = (KeyA.pressed() || KeyLeft.pressed());
		const bool right = (KeyD.pressed() || KeyRight.pressed());
		const bool jumpPressed = (KeySpace.down() || KeyW.down() || KeyUp.down());
		const bool running = KeyShift.pressed();

		double ax = 0.0;
		if (left ^ right) {
			const double a = grounded ? moveAccel : airAccel;
			ax = (left ? -a : a);
		}
		const double maxX = running ? (maxSpeedX * 1.4) : maxSpeedX;
		const double dt = Scene::DeltaTime();

		vel.x += ax * dt;
		vel.x -= vel.x * Min((grounded ? groundFric : airFric) * dt, 1.0);
		vel.x = Clamp(vel.x, -maxX, maxX);

		vel.y += gravity * dt;

		if (grounded && jumpPressed) {
			vel.y = -jumpSpeed;
			grounded = false;
			jumpedThisFrame = true;
		}

		// X 衝突
		pos.x += vel.x * dt;
		RectF aabbX{ pos, size };
		for (const auto& c : colliders) {
			if (!aabbX.intersects(c)) continue;
			if (vel.x > 0) pos.x = c.x - size.x - 0.01;
			else if (vel.x < 0) pos.x = c.x + c.w + 0.01;
			vel.x = 0;
			aabbX.setPos(pos);
		}
		// Y 衝突
		pos.y += vel.y * dt;
		RectF aabbY{ pos, size };
		grounded = false;
		for (const auto& c : colliders) {
			if (!aabbY.intersects(c)) continue;
			if (vel.y > 0) { pos.y = c.y - size.y - 0.01; vel.y = 0; grounded = true; }
			else if (vel.y < 0) { pos.y = c.y + c.h + 0.01; vel.y = 0; }
			aabbY.setPos(pos);
		}
		return true;
	}

	void advanceAnim() {
		const bool isMovingHoriz = (Math::Abs(vel.x) > 1.0);
		if (isMovingHoriz && grounded) {
			walkPhase += Scene::DeltaTime() * Math::TwoPi * (8.0 / 2.0);
		}
	}

	void draw() const {
		const bool isMovingHoriz = (Math::Abs(vel.x) > 1.0);

		Circle{ pos + Vec2{ size.x * 0.5, size.y + 6 }, 12 }
		.draw(ColorF{ 0, 0, 0, grounded ? 0.18 : 0.10 });

		const Vec2 center = pos + Vec2{ size } / 2;
		const double step = grounded ? (6.0 * Math::Sin(walkPhase * 2.0) * (isMovingHoriz ? 1.0 : 0.0)) : 0.0;
		const double swing = (isMovingHoriz && grounded) ? (12.0 * Math::Sin(walkPhase)) : 0.0;
		const double tilt = (isMovingHoriz ? Clamp(vel.x / maxSpeedX, -1.0, 1.0) * 0.12 : 0.0);

		const RoundRect body{ RectF{ Arg::center = center.movedBy(0, -4 + step), size }, 6.0 };
		const Transformer2D t{ Mat3x2::Rotate(tilt, body.rect.center()) };
		body.draw(ColorF{ 0.1, 0.1, 0.12 });

		const Circle head{ center.movedBy(0, -size.y * 0.9 + step), 14 };
		head.draw(ColorF{ 0.95 });

		const Vec2 faceDir = (Math::Abs(vel.x) > Math::Abs(vel.y))
			? Vec2{ (vel.x >= 0 ? 1 : -1), 0 }
		: Vec2{ 0, (vel.y >= 0 ? 1 : -1) };
		const Vec2 eyeOffset = faceDir * Vec2{ 4, 3 } *0.5;
		Circle{ head.center.movedBy(-5, -2) + eyeOffset, 1.6 }.draw(ColorF{ 0.08 });
		Circle{ head.center.movedBy(5, -2) + eyeOffset, 1.6 }.draw(ColorF{ 0.08 });

		const Vec2 armBaseL = body.rect.center().movedBy(-size.x * 0.6, -size.y * 0.2 + step);
		const Vec2 armBaseR = body.rect.center().movedBy(size.x * 0.6, -size.y * 0.2 + step);
		Line{ armBaseL, armBaseL.movedBy(0, 14 + swing * 0.2) }.draw(4, ColorF{ 0.15 });
		Line{ armBaseR, armBaseR.movedBy(0, 14 - swing * 0.2) }.draw(4, ColorF{ 0.15 });

		const Vec2 footBase = body.rect.center().movedBy(0, size.y * 0.5 + step);
		const Vec2 footL = footBase.movedBy(-8 - swing, 8);
		const Vec2 footR = footBase.movedBy(8 + swing, 8);
		Line{ footBase.movedBy(-6, -4), footL }.draw(5, ColorF{ 0.12 });
		Line{ footBase.movedBy(6, -4), footR }.draw(5, ColorF{ 0.12 });
		Circle{ footL, 4 }.draw(ColorF{ 0.2 });
		Circle{ footR, 4 }.draw(ColorF{ 0.2 });
	}
};

//============================= UI =============================
struct UIButton {
	RectF  rect;
	String text;

	double hoverDarken = 0.12;
	double pressInset = 2.0;

	mutable bool wasHovered = false;
	bool enabled = true;

	UIButton(const RectF& r, StringView t) : rect{ r }, text{ t } {}

	// --- 入力なしの描画だけ ---
	void draw(const Font& font) const {
		const bool hovered = enabled && rect.mouseOver();
		const bool pressing = hovered && MouseL.pressed();

		const ColorF base = enabled ? ColorF{ 1.0 } : ColorF{ 0.92 };
		const ColorF frame = enabled ? ColorF{ 0.2 } : ColorF{ 0.7 };
		const ColorF textCol = enabled ? ColorF{ 0.1 } : ColorF{ 0.6 };

		rect.draw(base);
		if (enabled && hovered) { rect.draw(ColorF{ 0,0,0, hoverDarken }); }
		if (enabled && pressing) { rect.stretched(-pressInset).draw(ColorF{ 0,0,0, 0.10 }); }
		rect.drawFrame(2, 0, frame);
		font(text).drawAt(rect.center(), textCol);
	}

	bool drawAndCheck(const Font& font) const {
		const bool hovered = enabled && rect.mouseOver();
		const bool pressing = hovered && MouseL.pressed();
		const bool clicked = hovered && MouseL.down();

		const ColorF base = enabled ? ColorF{ 1.0 } : ColorF{ 0.92 };
		const ColorF frame = enabled ? ColorF{ 0.2 } : ColorF{ 0.7 };
		const ColorF textCol = enabled ? ColorF{ 0.1 } : ColorF{ 0.6 };

		rect.draw(base);
		if (enabled && hovered) { rect.draw(ColorF{ 0,0,0, hoverDarken }); }
		if (enabled && pressing) { rect.stretched(-pressInset).draw(ColorF{ 0,0,0, 0.10 }); }
		rect.drawFrame(2, 0, frame);
		font(text).drawAt(rect.center(), textCol);

		// SE
		if (hovered && !wasHovered)
			AudioAsset(U"UIselectSE").play();
		if (clicked)
			AudioAsset(U"UIenterSE").play();

		wasHovered = hovered;
		return (enabled && clicked);
	}
};

//============================= シーン管理 =============================
enum class State { Title, Select, Stage1, Stage2, Stage3, Stage4, StageLast, EndRoll };
using App = SceneManager<State, Shared>;

static Array<RectF> MakeLevelColliders(const Size sceneSize, const Array<RectF>& platforms) {
	Array<RectF> cols = platforms;
	cols << RectF{ -100, 0, 100, (double)sceneSize.y };
	cols << RectF{ (double)sceneSize.x, 0, 100, (double)sceneSize.y };
	return cols;
}
static void DrawGoal(const RectF& goal) {
	goal.draw(ColorF{ 0.75, 0.94, 0.80 });
	goal.drawFrame(2, 0, ColorF{ 0.15, 0.45, 0.2 });
	Triangle{ goal.pos.movedBy(10, -20), goal.pos.movedBy(10, 0), goal.pos.movedBy(38, -10) }
	.draw(ColorF{ 0.2, 0.7, 0.3 });
	Line{ goal.pos.movedBy(10, -24), goal.pos.movedBy(10, 2) }.draw(3, ColorF{ 0.2, 0.2, 0.2 });
}

//----------------------------- Title -----------------------------
class Title : public App::Scene {
	Font title{ 80, Typeface::Light }, font{ 18 };
	UIButton start{ RectF{ Arg::center = Scene::Center().movedBy(0, 40), 220, 48 }, U"スタート" };
	UIButton select{ RectF{ Arg::center = Scene::Center().movedBy(0, 100), 220, 48 }, U"ステージセレクト" };

	// 背景リング
	struct Ring {
		Vec2 pos; double r, alpha, shrink;
		Ring(Vec2 p) : pos{ p }, r{ Random(280.0, 420.0) }, alpha{ 0.35 }, shrink{ Random(0.985, 0.992) } {}
		bool update() { r *= shrink; alpha *= 0.97; return (alpha > 0.02); }
		void draw() const { Circle(pos, r).drawFrame(r * 0.25, ColorF{ 0.5, 0.5, 0.5, alpha }); }
	};

	//==================== メンバ ====================
	Array<Ring> rings;
	Stopwatch spawn{ StartImmediately::Yes };

	// フェードアウト制御
	bool fading = false;
	Stopwatch fadeSW{ StartImmediately::No };
	double fadeOutSec = 0.6;

public:
	Title(const InitData& init)
		: IScene{ init }
	{
		AudioAsset::Register(U"UIenterSE", U"Assets/UIenterSE.mp3");
		AudioAsset::Register(U"UIselectSE", U"Assets/UIselectSE.mp3");
	}

	void update() override
	{
		// 背景リング
		if (spawn.sF() >= 1.5) {
			rings << Ring{ Vec2{ Random(0.0, (double)Scene::Width()),
								 Random(0.0, (double)Scene::Height()) } };
			spawn.restart();
		}
		for (auto& g : rings) g.update();
		rings.remove_if([](const Ring& g) { return g.alpha <= 0.02; });

		// 背景
		Scene::SetBackground(ColorF{ 0.96, 0.98, 1.0 });
		for (const auto& g : rings) g.draw();

		// UI
		if (!fading) {
			if (start.drawAndCheck(font)) { fading = true; fadeSW.restart(); }
			if (select.drawAndCheck(font))
			{
				StopAllAudio();
				changeScene(State::Select, 0.3s);
			}
		}
		else {
			if (fadeSW.sF() >= fadeOutSec) {
				StopAllAudio();
				changeScene(State::Stage1, 0.0s);
			}
		}
		if (KeyEscape.down()) {
			System::Exit();
		}
	}

	void draw() const override
	{
		title(U"シン・ランド").drawAt(Scene::Center().movedBy(0, -60), ColorF{ 0.1 });

		if (fading) {
			const double a = Clamp(fadeSW.sF() / fadeOutSec, 0.0, 1.0);
			RectF(Scene::Rect()).draw(ColorF{ 0, 0, 0, a });
		}
	}
};


//----------------------------- Select -----------------------------
class Select : public App::Scene {
	Font title{ 28, Typeface::Bold }, font{ 18 };
	Array<UIButton> buttons;

public:
	using App::Scene::Scene;

	Select(const InitData& init) : App::Scene(init) {
	AudioAsset::Register(U"UIenterSE", U"Assets/UIenterSE.mp3");
	AudioAsset::Register(U"UIselectSE", U"Assets/UIselectSE.mp3");

		// 3行×4列で12個配置
		const int cols = 4, rows = 3;
		const double w = 160, h = 46;
		const double gapX = 40, gapY = 60;
		const double startX = 140;
		const double startY = 160;

		buttons.reserve(12);
		for (int r = 0; r < rows; ++r) {
			for (int c = 0; c < cols; ++c) {
				const int idx = r * cols + c;
				const RectF rect{ startX + c * (w + gapX), startY + r * (h + gapY), w, h };
				String label = (idx < 11) ? (U"ステージ {}"_fmt(idx + 1)) : U"ラスト";
				buttons << UIButton{ rect, label };
			}
		}
	}

	void update() override {
		const int unlocked = getData().unlocked;
		for (int i = 0; i < (int)buttons.size(); ++i) {
			buttons[i].enabled = ((i + 1) <= unlocked);
		}

		for (int i = 0; i < (int)buttons.size(); ++i) {
			if (buttons[i].drawAndCheck(font)) {
				if (i < 11) {
					StopAllAudio();
					changeScene(static_cast<State>(static_cast<int>(State::Stage1) + i), 0.2s);
				}
				else {
					StopAllAudio();
					changeScene(State::StageLast, 0.2s);
				}
				break;
			}
		}

		if (KeyEscape.down())
		{
			StopAllAudio();
			changeScene(State::Title, 0.2s);
		}
	}

	void draw() const override {
		Scene::SetBackground(ColorF{ 0.95, 0.98, 1.0 });
		title(U"ステージセレクト").draw(20, 16, ColorF{ 0.15 });
		for (const auto& b : buttons) b.draw(font);
	}
};

//============================= ステージ基底 =============================
class StageBase : public App::Scene {
protected:
	Font font{ 18 }, head{ 22, Typeface::Bold };
	const Size sceneSize{ 960, 640 };
	Array<RectF> platforms;
	Array<RectF> colliders;
	RectF goal{ 840, 520, 80, 60 };
	Player player;

	virtual void drawBackground() const {
		Scene::SetBackground(ColorF{ 0.95, 0.98, 1.0 });
	}

	void drawLevel() const {
		for (const auto& pf : platforms) {
			pf.draw(ColorF{ 0.75, 0.78, 0.82 });
			pf.drawFrame(2, 0, ColorF{ 0.2, 0.25, 0.3, 0.4 });
		}
	}

	void uiCommon(const String& name) const {
		(void)name;
	}

public:
	using App::Scene::Scene;

	void update() override {
		player.update(colliders);
		player.advanceAnim();

		if (KeyEscape.down())
		{
			StopAllAudio();
			changeScene(State::Title, 0.2s);
		}
	}

	void draw() const override {
		drawBackground();

		drawLevel();
		player.draw();
	}

	virtual void onClear() = 0;
};

//---------------------------------- Stage1 -----------------------------
class Stage1 : public
	StageBase {
private:
	// ---- サル ----
	struct Monkey {
		enum class Phase { Waiting, Entering, Eating, Leaving };
		Phase phase = Phase::Waiting;
		double t = 0.0;
		Vec2 pos{ 1000, 520 };
		int fruitIndex = 0;
		bool triggered = false;

		void startIfTriggered(const Vec2& playerPos) {
			if (!triggered && playerPos.x > 400) {
				triggered = true; phase = Phase::Entering; t = 0.0;
				pos = Vec2{ 1000, 520 }; fruitIndex = 0;
				AudioAsset(U"monkeySE").play();
			}
		}
		void update(double dt) {
			switch (phase) {
			case Phase::Waiting: break;
			case Phase::Entering:
				pos.x -= 100 * dt; t += dt;
				if (pos.x <= 760) { pos.x = 760; phase = Phase::Eating; t = 0.0; fruitIndex = 0; }
				break;
			case Phase::Eating:
				t += dt; {
					const double fruitDuration = 1.0;
					fruitIndex = (int)(t / fruitDuration);
					if (fruitIndex > 3) {
						phase = Phase::Leaving; t = 0.0;
						AudioAsset(U"monkeySE").play();
					}
				}
				break;
			case Phase::Leaving:
				pos.x += 140 * dt; t += dt;
				if (pos.x > 1000) { phase = Phase::Waiting; t = 0.0; }
				break;
			}
			if (phase == Phase::Waiting && triggered) {
				t += dt; if (t > 2.0) {
					phase = Phase::Entering; t = 0.0; pos = Vec2{ 1000,520 }; fruitIndex = 0;
					AudioAsset(U"monkeySE").play();
				}
			}
		}

		// 果物描画（サルと木で共有）
		static void DrawFruit(int fruitIndex, const Vec2& p) {
			switch (fruitIndex) {
			case 0: { // りんご
				Circle{ p, 10 }.draw(ColorF{ 0.9, 0.05, 0.05 });
				Circle{ p.movedBy(-3,-3), 4 }.draw(ColorF{ 1.0, 0.8, 0.8, 0.6 });
				RectF{ p.movedBy(-1,-14), 2, 6 }.draw(ColorF{ 0.2,0.3,0.2 });
				Triangle{ p.movedBy(0,-14), p.movedBy(6,-18), p.movedBy(2,-20) }.draw(ColorF{ 0.2,0.5,0.2 });
			} break;
			case 1: { // ばなな
				const Vec2 b0 = p.movedBy(-16, 0);
				const Vec2 b1 = p.movedBy(0, -10);
				const Vec2 b2 = p.movedBy(16, -2);
				for (int i = -2; i <= 2; ++i)
					Bezier2{ b0.movedBy(0,i), b1.movedBy(0,i), b2.movedBy(0,i) }.draw(4, ColorF{ 0.98, 0.9, 0.3 });
				Circle{ b2, 3 }.draw(ColorF{ 0.4,0.3,0.1 });
			} break;
			case 2: { // もも
				Circle{ p.movedBy(-5,0), 11 }.draw(ColorF{ 1.0, 0.75, 0.8 });
				Circle{ p.movedBy(5,0), 11 }.draw(ColorF{ 1.0, 0.70, 0.7 });
				RectF{ p.movedBy(-1,-8), 2, 16 }.draw(ColorF{ 1.0,0.8,0.85,0.4 });
				Triangle{ p.movedBy(0,-12), p.movedBy(8,-16), p.movedBy(2,-18) }.draw(ColorF{ 0.4,0.7,0.4 });
			} break;
			case 3: { // ぶどう
				const ColorF grape{ 0.5,0.2,0.7 };
				Circle{ p.movedBy(0,-6), 6 }.draw(grape);
				Circle{ p.movedBy(-6, 0), 6 }.draw(grape);
				Circle{ p.movedBy(6, 0), 6 }.draw(grape);
				Circle{ p.movedBy(0, 6), 6 }.draw(grape);
				Circle{ p.movedBy(-4,10), 5 }.draw(grape);
				Circle{ p.movedBy(4,10), 5 }.draw(grape);
				RectF{ p.movedBy(-1,-16), 2, 6 }.draw(ColorF{ 0.2,0.3,0.2 });
			} break;
			}
		}

		void draw() const
		{
			if (phase == Phase::Waiting && !triggered) return;

			// 明るめ茶系
			const ColorF furColor{ 0.55, 0.33, 0.18 };
			const ColorF faceColor{ 0.97, 0.90, 0.80 };
			const ColorF eyeColor{ 0.10, 0.07, 0.07 };
			const double groundY = 580.0;
			const Vec2 c = pos;

			auto drawSitting = [&](const Vec2& cPos, bool holdingFruit)
				{
					const double bodyBottom = groundY;
					const double bodyTop = bodyBottom - 28;
					const double bodyCenterY = (bodyTop + bodyBottom) / 2;
					const double faceY = bodyTop - 12;

					// 頭（外＝毛 / 内＝顔）
					Circle{ Vec2{ cPos.x, faceY }, 16 }.draw(furColor);
					Circle{ Vec2{ cPos.x, faceY + 2 }, 12 }.draw(faceColor);
					// 耳
					Circle{ Vec2{ cPos.x - 16, faceY + 2 }, 6 }.draw(furColor);
					Circle{ Vec2{ cPos.x + 16, faceY + 2 }, 6 }.draw(furColor);
					// 目
					Circle{ Vec2{ cPos.x - 4, faceY }, 2 }.draw(eyeColor);
					Circle{ Vec2{ cPos.x + 4, faceY }, 2 }.draw(eyeColor);
					// 胴体
					RoundRect{ RectF{ Arg::center = Vec2{ cPos.x, bodyCenterY }, 30, 28 }, 6 }.draw(furColor);
					// 足
					RectF{ cPos.x - 12, bodyBottom - 14, 10, 14 }.draw(furColor);
					RectF{ cPos.x + 2, bodyBottom - 14, 10, 14 }.draw(furColor);
					RoundRect{ RectF{ cPos.x - 14, groundY - 8, 10, 8 }, 2 }.draw(furColor);
					RoundRect{ RectF{ cPos.x + 2, groundY - 8, 10, 8 }, 2 }.draw(furColor);
					// しっぽ
					{
						const Vec2 base = Vec2{ cPos.x + 16, bodyBottom - 16 };
						Bezier2{ base, base.movedBy(10,-10), base.movedBy(0,-20) }.draw(4, furColor);
					}
					// 腕
					if (holdingFruit) {
						RectF{ cPos.x - 20, faceY + 4, 6, -24 }.draw(furColor);
						RectF{ cPos.x + 14, faceY + 4, 6, -24 }.draw(furColor);
						DrawFruit(fruitIndex, Vec2{ cPos.x, faceY - 28 });
					}
					else {
						RectF{ cPos.x - 16, bodyBottom - 20, 8, 16 }.draw(furColor);
						RectF{ cPos.x + 8, bodyBottom - 20, 8, 16 }.draw(furColor);
					}
				};

			auto drawWalking = [&](const Vec2& cPos)
				{
					const double swing = Math::Sin(t * 8.0) * 6.0;
					const double bodyBottom = 580.0;
					const double bodyTop = bodyBottom - 36;
					const double bodyCenterY = (bodyTop + bodyBottom) / 2;
					const double faceY = bodyTop - 14;

					Circle{ Vec2{ cPos.x, faceY }, 16 }.draw(furColor);
					Circle{ Vec2{ cPos.x, faceY + 2 }, 12 }.draw(faceColor);
					Circle{ Vec2{ cPos.x - 16, faceY + 2 }, 6 }.draw(furColor);
					Circle{ Vec2{ cPos.x + 16, faceY + 2 }, 6 }.draw(furColor);
					Circle{ Vec2{ cPos.x - 4, faceY }, 2 }.draw(eyeColor);
					Circle{ Vec2{ cPos.x + 4, faceY }, 2 }.draw(eyeColor);

					RoundRect{ RectF{ Arg::center = Vec2{ cPos.x, bodyCenterY }, 32, 36 }, 6 }.draw(furColor);

					RectF{ cPos.x - 22, bodyTop + 4 + swing * 0.3, 6, 20 }.draw(furColor);
					RectF{ cPos.x + 16, bodyTop + 4 - swing * 0.3, 6, 20 }.draw(furColor);

					RectF{ cPos.x - 8, bodyBottom - 8 + swing * 0.5, 6, 8 }.draw(furColor);
					RectF{ cPos.x + 2, bodyBottom - 8 - swing * 0.5, 6, 8 }.draw(furColor);

					const Vec2 base = Vec2{ cPos.x + 18, bodyBottom - 20 + swing * 0.2 };
					Bezier2{ base, base.movedBy(10,-12), base.movedBy(0,-28) }.draw(4, furColor);
				};

			if (phase == Phase::Entering || phase == Phase::Leaving) {
				drawWalking(pos);
			}
			else {
				const bool holdingFruit = (phase == Phase::Eating);
				drawSitting(pos, holdingFruit);
			}
		}
	};

	Monkey monkey;

	// ---- 謎解き：木に生る果物の並び ----
	Array<int> fruits{ 0, 1, 2, 3 };
	const Array<int> answer{ 0, 1, 2, 3 };
	Array<Vec2> fruitSlots;

	// ---- ジャンプ踏みスイッチ ----
	RectF swSwap;     // 左右端スワップ
	RectF swRotate;   // 右に2つローテート
	bool  swSwapPrev = false;
	bool  swRotatePrev = false;

	RectF door{ 20, 500, 60, 80 };
	bool  doorAppeared = false;

	// ===== フェード =====
	double fadeInAlpha = 1.0;
	const double fadeInSec = 0.6;

	bool   clearing = false;
	double clearT = 0.0;
	const double fadeOutSec = 0.7;

	static void drawPad(const RectF& r, bool pressed) {
		const ColorF base = pressed ? ColorF{ 0.65,0.7,0.75 } : ColorF{ 0.8,0.85,0.9 };
		r.draw(base);
		r.drawFrame(2, 0, ColorF{ 0.22,0.26,0.3,0.8 });
		r.stretched(-6, -8).drawFrame(2, 0, ColorF{ 0.22,0.26,0.3,0.25 });
	}

	bool landedOn(const RectF& r) const {
		const double prevBottom = player.prevPos.y + player.size.y;
		const double nowBottom = player.pos.y + player.size.y;
		const bool   goingDown = (player.vel.y >= 0);
		const bool   horizontal =
			(player.pos.x + player.size.x > r.x) &&
			(r.x + r.w > player.pos.x);
		return (prevBottom <= r.y) && (nowBottom >= r.y) && goingDown && horizontal;
	}

public:
	using StageBase::StageBase;

	Stage1(const InitData& init) : StageBase(init) {
		platforms = { RectF{ 0, 580, 960, 60 }, };
		colliders = MakeLevelColliders(sceneSize, platforms);
		player = Player{}; player.pos = Vec2{ 80, 540 };

		// 木の横一列スロット
		const double cx = sceneSize.x * 0.5;
		const double y = 210;
		const double step = 48;
		fruitSlots = {
			Vec2{ cx - 1.5 * step, y },
			Vec2{ cx - 0.5 * step, y },
			Vec2{ cx + 0.5 * step, y },
			Vec2{ cx + 1.5 * step, y },

		};

		// SE
		AudioAsset::Register(U"clearSE", U"Assets/clearSE.mp3");
		AudioAsset(U"clearSE").setVolume(0.9);
		AudioAsset::Register(U"doorSE", U"Assets/doorSE.mp3");
		AudioAsset::Register(U"monkeySE", U"Assets/monkeySE.mp3");
		AudioAsset(U"monkeySE").setVolume(0.4);
		AudioAsset::Register(U"buttonSE", U"Assets/buttonSE.mp3");
		AudioAsset(U"buttonSE").setVolume(1.4);

		// BGM 
		AudioAsset::Register(U"stage1BGM", U"Assets/stage1BGM.mp3");
		AudioAsset(U"stage1BGM").setLoop(true);
		AudioAsset(U"stage1BGM").setVolume(0.15);
		AudioAsset(U"stage1BGM").play();


		// スイッチ（中央付近）
		swSwap = RectF{ 420, 560, 48, 20 };
		swRotate = RectF{ 500, 560, 48, 20 };

		// 初期配置をランダム（正解と一致しないまでシャッフル）
		do { Shuffle(fruits); } while (fruits == answer);

		doorAppeared = false;

		// フェードイン開始状態
		fadeInAlpha = 1.0;
		clearing = false;
		clearT = 0.0;
	}


	// --- 背景（森＋象徴の木＋横一列の木の実） ---
	void drawBackground() const override {
		const double groundLineY = 560.0;

		RectF{ 0,0,(double)sceneSize.x,(double)sceneSize.y }
		.draw(Arg::top = ColorF{ 0.88,0.95,1.0 }, Arg::bottom = ColorF{ 0.82,0.93,0.86 });

		// 奥の森
		{
			const ColorF farTree{ 0.45,0.55,0.45,0.35 };
			for (int i = 0; i < 14; ++i) {
				const double bx = (i * 80.0) + (i % 3 * 12.0);
				const double cy = 140.0 + (i % 4 * 8.0);
				Circle{ bx + 38, cy + 0, 70 }.draw(farTree);
				Circle{ bx + 14, cy + 20, 60 }.draw(farTree);
				Circle{ bx + 60, cy + 28, 58 }.draw(farTree);
				RectF{ bx + 30, cy + 40, 24, groundLineY - (cy + 40) }.draw(farTree);
			}
		}
		// 手前の森（薄め）
		{
			const ColorF nearTree{ 0.35,0.45,0.35,0.4 };
			for (int i = 0; i < 10; ++i) {
				const double bx = (i * 110.0) + ((i % 2) * 25.0);
				const double cy = 100.0 + (i % 3 * 10.0);
				Circle{ bx + 38, cy - 20, 90 }.draw(nearTree);
				Circle{ bx + 8, cy + 10, 75 }.draw(nearTree);
				Circle{ bx + 70, cy + 18, 70 }.draw(nearTree);
				RectF{ bx + 28, cy + 30, 24, groundLineY - (cy + 30) }.draw(nearTree);
			}
		}

		// 象徴の木
		{
			const double cx = sceneSize.x * 0.5;
			const double crownBaseY = 180.0;
			const double trunkW = 48.0;
			const ColorF leaf{ 0.16,0.24,0.16,0.8 }, trunk{ 0.10,0.16,0.10,0.85 };

			Circle{ Vec2{cx - 60,crownBaseY + 20},70 }.draw(leaf);
			Circle{ Vec2{cx + 60,crownBaseY + 20},70 }.draw(leaf);
			Circle{ Vec2{cx,   crownBaseY - 10},80 }.draw(leaf);
			Circle{ Vec2{cx - 90,crownBaseY + 50},60 }.draw(leaf);
			Circle{ Vec2{cx + 90,crownBaseY + 50},60 }.draw(leaf);

			const double tTop = crownBaseY + 40, tBot = 560;
			RectF{ cx - trunkW * 0.5, tTop, trunkW, tBot - tTop }.draw(trunk);
			Circle{ Vec2{ cx - trunkW * 0.7, tTop + 20 }, 30 }.draw(leaf);
			Circle{ Vec2{ cx + trunkW * 0.7, tTop + 20 }, 30 }.draw(leaf);
		}

		// 木の実（横一列）
		for (size_t i = 0; i < fruits.size(); ++i) {
			Monkey::DrawFruit(fruits[i], fruitSlots[i]);
		}

		// 地面帯＋薄霧
		RectF{ 0, groundLineY, sceneSize.x, (double)sceneSize.y - groundLineY }.draw(ColorF{ 0.06,0.08,0.06,0.8 });
		RectF{ 0, 0, sceneSize.x, sceneSize.y }.draw(ColorF{ 1,1,1,0.04 });
	}

	// --- 更新 ---
	void update() override
	{
		const double dt = Scene::DeltaTime();

		if (!clearing) {
			player.update(colliders);
			player.advanceAnim();
		}
		else {
			player.vel = Vec2{ 0,0 };
		}

		monkey.startIfTriggered(player.pos);
		monkey.update(dt);

		if (fadeInAlpha > 0.0) {
			fadeInAlpha = Max(0.0, fadeInAlpha - dt / fadeInSec);
		}

		if (clearing) {
			clearT += dt;
			if (clearT >= fadeOutSec) {
				getData().unlocked = Max(getData().unlocked, 2);
				TextWriter writer{ U"Assets/SaveData.txt" };
				if (writer)
				{
					writer.writeln(Format(getData().unlocked));
				}
				writer.close();
				StopAllAudio();
				changeScene(State::Stage2, 0s);
				return;
			}
		}

		// クリア前のみパズル操作を有効
		if (!clearing) {
			// 踏み検出（ジャンプで上から着地した瞬間のみ反応）
			const bool pressSwap = landedOn(swSwap);
			const bool pressRotate = landedOn(swRotate);

			if (pressSwap && !swSwapPrev) {
				AudioAsset(U"buttonSE").play();
				std::swap(fruits[0], fruits[3]); // 端同士スワップ
			}
			if (pressRotate && !swRotatePrev) {
				AudioAsset(U"buttonSE").play();
				Array<int> next(4);
				for (int i = 0; i < 4; ++i) {
					next[(i + 1) % 4] = fruits[i];  // 右に1つずらす
				}
				fruits = next;
			}
			swSwapPrev = pressSwap;
			swRotatePrev = pressRotate;

			// 正解になった瞬間に扉「出現」
			if (!doorAppeared && (fruits == answer)) {
				doorAppeared = true;
				AudioAsset(U"doorSE").play();
			}

			// 出現済みの扉に触れたら → SE 再生＋フェードアウト開始
			if (doorAppeared && RectF{ player.pos, player.size }.intersects(door)) {
				clearing = true;
				clearT = 0.0;
				AudioAsset(U"clearSE").play();
				AudioAsset(U"stage1BGM").stop();
			}
		}

		if (!clearing && KeyEscape.down()) {
			AudioAsset(U"monkeySE").stop();
			StopAllAudio();
			changeScene(State::Title, 0.2s);
		}
	}


	// --- 描画（レイヤー順：背景 → 地面 → サル（奥） → パッド/扉 → プレイヤー（手前）） ---
	void draw() const override
	{
		drawBackground();
		drawLevel();
		monkey.draw();

		drawPad(swSwap, swSwapPrev);
		drawPad(swRotate, swRotatePrev);

		if (doorAppeared) {
			door.draw(Palette::White);
			door.drawFrame(4, 0, ColorF{ 0.15,0.5,0.25 });
			RectF{ door.x + 6, door.y + 6, door.w - 12, door.h - 12 }.draw(ColorF{ 0.85,1.0,0.9,0.35 });
		}

		player.draw();

		if (fadeInAlpha > 0.0) {
			RectF{ 0,0, (double)sceneSize.x, (double)sceneSize.y }.draw(ColorF{ 0,0,0, fadeInAlpha });
		}
		if (clearing) {
			const double a = Saturate(clearT / fadeOutSec);
			RectF{ 0,0, (double)sceneSize.x, (double)sceneSize.y }.draw(ColorF{ 0,0,0, a });
		}
	}

	void onClear() override {}
};


//----------------------------- Stage2 -----------------------------
class Stage2 : public StageBase {
public:
	using StageBase::StageBase;

private:
	// --- 拍同期 ---
	double heartHz = 1.1;     // 背景・判定・SFX すべてこの周波数
	double t0 = 0.0;          // シーン開始時刻（基準）
	static constexpr double peakPhase = 0.25;

	// 連打ゲージ
	int combo = 0;
	static constexpr int kGoalCombo = 10;

	RectF door{ 40, 500, 60, 80 };
	bool  doorAppeared = false;

	// --- 拍ユーティリティ ---
	double beatTime() const { return (Scene::Time() - t0); }
	double period() const { return 1.0 / heartHz; }
	double phase()  const {
		double cyc = beatTime() * heartHz;
		return (cyc - Math::Floor(cyc));
	}
	double beatEnvelope() const {
		return (Math::Sin(Math::TwoPi * heartHz * beatTime()) * 0.5 + 0.5);
	}
	bool justHitPeakThisFrame() const {
		const double p = period();
		const double a = beatTime() - p * peakPhase;
		const double b = a - Scene::DeltaTime();
		const int nA = (int)Math::Floor(a / p + 0.5);
		const int nB = (int)Math::Floor(b / p + 0.5);
		return (nA != nB);
	}
	bool isOnBeatFrames(int frames) const {
		const double p = period();
		const double x = beatTime() - 0.5 - p * peakPhase;
		const double nearest = p * Math::Round(x / p);
		const double tol = frames * Scene::DeltaTime();
		return (Abs(x - nearest) <= tol);
	}
	bool doorSEPlayed = false;

public:
	Stage2(const InitData& init) : StageBase(init) {
		platforms = { RectF{ 0, 580, 960, 60 }, };
		colliders = MakeLevelColliders(sceneSize, platforms);
		player = Player{}; player.pos = Vec2{ 60, 540 };

		doorAppeared = false;
		combo = 0;

		t0 = Scene::Time();
		AudioAsset::Register(U"heartbeat", U"Assets/heartbeats.mp3");
		AudioAsset(U"heartbeat").setVolume(0.8);
		AudioAsset::Register(U"clearSE", U"Assets/clearSE.mp3");
		AudioAsset(U"clear").setVolume(0.9);
		AudioAsset::Register(U"doorSE", U"Assets/doorSE.mp3");
	}


	void update() override {
		StageBase::update();


		if (justHitPeakThisFrame()) {
			if (!doorSEPlayed)
				AudioAsset(U"heartbeat").play();
		}

		if (player.jumpedThisFrame) {
			if (isOnBeatFrames(9)) {
				combo = Min(combo + 1, kGoalCombo);
				if (combo >= kGoalCombo) doorAppeared = true;
			}
			else {
				combo = 0;
			}
		}
		if (doorAppeared && !doorSEPlayed) {
			AudioAsset(U"heartbeat").stop();
			AudioAsset(U"doorSE").play();
			doorSEPlayed = true;
		}

		if (doorAppeared && RectF{ player.pos, player.size }.intersects(door)) {
			onClear();
		}
	}


	// 心臓
	void drawBackground() const override
	{
		RectF{ 0,0,(double)sceneSize.x,(double)sceneSize.y }
		.draw(Arg::top = ColorF{ 0.92,0.96,1.0 }, Arg::bottom = ColorF{ 1.0,0.92,0.96 });

		const double beat = beatEnvelope();
		const double scale = 1.0 + 0.03 * beat;

		const Vec2   C = Vec2{ sceneSize.x * 0.5, sceneSize.y * 0.48 };
		const double S = 170.0 * scale;

		Ellipse{ C.movedBy(10, 18), 140 * scale, 46 * scale }.draw(ColorF{ 0,0,0,0.08 });

		const Polygon heart = Shape2D::Heart(S, C);
		heart.draw(ColorF{ 0.90, 0.25, 0.35 });
		heart.scaledAt(C, 0.92).draw(ColorF{ 0.85, 0.18, 0.30, 0.9 });
		heart.drawFrame(4, ColorF{ 0.70, 0.10, 0.20, 0.35 });

		const double a = 0.20 + 0.10 * beat;
		Ellipse{ C.movedBy(-S * 0.25, -S * 0.20), S * 0.55, S * 0.38 }.draw(ColorF{ 1.0, 0.95, 0.98, a * 0.7 });
		Ellipse{ C.movedBy(-S * 0.10, -S * 0.30), S * 0.25, S * 0.18 }.draw(ColorF{ 1.0, 1.0, 1.0, a * 0.35 });

		const ColorF tube{ 0.75, 0.18, 0.28, 0.7 };

		Bezier2{ C.movedBy(-S * 0.08, -S * 0.55), C.movedBy(-S * 0.22, -S * 0.68), C.movedBy(-S * 0.35, -S * 0.50) }.draw(10, tube);
		Bezier2{ C.movedBy(S * 0.05, -S * 0.55), C.movedBy(S * 0.22, -S * 0.70), C.movedBy(S * 0.34, -S * 0.56) }.draw(8, tube);

		heart.drawFrame(14, ColorF{ 1.0, 0.6, 0.7, 0.06 });
	}


	// 描画順：背景（心臓）→ 地面 → ゴール → プレイヤー
	void draw() const override {
		drawBackground();
		drawLevel();

		// --- リズムゲージ ---
		{
			const Vec2 base = Vec2{ Scene::CenterF().x - 200, 24 };
			const double w = 36, h = 10, gap = 6;
			for (int i = 0; i < kGoalCombo; ++i) {
				const RectF r{ base.x + i * (w + gap), base.y, w, h };
				if (i < combo) {
					r.stretched(0, 2).draw(ColorF{ 0.9, 0.2, 0.3, 0.9 });
				}
				else {
					r.draw(ColorF{ 0.92, 0.92, 0.95, 0.7 });
					r.drawFrame(1.5, ColorF{ 0.5, 0.5, 0.6, 0.6 });
				}
			}
			const double t = Scene::Time();
			const double p = (t * heartHz) - Math::Floor(t * heartHz); // 0..1
			const double x = base.x + (w + gap) * (kGoalCombo * Math::Clamp(p, 0.0, 1.0));
			Line{ x, base.y - 6, x, base.y + h + 6 }.draw(2, ColorF{ 0.8,0.3,0.4,0.25 });
		}
		if (doorAppeared) {
			door.drawFrame(4, ColorF{ 0.15,0.5,0.25 });
			RectF{ door.x + 6, door.y + 6, door.w - 12, door.h - 12 }
			.draw(ColorF{ 0.85,1.0,0.9,0.35 });
		}
		player.draw();

		uiCommon(U"ステージ 2");
	}

	void onClear() override {
		AudioAsset(U"clearSE").play();
		getData().unlocked = Max(getData().unlocked, 3);
		TextWriter writer{ U"Assets/SaveData.txt" };
		if (writer)
		{
			writer.writeln(Format(getData().unlocked));
		}
		writer.close();
		StopAllAudio();
		changeScene(State::Stage3, 0.5s);
	}
};

//----------------------------- Stage3 -----------------------------
class Stage3 : public StageBase {
public:
	using StageBase::StageBase;

private:
	// ===== 右向きシャーペン（本体=固定長 / 芯=段階的に伸びる） =====
	struct PencilBridge {
		Vec2   origin;              // 左端（床yは origin.y）
		double bodyLen = 160;     // 本体の固定長（スタート島の幅）
		double leadStep = 80;      // ボタン1回で伸びる芯の長さ
		double maxLead = 560;     // 芯の最大長
		int    presses = 0;       // 押下回数
		int    hp = 3;       // ひび演出用（お好み）

		// 形状定数
		static constexpr double H = 20;  // 本体の見た目高さ
		static constexpr double bodyCut = 16;  // 先端金具ぶん本体を短く
		static constexpr double tipLen = 18;  // 金属口金の長さ
		static constexpr double leadDrawH = 4;   // 芯の見た目の太さ（細く）
		static constexpr double leadColH = 6;   // 芯の当たり判定の厚み（遊びやすさ用）
		static constexpr double leadRise = 0;   // 芯の縦微調整

		// 幾何ユーティリティ
		double bodyWidth()  const { return Max(0.0, bodyLen - bodyCut); }
		double tipBaseX()   const { return origin.x + bodyWidth(); }
		double leadStartX() const { return tipBaseX() + tipLen; }
		double leadLength() const { return Min(presses * leadStep, maxLead); }

		// コライダ
		RectF colliderBody() const {                // 本体（固定）
			return RectF{ origin.x, origin.y - 8, bodyWidth(), 10 };
		}
		RectF colliderLead() const {                // 芯（可変）
			return RectF{ leadStartX(),
						   origin.y - (leadColH * 0.5) + leadRise,
						   Max(0.0, leadLength()), leadColH };
		}

		// ノック（push）矩形：ボタン配置に使用
		RectF capRect() const {
			const double y = origin.y - H * 0.5;
			const double capX = origin.x - 8;
			return RectF{ capX - 6, y + 2, 10, H - 4 };
		}

		// 伸長・リセット
		bool extendOnce() {
			if (leadLength() >= maxLead) return false;
			++presses;
			return true;
		}
		void reset() { presses = 0; }

		void applyImpact(double vy) {
			const double impact = Abs(Min(vy, 0.0));
			if (impact > 340) hp = Max(0, hp - 2);
			else if (impact > 200) hp = Max(0, hp - 1);
		}

		// 描画（本体固定＋口金＋細い芯＋ノック）
		void draw() const {
			const double y = origin.y - H * 0.5;

			// 本体（濃い緑 / 固定）
			RectF{ origin.x, y, bodyWidth(), H }
				.draw(ColorF{ 0.08, 0.35, 0.18 })
				.drawFrame(2, ColorF{ 0.05, 0.18, 0.10, 0.9 });
			RectF{ origin.x + 6, y + 2, Max(0.0, bodyWidth() - 12), 6 }.draw(ColorF{ 1,1,1,0.05 });
			RectF{ origin.x + 6, y + H - 8, Max(0.0, bodyWidth() - 12), 6 }.draw(ColorF{ 0,0,0,0.08 });

			// 先端の金属口金（固定）
			const double tipY = y + H * 0.5;
			Triangle{ Vec2{ tipBaseX(), y }, Vec2{ tipBaseX() + tipLen, tipY }, Vec2{ tipBaseX(), y + H } }
				.draw(ColorF{ 0.85,0.86,0.90 })
				.drawFrame(2, ColorF{ 0.5,0.55,0.6,0.6 });

			// 芯（見た目は細く / コライダは colliderLead）
			const double L = leadLength();
			RectF{ leadStartX(), tipY - (leadDrawH * 0.5) + leadRise, Max(0.0, L), leadDrawH }
			.draw(ColorF{ 0.12,0.12,0.14, 0.95 });

			// 後端ノック（push）
			{
				const RectF cap = capRect();
				RoundRect{ cap, 3 }
					.draw(ColorF{ 0.10,0.12,0.14 })
					.drawFrame(2, ColorF{ 0.3,0.3,0.36,0.6 });
			}

			// ひび演出（任意）
			if (hp <= 2) Line{ origin.movedBy(18, y + 6), Vec2{ origin.x + bodyWidth() - 18, y + 12 } }.draw(2, ColorF{ 0,0,0,0.35 });
			if (hp <= 1) Line{ origin.movedBy(30, y + 14), Vec2{ origin.x + bodyWidth() - 28, y + 4 } }.draw(2, ColorF{ 0,0,0,0.45 });
		}
	};

	// ===== 折れた芯（落下アニメ用） =====
	struct LeadFragment {
		Vec2 pos;
		double w, h;
		Vec2 vel;
		double angle = 0.0;   // 傾き角度（固定）
		bool active = false;

		void init(const RectF& srcRect) {
			// 少し太く＆わずかに右にずらしてスタート
			pos = srcRect.pos.movedBy(0, -1);     // わずかに上補正
			w = srcRect.w;
			h = srcRect.h * 0.8;
			vel = Vec2{ Random(60.0, 90.0), -50 }; // 右方向に初速
			angle = Random(6_deg, 14_deg);         // 少し右に傾く固定角
			active = true;
		}

		void update(double dt) {
			if (!active) return;
			vel.y += 980 * dt;     // 重力
			pos += vel * dt;
			if (pos.y > Scene::Height() + 100) active = false;
		}

		void draw() const {
			if (!active) return;
			RectF{ pos, w, h }
				.rotated(angle)
				.draw(ColorF{ 0.1, 0.1, 0.12, 0.95 });
		}
	};


	LeadFragment brokenLead;


	// ===== ステージ要素 =====
	PencilBridge pencil;          // スタート島（シャーペン）
	RectF        doorPad;         // ドア島（幅=80=ドア±10）
	RectF        door;            // 60×80（共通）

	// ノック部ボタン
	RectF        button;
	Stopwatch    buttonCD{ StartImmediately::No };
	double       buttonCooldown = 0.20;
	bool         buttonPrevOverlap = false;

	// リスポーン
	Vec2         startPos;

	// 折れロジック調整
	double breakMinFall = 3.0;     // 落差しきい値（px）
	double breakMinVy = 80.0;   // 着地直前の下向き速度しきい値
	double leadRootSafeLen = 24.0; // 口金直後は安全帯（ここでは折れない）

	// 状態
	bool wasOnLead = false;

	// “ジャンプからの着地のみ”を拾う（水平移動で乗っただけは false）
	bool landedFromJumpOn(const RectF& r) const {
		const double eps = 1.0; // 解決誤差許容
		const double prevB = player.prevPos.y + player.size.y;
		const double nowB = player.pos.y + player.size.y;
		const double dy = nowB - prevB;             // 下向きが+
		const bool crossed = (prevB <= r.y + eps) && (nowB >= r.y - eps);
		const bool goingDown = (player.vel.y >= -0.1); // 解決後の微負値も許容
		const bool horiz = (player.pos.x + player.size.x > r.x) && (r.x + r.w > player.pos.x);
		const bool fallEnough = (prevB <= r.y - breakMinFall) || (player.vel.y > breakMinVy);
		return crossed && goingDown && horiz && (dy > 0.5) && fallEnough;
	}

	// 芯を折る
	void breakLead() {
		AudioAsset(U"BreakSE").play();
		// 現在の芯の矩形をコピーして破片に
		const RectF leadRect = pencil.colliderLead();
		if (leadRect.w > 0) {
			brokenLead.init(leadRect);
		}

		pencil.reset();               // 芯を消す（長さ0に戻す）
		buttonPrevOverlap = false;    // ボタン再押下を確実に
		wasOnLead = false;
	}


public:
	Stage3(const InitData& init) : StageBase(init) {
		const double groundY = 560.0;

		// スタート島（シャーペン本体は固定長）
		pencil.origin = Vec2{ 60, groundY };
		pencil.bodyLen = 160;
		pencil.leadStep = 80;
		pencil.maxLead = 560;

		// ドア島（幅=80）
		doorPad = RectF{ 770, groundY, 80, 14 };
		door = RectF{ doorPad.centerX() - 30, doorPad.y - 80, 60, 80 };

		// 固定床（ドア島のみ）※ペンは動的コライダで追加
		platforms = { doorPad };
		colliders = MakeLevelColliders(sceneSize, platforms);

		// プレイヤー初期位置（少し右にシフト）
		player = Player{};
		startPos = Vec2{ pencil.origin.x + 24, pencil.origin.y - player.size.y }; // ← +24 に
		player.pos = startPos;

		// ボタン：ノック（push）部分の上に配置
		{
			const RectF cap = pencil.capRect();
			button = RectF{ cap.centerX() - 12, cap.y - 8, 24, 6 };
		}

		// SE
		AudioAsset::Register(U"BreakSE", U"Assets/pencilBreakSE.mp3"); 
		AudioAsset(U"Break").setVolume(1.5);
		AudioAsset::Register(U"PushSE", U"Assets/pushPencilSE.mp3"); 
		AudioAsset(U"PushSE").setVolume(1.5);
		AudioAsset::Register(U"clearSE", U"Assets/clearSE.mp3"); 
		AudioAsset(U"clearSE").setVolume(0.9);
	}

	void update() override {
		// 先頭で一度コライダ算出 → player.update に渡す
		const RectF colBody_pre = pencil.colliderBody();
		const RectF colLead_pre = pencil.colliderLead();

		Array<RectF> dyn = platforms;
		dyn << colBody_pre << colLead_pre << button;
		dyn << RectF{ -100, 0, 100, (double)sceneSize.y }
		<< RectF{ (double)sceneSize.x, 0, 100, (double)sceneSize.y };

		player.update(dyn);
		player.advanceAnim();
		// === 折れた芯の落下更新 ===
		brokenLead.update(Scene::DeltaTime());


		// ---- ボタン押下（交差開始 or 着地）＋クールダウン ----
		const RectF playerAABB{ player.pos, player.size };   // ← ここで1回だけ定義
		const bool  overlapNow = playerAABB.intersects(button.stretched(1));
		const bool  enteredNow = (overlapNow && !buttonPrevOverlap);
		const bool  landedBtn = landedFromJumpOn(button);   // ボタンは“着地”でもOK
		if ((enteredNow || landedBtn)
			&& (buttonCD.isRunning() == false || buttonCD.sF() >= buttonCooldown))
		{
			if (pencil.extendOnce()) {
				AudioAsset(U"PushSE").play();
			}
			buttonCD.restart();
		}
		buttonPrevOverlap = overlapNow;

		// ---- 最新コライダ（延長後に更新）----
		const RectF colBody = pencil.colliderBody();
		const RectF colLead = pencil.colliderLead();

		// === 折れる判定 ===
		const double screenMidX = Scene::CenterF().x;  // 画面右半分しきい
		const double prevCenterX = player.prevPos.x + player.size.x * 0.5;
		const double nowCenterX = player.pos.x + player.size.x * 0.5;

		// 足元（接地/立っている判定用）← ここで1回だけ定義して以降も再利用
		const RectF feet{ player.pos.x, player.pos.y + player.size.y - 2, player.size.x, 4 };

		// 条件1: 芯に“ジャンプから着地” → 折れる（根元の安全帯は除外）
		if (landedFromJumpOn(colLead)) {
			const double safeX = colLead.x + leadRootSafeLen;
			if (nowCenterX >= safeX) {
				breakLead();
				goto AFTER_BREAK_CHECKS;
			}
		}

		// 条件3: プッシュ6回以上 & 画面右半分に到達 & grounded & 芯に“立っている” → 折れる
		if (pencil.presses >= 6) {
			const bool onLeadNowFeet = feet.intersects(colLead.stretched(0, 1));
			const bool inRightHalfOfScreen = (nowCenterX >= screenMidX);
			if (onLeadNowFeet && player.grounded && inRightHalfOfScreen) {
				breakLead();
				goto AFTER_BREAK_CHECKS;
			}
		}

	AFTER_BREAK_CHECKS:;

		// 衝撃演出（本体or芯上）
		if (colBody.intersects(feet) || pencil.colliderLead().intersects(feet)) {
			pencil.applyImpact(player.vel.y);
		}

		// 落下でリスポーン＆芯リセット
		if (player.pos.y > sceneSize.y + 40) {
			player.pos = startPos;
			player.prevPos = startPos;
			player.vel = Vec2{ 0, 0 };
			pencil.reset();
			buttonPrevOverlap = false;
			wasOnLead = false;
		}

		// クリア
		if (RectF{ player.pos, player.size }.intersects(door)) {
			onClear();
		}

		if (KeyEscape.down()) {
			StopAllAudio();
			changeScene(State::Title, 0.2s);
		}
	}


	void drawBackground() const override {
		Rect{ sceneSize }.draw(ColorF{ 0.97,0.98,1.0 });
		for (int x = 0; x <= sceneSize.x; x += 40) Line{ x,0,x,sceneSize.y }.draw(1, ColorF{ 0,0,0,0.05 });
		for (int y = 0; y <= sceneSize.y; y += 40) Line{ 0,y,sceneSize.x,y }.draw(1, ColorF{ 0,0,0,0.05 });
	}

	void draw() const override {
		drawBackground();

		// ドア島
		doorPad.draw(ColorF{ 0.82,0.85,0.9 });
		doorPad.drawFrame(2, 0, ColorF{ 0.2,0.25,0.3,0.4 });

		// シャーペン（本体固定＋芯可変）
		pencil.draw();

		// 折れた芯（落下中）を描画
		brokenLead.draw();

		// ボタン（ノック上）
		RoundRect{ button, 3 }
			.draw(ColorF{ 0.90,0.92,0.96 })
			.drawFrame(2, ColorF{ 0.4,0.45,0.5,0.7 });
		Triangle{ button.center().movedBy(0,-7), button.center().movedBy(-6,3), button.center().movedBy(6,3) }
		.draw(ColorF{ 0.2,0.2,0.25,0.9 });

		// ドア（共通 60×80）
		door.draw(Palette::White);
		door.drawFrame(4, ColorF{ 0.15,0.5,0.25 });
		RectF{ door.x + 6, door.y + 6, door.w - 12, door.h - 12 }.draw(ColorF{ 0.85,1.0,0.9,0.35 });

		player.draw();
	}

	void onClear() override {
		AudioAsset(U"clearSE").play();
		getData().unlocked = Max(getData().unlocked, 4);
		TextWriter writer{ U"Assets/SaveData.txt" };
		if (writer) writer.writeln(Format(getData().unlocked));
		writer.close();
		StopAllAudio();
		changeScene(State::Stage4, 0.5s);
	}
};

//----------------------------- Stage4 -----------------------------
class Stage4 : public StageBase {
public:
	using StageBase::StageBase;

	Stage4(const InitData& init)
		: StageBase(init)
	{
		// ← これを追加
		platforms = { RectF{ 0, 580, 960, 60 } };                 // 地面
		colliders = MakeLevelColliders(sceneSize, platforms);     // 画面端の壁も追加
		player = Player{};                                        // プレイヤー初期化
		player.pos = startPos;                                    // 開始位置に置く
	}

private:
	//============== ルール状態 ==============
	enum class Light { Red, Green };

	struct Car {
		RectF  body{ 0, 0, 64, 28 };
		double speed = 520.0;   // 速さ
		int    dir = +1;      // +1: 左→右,  -1: 右→左
		bool   active = false;

		void spawnFromLeft(double y) { body.pos = { -120, y };              dir = +1; active = true; }
		void spawnFromRight(double y) { body.pos = { Scene::Width() + 120, y }; dir = -1; active = true; }

		void update(double dt) {
			if (!active) return;
			body.x += (dir * speed * dt);

			// 画面外判定（right()/bottom()は使わない）
			if (dir == -1) {
				// 右→左：右端 (x+w) が左に抜けたら消す
				if ((body.x + body.w) < -100) active = false;
			}
			else {
				// 左→右：左端 x が画面右を越えたら消す
				if (body.x > (Scene::Width() + 100)) active = false;
			}
		}

		void draw() const {
			if (!active) return;
			const RectF r = body;
			// 車ボディ
			r.stretched(1).rounded(6).draw(ColorF(0.15));
			RectF(r.x + 6, r.y + 4, r.w - 12, r.h - 10).rounded(6).draw(ColorF(0.9));
			// タイヤ（y+h を使用）
			Circle(r.x + r.w * 0.25, r.y + r.h, 6).draw(ColorF(0.1));
			Circle(r.x + r.w * 0.75, r.y + r.h, 6).draw(ColorF(0.1));
			// ヘッドライト
			const Vec2 hl = (dir == +1) ? Vec2(r.x + r.w - 6, r.y + 10) : Vec2(r.x + 6, r.y + 10);
			Triangle(hl, 10, (dir == +1 ? -90_deg : +90_deg)).draw(ColorF(1.0, 0.9));
		}
	};

	//============== 配置 ==============
	const Vec2  startPos{ 120, 540 };
	const RectF crosswalk{ 360, 560, 240, 24 }; // 横断歩道
	const RectF sensor{ 168, 520, 28, 44 };     // センサー（一定時間で青）
	const RectF goalDoor{ 820, 500, 60, 80 };   // クリア扉
	const double roadY = crosswalk.y - 8;       // 車レーンのY

	//============== 信号 / タイミング ==============
	Light  light = Light::Red;
	double senseHold = 0.0;                     // センサー滞在時間蓄積
	static constexpr double kHoldToGreen = 2.0; // 青にするのに必要な滞在秒
	static constexpr double kGreenWindow = 6.0; // 青の持続秒
	double greenRemain = 0.0;
	bool   goalAppeared = false;

	//============== 車制御 ==============
	Car    car;
	bool   carQueued = false;
	double carSpawnDelay = 0.25;
	double carSpawnT = 0.0;

	//============== ヘルパ（仮想関数にせずローカルで完結） ==============
	RectF playerRect() { return RectF{ player.pos, player.size }; }
	void  warpPlayerToStart() { player.pos = startPos; player.vel = Vec2{ 0, 0 }; }

	void resetAfterHit() {
		car.active = false;
		carQueued = false;
		carSpawnT = 0.0;
		warpPlayerToStart();
	}

	// --- ノックバック（轢かれた演出） ---
	bool  knocked = false;
	Vec2  knockVel{ 0, 0 };
	static constexpr double gravityY = 1600.0;
	static constexpr double groundY = 560.0;  // 着地ライン（道路の上）

public:
	void update() override {
		StageBase::update();
		const double dt = Scene::DeltaTime();
		const RectF prect = playerRect();

		// --- センサー判定：滞在で青化 ---
		if (prect.intersects(sensor)) {
			senseHold = Min(senseHold + dt, kHoldToGreen);
			if (senseHold >= kHoldToGreen && light == Light::Red) {
				light = Light::Green;
				greenRemain = kGreenWindow;
				carQueued = false; // 違反キューを消す
				// AudioAsset(U"switch").play();
			}
		}
		else {
			// 少しだけ減衰（完全に0には戻りづらい感覚）
			senseHold = Max(0.0, senseHold - dt * 0.7);
		}

		// --- 青の残り時間 ---
		if (light == Light::Green) {
			greenRemain -= dt;
			if (greenRemain <= 0.0) {
				light = Light::Red;
				senseHold = 0.0;
			}
		}

		// --- 赤で横断 → 車出現キュー ---
		if (light == Light::Red && prect.intersects(crosswalk) && (!car.active) && (!carQueued)) {
			carQueued = true;
			carSpawnT = carSpawnDelay;
		}

		// --- 車スポーン ---
		if (carQueued) {
			carSpawnT -= dt;
			if (carSpawnT <= 0.0) {
				const double cwCenterX = crosswalk.x + crosswalk.w * 0.5;
				const bool spawnFromRight = (prect.center().x > cwCenterX);
				if (spawnFromRight) car.spawnFromRight(roadY);
				else                car.spawnFromLeft(roadY);
				carQueued = false;
			}
		}

		car.update(dt);

		// --- 衝突
		if (!knocked && car.active && prect.intersects(car.body)) {
			// ノックバック開始：車の進行方向に大きく、上にも跳ねる
			knocked = true;
			knockVel = Vec2(car.dir * 320.0, -420.0);
			// ここでは即リセットしない（車が画面外に抜けるまで待つ）
		}

		// --- クリア条件：青のあいだに横断し右側へ抜ける ---
		{
			const double cwRight = crosswalk.x + crosswalk.w; // right() 不使用
			if (!goalAppeared && light == Light::Green) {
				if (prect.x > (cwRight + 6.0)) {
					goalAppeared = true;
				}
			}
		}

		// --- 扉入場で次シーンへ ---
		if (goalAppeared && prect.intersects(goalDoor)) {
			// getData().unlocked = Max(getData().unlocked, 5);
			changeScene(State::StageLast, 0.3s);
		}

		// === ノックバック挙動 ===
		if (knocked) {
			// 簡易物理（重力のみ・壁当たりは無し）
			knockVel.y += gravityY * dt;
			player.pos += knockVel * dt;

			// 地面で止める（道路ラインより下に行かない）
			if (player.pos.y + player.size.y > groundY) {
				player.pos.y = groundY - player.size.y;
				knockVel.y = 0.0;
			}

			// 車が画面外に抜けたら（= active が false になったら）リセット
			if (!car.active) {
				knocked = false;
				resetAfterHit();  // ここでワープ＆各種フラグ初期化
			}
		}
	}

	void draw() const override {
		// 道路
		RectF(0, 580, Scene::Width(), 60).draw(ColorF(0.12));

		// ===== 横断歩道（くっきり白線） =====
		{
			const double stripeW = 22.0;           // 白線の幅
			const double stripeGap = 14.0;           // 白と白の間隔
			const int    count = (int)((crosswalk.w + stripeGap) / (stripeW + stripeGap));

			// 枠（うっすら）
			RectF(crosswalk.x, crosswalk.y, crosswalk.w, crosswalk.h)
				.draw(ColorF(0.92))                  // 白地
				.drawFrame(2.0, 0.0, ColorF(0, 0, 0, 0.18));

			// 白線を等間隔で敷く（乱数なし）
			double x = crosswalk.x + stripeGap * 0.5;
			for (int i = 0; i < count; ++i) {
				const double w = Min(stripeW, crosswalk.x + crosswalk.w - x);
				if (w <= 0) break;
				RectF(x, crosswalk.y, w, crosswalk.h).draw(Palette::White);
				x += (stripeW + stripeGap);
			}
		}

			// ===== 上部 HUD ゲージ（大きめ） =====
		{
			// バー位置とサイズ
			const Vec2 base = Vec2{ Scene::CenterF().x - 180, 24 };
			const double W = 360, H = 16;

			// 背景と枠
			RectF(base.x, base.y, W, H).draw(ColorF(0.95, 0.97, 1.0, 0.85));
			RectF(base.x, base.y, W, H).drawFrame(2, 0, ColorF(0.25, 0.3, 0.35, 0.7));

			if (light == Light::Red) {
				// センサー滞在の充電ゲージ
				const double p = (kHoldToGreen > 0 ? (senseHold / kHoldToGreen) : 1.0);
				RectF(base.x, base.y, W * Saturate(p), H).draw(ColorF(0.35, 0.85, 0.75, 0.9));
				FontAsset(U"ui")(U"センサー充電中").drawAt(base.movedBy(W * 0.5, -14), ColorF(0.25));
			}
			else {
				// 青信号の残り時間ゲージ（右から減る）
				const double p = Saturate(greenRemain / kGreenWindow);
				RectF(base.x, base.y, W * p, H).draw(ColorF(0.35, 1.0, 0.45, 0.9));
				FontAsset(U"ui")(Format(U"青信号 {:.1f}s", greenRemain))
					.drawAt(base.movedBy(W * 0.5, -14), ColorF(0.25));
			}
		}

		// 信号
		{
			const double baseX = crosswalk.x + crosswalk.w * 0.5;
			const double baseY = crosswalk.y - 70.0;
			RectF(baseX - 10.0, baseY, 20.0, 100.0).draw(ColorF(0.2));
			const Circle red(baseX, baseY + 10.0, 10.0);
			const Circle green(baseX, baseY + 40.0, 10.0);
			red.draw((light == Light::Red) ? ColorF(1.0, 0.25, 0.25) : ColorF(0.3, 0.1, 0.1));
			green.draw((light == Light::Green) ? ColorF(0.35, 1.0, 0.45) : ColorF(0.1, 0.25, 0.1));
			if (light == Light::Green) {
				FontAsset(U"ui")(Format(U"{:.1f}", greenRemain))
					.drawAt(baseX, baseY + 66.0, ColorF(0.9));
			}
		}

		// 車
		car.draw();

		// 扉
		if (goalAppeared) {
			// 白い本体
			goalDoor.draw(Palette::White);
			// 緑フレーム
			goalDoor.drawFrame(4, ColorF{ 0.15,0.5,0.25 });
			// うっすら内側
			RectF{ goalDoor.x + 6, goalDoor.y + 6, goalDoor.w - 12, goalDoor.h - 12 }
			.draw(ColorF{ 0.85,1.0,0.9,0.35 });
		}


		player.draw();

	}
	void onClear() override {}
};

//----------------------------- StageLast -----------------------------
class StageLast : public StageBase {
public:
	using StageBase::StageBase;

private:
	// 家具当たり/描画用
	RectF chairArea{ 860, 540, 60, 40 };   // イス座面
	RectF deskArea{ 820, 520, 120, 20 };  // デスク天板
	RectF pcRect{ 880, 470,  40, 28 };  // モニタ
	RectF towerRect{ 830, 540,  20, 40 };  // PC本体
	RectF decoStep{ 720, 560,  80, 20 };

	bool  sitting = false;
	bool  clicked = false;
	bool  blackedOut = false;
	Stopwatch sitSW{ StartImmediately::No };
	Stopwatch blackoutSW{ StartImmediately::No };

	const double blackoutDelay = 2.1;   // 暗転開始（即時黒）
	const double holdBlack = 0.20;  // 黒を見せる時間
	const double clickLead = 0.25;  // シーン遷移までの時間


public:
	StageLast(const InitData& init) : StageBase(init) {
		platforms = { RectF{ 0, 580, 960, 60 } };
		colliders = MakeLevelColliders(sceneSize, platforms);

		player = Player{};
		player.pos = Vec2{ 40, 540 };
		goal = RectF{};

		if (!AudioAsset::IsRegistered(U"clickSE"))
			AudioAsset::Register(U"clickSE", U"Assets/clickSE.mp3");
	}

	void drawBackground() const override {
		RectF{ 0,0,(double)sceneSize.x,(double)sceneSize.y }
		.draw(Arg::top = ColorF{ 0.94,0.95,0.98 }, Arg::bottom = ColorF{ 0.90,0.92,0.96 });
		RectF{ 0, 560, (double)sceneSize.x, 80 }.draw(ColorF{ 0.80,0.83,0.86 });
		Line{ 0,560, sceneSize.x,560 }.draw(2, ColorF{ 0.5,0.55,0.6,0.35 });

		{
			const RectF bedBase{ 100, 520, 220, 40 };
			bedBase.draw(ColorF{ 0.85,0.86,0.90 });
			bedBase.drawFrame(3, ColorF{ 0.55,0.58,0.62,0.6 });
			RectF{ bedBase.x + 6, bedBase.y - 28, 140, 28 }.draw(ColorF{ 0.94,0.96,1.0 });
			RectF{ bedBase.x + 6, bedBase.y - 28, 140, 28 }.drawFrame(2, ColorF{ 0.6,0.65,0.7,0.5 });
			RectF{ bedBase.x + 150, bedBase.y - 16, 60, 16 }.draw(ColorF{ 0.95,0.95,0.98 });
		}

		{
			const RectF window{ 360, 180, 180, 120 };
			window.draw(ColorF{ 0.15,0.18,0.30 });
			for (int i = 0; i < 18; ++i) {
				Circle{ window.pos.movedBy(Random(10.0, window.w - 10.0),
										   Random(10.0, window.h - 10.0)),
						Random(1.2, 2.2) }
				.draw(ColorF{ 1.0,1.0,0.9, Random(0.3,0.6) });
			}
			window.drawFrame(4, ColorF{ 0.6,0.65,0.7 });
			Line{ window.x, window.centerY(), window.x + window.w, window.centerY() }.draw(2, ColorF{ 0.6,0.65,0.7,0.7 });
			Line{ window.centerX(), window.y, window.centerX(), window.y + window.h }.draw(2, ColorF{ 0.6,0.65,0.7,0.7 });
		}

		{
			const RectF seat = chairArea;
			seat.draw(ColorF{ 0.78,0.80,0.84 });
			seat.drawFrame(2, ColorF{ 0.5,0.55,0.6,0.7 });
			RectF{ seat.x + 6, seat.y - 24, 10, 24 }.draw(ColorF{ 0.6,0.65,0.7 }); // 背
			RectF{ seat.x + seat.w - 16, seat.y - 24, 10, 24 }.draw(ColorF{ 0.6,0.65,0.7 });
			Line{ seat.center().x, seat.y + seat.h, seat.center().x, seat.y + seat.h + 18 }.draw(3, ColorF{ 0.55,0.6,0.66 });
			Line{ seat.center().x, seat.y + seat.h + 18, seat.center().x - 12, seat.y + seat.h + 28 }.draw(3, ColorF{ 0.55,0.6,0.66 });
			Line{ seat.center().x, seat.y + seat.h + 18, seat.center().x + 12, seat.y + seat.h + 28 }.draw(3, ColorF{ 0.55,0.6,0.66 });
		}


		{
			deskArea.draw(ColorF{ 0.82,0.84,0.88 });
			deskArea.drawFrame(2, ColorF{ 0.5,0.55,0.6,0.6 });
			RectF{ deskArea.x + 6, deskArea.y + deskArea.h, 8, 40 }.draw(ColorF{ 0.6,0.65,0.7 });
			RectF{ deskArea.x + deskArea.w - 14, deskArea.y + deskArea.h, 8, 40 }.draw(ColorF{ 0.6,0.65,0.7 });

			pcRect.draw(ColorF{ 0.10,0.11,0.12 });
			RectF{ pcRect.x + 4, pcRect.y + 4, pcRect.w - 8, pcRect.h - 8 }
			.draw(Arg::top = ColorF{ 0.9,0.95,1.0,0.9 }, Arg::bottom = ColorF{ 0.75,0.85,1.0,0.9 });
			pcRect.drawFrame(2, ColorF{ 0.6,0.65,0.7,0.8 });
			Ellipse{ pcRect.center().movedBy(0,8), 60, 18 }.draw(ColorF{ 0.8,0.9,1.0,0.08 });

			towerRect.draw(ColorF{ 0.22,0.24,0.28 });
			towerRect.drawFrame(2, ColorF{ 0.55,0.6,0.66,0.5 });
			RectF{ towerRect.x + 6, towerRect.y + 8, towerRect.w - 12, 6 }.draw(ColorF{ 0.85,0.2,0.2 });
			Circle{ towerRect.x + 14, towerRect.y + towerRect.h - 10, 3 }.draw(ColorF{ 0.2,0.9,0.3 });

			{
				Line{ 640, 560, 640, 500 }.draw(3, ColorF{ 0.6,0.65,0.7 });
				Triangle{ Vec2{640,500}, Vec2{660,490}, Vec2{620,490} }.draw(ColorF{ 0.9,0.9,0.95 });
				Ellipse{ 640, 570, 26, 8 }.draw(ColorF{ 0,0,0,0.08 });
			}

		}

		decoStep.draw(ColorF{ 0.76,0.79,0.82 });
		decoStep.drawFrame(2, ColorF{ 0.5,0.55,0.6,0.5 });
	}

	void update() override {
		const double dt = Scene::DeltaTime();

		if (!sitting) {
			const bool left = (KeyA.pressed() || KeyLeft.pressed());
			const bool right = (KeyD.pressed() || KeyRight.pressed());

			double ax = 0.0;
			if (left ^ right) {
				const double a = player.grounded ? player.moveAccel * 0.8 : player.airAccel * 0.8;
				ax = (left ? -a : a);
			}
			const double maxX = player.maxSpeedX * 0.7;

			player.prevPos = player.pos;

			player.vel.x += ax * dt;
			player.vel.x -= player.vel.x * Min((player.grounded ? player.groundFric : player.airFric) * dt, 1.0);
			player.vel.x = Clamp(player.vel.x, -maxX, maxX);
			// 縦は重力（着地維持・ジャンプ禁止）
			player.vel.y += player.gravity * dt;

			player.pos.x += player.vel.x * dt;
			{
				RectF aabbX{ player.pos, player.size };
				for (const auto& c : colliders) {
					if (!aabbX.intersects(c)) continue;
					if (player.vel.x > 0) player.pos.x = c.x - player.size.x - 0.01;
					else if (player.vel.x < 0) player.pos.x = c.x + c.w + 0.01;
					player.vel.x = 0; aabbX.setPos(player.pos);
				}
			}
			player.pos.y += player.vel.y * dt;
			{
				RectF aabbY{ player.pos, player.size };
				player.grounded = false;
				for (const auto& c : colliders) {
					if (!aabbY.intersects(c)) continue;
					if (player.vel.y > 0) { player.pos.y = c.y - player.size.y - 0.01; player.vel.y = 0; player.grounded = true; }
					else if (player.vel.y < 0) { player.pos.y = c.y + c.h + 0.01; player.vel.y = 0; }
					aabbY.setPos(player.pos);
				}
			}
			player.jumpedThisFrame = false;
			player.advanceAnim();

			const RectF playerAABB{ player.pos, player.size };
			if (playerAABB.intersects(chairArea)) {
				sitting = true;
				player.vel = Vec2{ 0,0 };
				player.pos = Vec2{ chairArea.x + 14, chairArea.y - player.size.y + 12 };

				sitSW.restart();
			}
		}
		else {
			const double t = sitSW.sF();

			const double clickAt = Max(0.0, blackoutDelay - clickLead);
			if (!clicked && t >= clickAt) {
				clicked = true;
				if (AudioAsset::IsRegistered(U"clickSE")) AudioAsset(U"clickSE").play();
			}
			if (!blackedOut && t >= blackoutDelay) {
				blackedOut = true;
				blackoutSW.restart();
			}
			if (blackedOut && blackoutSW.sF() >= holdBlack) {
				getData().unlocked = Max(getData().unlocked, 13);
				TextWriter writer{ U"Assets/SaveData.txt" };
				if (writer)
				{
					writer.writeln(Format(getData().unlocked));
				}
				writer.close();
				StopAllAudio();
				changeScene(State::EndRoll, 0.0s);
				return;
			}
		}
	};

	void draw() const override {
		drawBackground();
		drawLevel();

		if (!sitting) {
			player.draw();
		}
		else {
			// 座る自然ポーズ
			const Vec2 center = player.pos + Vec2{ player.size } / 2;

			Ellipse{ center.movedBy(8, player.size.y / 2 + 12), 20, 7 }.draw(ColorF{ 0,0,0,0.12 });
			const RoundRect body{ RectF{ Arg::center = center.movedBy(12, -4), 26, 30 }, 6.0 };
			{
				const Transformer2D tilt{ Mat3x2::Rotate(0.10, body.rect.center()) };
				body.draw(ColorF{ 0.12,0.12,0.14 });
			}
			const Circle head{ center.movedBy(18, -player.size.y * 0.9 + 8), 12 };
			head.draw(ColorF{ 0.95 });
			Circle{ head.center.movedBy(3, -2), 1.6 }.draw(ColorF{ 0.08 });
			Line{ body.rect.center().movedBy(2,-6), body.rect.center().movedBy(18,-10) }.draw(4, ColorF{ 0.15 });
			Line{ body.rect.center().movedBy(2,-0), body.rect.center().movedBy(18,-2) }.draw(4, ColorF{ 0.15 });
			Line{ body.rect.bottomCenter().movedBy(-4,-2), body.rect.bottomCenter().movedBy(-16,10) }.draw(5, ColorF{ 0.12 });
			Line{ body.rect.bottomCenter().movedBy(8,-2), body.rect.bottomCenter().movedBy(20,10) }.draw(5, ColorF{ 0.12 });
			Circle{ body.rect.bottomCenter().movedBy(-18, 12), 4 }.draw(ColorF{ 0.2 });
			Circle{ body.rect.bottomCenter().movedBy(22, 12), 4 }.draw(ColorF{ 0.2 });
			RectF seatLip{ chairArea.x, chairArea.y + chairArea.h - 5, chairArea.w, 5 };
			seatLip.draw(ColorF{ 0.74,0.76,0.80 });
			seatLip.drawFrame(1.5, ColorF{ 0.5,0.55,0.6,0.7 });
		}

		// 最前面レイヤ-（小物：当たり判定なし）
		{
			// ゴミ箱
			const RectF bin{ 300, 576, 28, 24 };
			RoundRect{ bin, 4 }.draw(ColorF{ 0.78,0.80,0.84 });
			RoundRect{ bin, 4 }.drawFrame(2, ColorF{ 0.5,0.55,0.6,0.7 });
			Line{ bin.x + 6, bin.y + 6, bin.x + bin.w - 6, bin.y + 6 }.draw(2, ColorF{ 0.6,0.65,0.7,0.7 });

			// 観葉植物
			const RectF pot{ 520, 586, 26, 16 };
			pot.draw(ColorF{ 0.7,0.5,0.4 });
			for (int i = 0; i < 4; ++i) {
				Bezier2{ pot.center().movedBy(0,-2),
						 pot.center().movedBy(-18 + 12 * i,-22),
						 pot.center().movedBy(-10 + 10 * i,-36) }.draw(4, ColorF{ 0.25,0.5,0.3,0.9 });
			}

			// ボックス
			RectF{ 220, 588, 36, 14 }.draw(ColorF{ 0.82,0.84,0.88 });
			RectF{ 220, 588, 36, 14 }.drawFrame(2, ColorF{ 0.5,0.55,0.6,0.6 });
		}

		// 着席後の暗転（即時）
		if (sitting && blackedOut) {
			RectF(Scene::Rect()).draw(ColorF{ 0,0,0 });
		}

		uiCommon(U"ラストステージ");
	}

	void onClear() override {}
};


//----------------------------- EndRoll -----------------------------
class EndRoll : public App::Scene {
public:
	using App::Scene::Scene;

private:
	// 表示するスライド（\n で複数行）
	Array<String> slides{
		U"　",
		U"シン・ランド",
		U"設計・システム・デザイン：haito",
		U"サウンド：効果音ラボ",
		U"スペシャルサンクス：プレイヤーの皆様",
		U"END"
	};

	// 表示制御
	int index = 0;                 // 現在のスライド
	Stopwatch sw{ StartImmediately::Yes };
	const double fadeSec = 0.8;    // フェードイン/アウト時間
	const double holdSec = 2.0;    // 文字がくっきり見える時間

	// 早送り（クリック/スペース）で次へ
	void nextSlide() {
		++index;
		if (index >= (int)slides.size()) {
			StopAllAudio();
			changeScene(State::Title, 2.5s); // 完走→タイトルへ
		}
		else {
			sw.restart();
		}
	}

public:
	EndRoll(const InitData& init) : IScene(init) {
		// BGMや環境音が鳴っていたら止めたい場合は任意で：
		// if (AudioAsset::IsRegistered(U"stage1BGM")) AudioAsset(U"stage1BGM").stop();
		Scene::SetBackground(ColorF{ 0,0,0 });
	}

	void update() override {
		// Esc で即タイトルへ
		if (KeyEscape.down()) {
			StopAllAudio();
			changeScene(State::Title, 0.3s);
			return;
		}
		// クリック / スペースで早送り
		if (MouseL.down() || KeySpace.down()) {
			nextSlide();
			return;
		}
		// 指定時間経過で自動的に次へ
		const double t = sw.sF();
		if (t >= (fadeSec + holdSec + fadeSec)) {
			nextSlide();
		}
	}

	void draw() const override {
		// 黒背景
		Rect(Scene::Rect()).draw(ColorF{ 0,0,0 });

		if (index >= (int)slides.size()) return;

		// 中央に白文字（フェード）
		const double t = sw.sF();
		double alpha = 1.0;
		if (t < fadeSec) {
			alpha = t / fadeSec; // フェードイン
		}
		else if (t > fadeSec + holdSec) {
			alpha = 1.0 - ((t - (fadeSec + holdSec)) / fadeSec); // フェードアウト
		}
		alpha = Saturate(alpha);

		// 大きめフォントで中央表示（必要ならサイズ調整）
		static const Font fBig{ 36 };
		const Vec2 center = Scene::CenterF();
		fBig(slides[index]).drawAt(center, ColorF{ 1,1,1, alpha });
	}
};

//============================= Main =============================
void Main() {
	System::SetTerminationTriggers(UserAction::CloseButtonClicked);
	Window::Resize(960, 640);
	Window::SetTitle(U"Sin Land");

	int unlockedValue = 1;

	// --- セーブデータ読込 ---
	TextReader reader{ U"Assets/SaveData.txt" };

	if (reader)
	{
		String line;
		if (reader.readLine(line))
		{
			line = line.trimmed();

			const bool isAllDigit = std::all_of(line.begin(), line.end(), [](const char32 ch)
				{
					return IsDigit(ch);
				});

			if (isAllDigit)
			{
				const int v = Parse<int>(line);
				if (InRange(v, 1, 13))
				{
					unlockedValue = v;
				}
			}
		}
	}
	reader.close();

	// 例：初期化時
	//FontAsset::Register(U"ui", 18, Typeface::Regular);
	//FontAsset::Register(U"ui_s", 14, Typeface::Regular);

	App manager;
	manager.add<Title>(State::Title);
	manager.add<Select>(State::Select);
	manager.add<Stage1>(State::Stage1);
	manager.add<Stage2>(State::Stage2);
	manager.add<Stage3>(State::Stage3);
	manager.add<Stage4>(State::Stage4);
	//manager.add<Stage5>(State::Stage5);
	manager.add<StageLast>(State::StageLast);
	manager.add<EndRoll>(State::EndRoll);

	manager.init(State::Title);

	manager.get()->unlocked = unlockedValue;

	while (System::Update()) {
		manager.update();
	}
}
