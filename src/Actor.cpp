#include "Actor.h"

#include <cassert>

Actor::Actor() = default;

void Actor::takeDamage(int damage)
{
	assert(damage >= 0);

    if (damage <= 0)
    {
        return;
    }

    if (damage > m_hp)
    {
        m_hp = 0;
        return;
    }

    m_hp -= damage;
}

int Actor::getHp() const
{
    return m_hp;
}
