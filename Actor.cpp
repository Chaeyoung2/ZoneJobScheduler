#include "Actor.h"

Actor::Actor() = default;

Actor::Actor(int initialMaxHp, int initialHp)
    : m_maxHp(initialMaxHp),
      m_hp(initialHp)
{
}

void Actor::takeDamage(unsigned int damage)
{
    if (damage > m_hp)
    {
        m_hp = 0;
        return;
    }

    m_hp -= damage;
}

int Actor::getHp()
{
    return m_hp;
}
