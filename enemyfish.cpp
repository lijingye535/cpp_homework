#include "enemyfish.h"

#include <QtMath>

namespace {
constexpr qreal kSteerStrength = 45.0;
constexpr qreal kSteerDistMin = 60.0;
constexpr qreal kSteerDistMax = 180.0;
constexpr qreal kMaxSpeedCross = 158.0;
constexpr qreal kMaxSpeedWander = 248.0;
constexpr qreal kMaxTiltRad = 0.524;
}

qreal EnemyFish::speedMultiplier() const
{
    static const qreal tbl[] = {1.0, 0.88, 0.76, 0.64, 0.52, 0.40, 0.30};
    return tbl[qBound(0, tier_, 6)];
}

EnemyFish::EnemyFish(qreal x, qreal y, qreal radius, const QPointF &velocity, const QColor &color, int enemyTier,
                     MovementMode mode)
    : Fish(x, y, radius, color, enemyTier)
    , vel_(velocity)
    , mode_(mode)
{
}

void EnemyFish::clampVelocity()
{
    const qreal baseMaxSp = mode_ == MovementMode::HorizontalCross ? kMaxSpeedCross : kMaxSpeedWander;
    const qreal maxSp = baseMaxSp * speedMultiplier();
    const qreal sp = qSqrt(vel_.x() * vel_.x() + vel_.y() * vel_.y());
    if (sp > maxSp && sp > 1e-6)
        vel_ *= maxSp / sp;
}

void EnemyFish::applyTierSteering(qreal dtSec, const QPointF &playerPos, int playerTier)
{
    const QPointF toPlayer = playerPos - pos_;
    const qreal distSq = toPlayer.x() * toPlayer.x() + toPlayer.y() * toPlayer.y();
    const qreal dMin = kSteerDistMin;
    const qreal dMax = kSteerDistMax;
    if (distSq < dMin * dMin || distSq > dMax * dMax)
        return;

    const qreal dist = qSqrt(distSq);
    QPointF dir(toPlayer.x() / dist, toPlayer.y() / dist);

    const int et = tier();
    if (et < playerTier)
        vel_ -= dir * kSteerStrength * dtSec;
    else if (et > playerTier)
        vel_ += dir * kSteerStrength * dtSec;

    clampVelocity();
}

void EnemyFish::updateFish(qreal dtSec, const QRectF &bounds, const QPointF &playerPos, int playerTier)
{
    applyTierSteering(dtSec, playerPos, playerTier);

    if (mode_ == MovementMode::HorizontalCross) {
        pos_ += vel_ * dtSec;
        const qreal vlen = qSqrt(vel_.x() * vel_.x() + vel_.y() * vel_.y());
        if (vlen > 12.0) {
            qreal targetHeading = qAtan2(vel_.y(), qAbs(vel_.x()) + 1e-6);
            if (targetHeading > kMaxTiltRad) {
                targetHeading = kMaxTiltRad;
            } else if (targetHeading < -kMaxTiltRad) {
                targetHeading = -kMaxTiltRad;
            }
            headingRad_ = targetHeading;
        } else if (qAbs(vel_.x()) > 1e-2) {
            headingRad_ = vel_.x() >= 0 ? 0.0 : qDegreesToRadians(180.0);
        }
        return;
    }

    pos_ += vel_ * dtSec;

    const qreal vlen = qSqrt(vel_.x() * vel_.x() + vel_.y() * vel_.y());
    if (vlen > 8.0) {
        qreal targetHeading = qAtan2(vel_.y(), qAbs(vel_.x()) + 1e-6);
        if (targetHeading > kMaxTiltRad) {
            targetHeading = kMaxTiltRad;
        } else if (targetHeading < -kMaxTiltRad) {
            targetHeading = -kMaxTiltRad;
        }
        headingRad_ = targetHeading;
    }

    const bool shouldBounce = (QRandomGenerator::global()->bounded(100) < 20);
    if (pos_.x() - radius_ < bounds.left()) {
        pos_.setX(bounds.left() + radius_);
        if (shouldBounce) {
            vel_.setX(qAbs(vel_.x()) * 0.6);
        } else {
            vel_.setX(-qAbs(vel_.x()));
        }
    } else if (pos_.x() + radius_ > bounds.right()) {
        pos_.setX(bounds.right() - radius_);
        if (shouldBounce) {
            vel_.setX(-qAbs(vel_.x()) * 0.6);
        } else {
            vel_.setX(qAbs(vel_.x()));
        }
    }

    if (pos_.y() - radius_ < bounds.top()) {
        pos_.setY(bounds.top() + radius_);
        if (shouldBounce) {
            vel_.setY(qAbs(vel_.y()) * 0.6);
        } else {
            vel_.setY(-qAbs(vel_.y()));
        }
    } else if (pos_.y() + radius_ > bounds.bottom()) {
        pos_.setY(bounds.bottom() - radius_);
        if (shouldBounce) {
            vel_.setY(-qAbs(vel_.y()) * 0.6);
        } else {
            vel_.setY(qAbs(vel_.y()));
        }
    }
}

bool EnemyFish::isOutsideAfterCross(const QRectF &bounds) const
{
    if (mode_ != MovementMode::HorizontalCross)
        return false;
    if (vel_.x() >= 0)
        return pos_.x() - radius_ > bounds.right();
    return pos_.x() + radius_ < bounds.left();
}
