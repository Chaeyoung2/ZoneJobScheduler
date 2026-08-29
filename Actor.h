#pragma once

enum class ActorType { ACTOR_NONE, ACTOR_PLAYER, ACTOR_MONSTER };

struct Vec2 { int x, y; Vec2(int xx, int yy) : x(xx), y(yy) {} };

// Base ------------------------------
class Actor
{
public:
	Actor(int id, ActorType actorType, int initHp, int x, int y)
		: actorID(id), type(actorType), hp(initHp), pos(x, y) {}

	virtual ~Actor() = default;

	virtual void update() = 0;

	virtual void move(int dx, int dy) = 0;
	virtual void attack(Actor* trg) = 0;

public:
	void takeDamage(int dmg) { hp -= dmg; }

protected:
	int actorID = 0;
	int hp = 0;
	ActorType type = ActorType::ACTOR_NONE;
	Vec2 pos;
};

// Derive ------------------------------
class Player : public Actor
{
public:
	Player(int id, int initHp, int x, int y)
		: Actor{ id, ActorType::ACTOR_PLAYER, initHp, x, y } {}

	void update() override {}

	void move(int dx, int dy) override { pos.x += dx; pos.y += dy; }
	void attack(Actor* trg) override { if (trg) trg->takeDamage(10); }
};

class Monster : public Actor
{
public:
	Monster(int id, int initHp, int x, int y)
		: Actor{ id, ActorType::ACTOR_MONSTER, initHp, x, y } {}

	void update() override {}

	void move(int dx, int dy) override { pos.x += dx; pos.y += dy; }
	void attack(Actor* trg) override { if (trg) trg->takeDamage(10); }
};
