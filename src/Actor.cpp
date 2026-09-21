#include "Actor.h"

Actor::Actor() = default;

bool Actor::takeDamage(int damage)
{
    if (damage < 0)
    {
        return false;
    }

    if (damage >= m_hp)
    {
        m_hp = 0;
    }
    else
    {
        m_hp -= damage;
    }

    return true;
}

int Actor::getHp() const
{
    return m_hp;
}
