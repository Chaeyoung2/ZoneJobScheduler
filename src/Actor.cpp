#include "Actor.h"

#include <cassert>

Actor::Actor() = default;

Actor::Actor(int initialMaxHp, int initialHp)
    : m_maxHp(initialMaxHp),
      m_hp(initialHp)
{
	assert(initialMaxHp >= 0);
	assert(initialHp >= 0);
	assert(initialHp <= initialMaxHp);
}

void Actor::takeDamage(int damage)
{
	assert(damage >= 0);

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
