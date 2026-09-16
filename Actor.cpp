#include "Actor.h"

Actor::Actor() = default;

Actor::Actor(int initialMaxHp, int initialHp)
    : maxHp(initialMaxHp),
      hp(initialHp)
{
}

void Actor::takeDamage(unsigned int damage)
{
    if (damage > hp)
    {
        hp = 0;
        return;
    }

    hp -= damage;
}

int Actor::getHp()
{
    return hp;
}
