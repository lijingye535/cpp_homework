#include "playerfish.h"

#include <QtMath>

namespace {
constexpr qreal kBoostSpeedMul = 1.72;
constexpr qreal kBoostAccelMul = 1.45;
constexpr qreal kMaxTiltRad = 0.524;
}

PlayerFish::PlayerFish(qreal x, qreal y, qreal radius)
    : Fish(x, y, radius, QColor(72, 188, 255), 1)
    , moveDir_(0, 0)
    , vel_(0, 0)
    , accel_(520.0)
    , maxSpeed_(260.0)
{
}

void PlayerFish::updateFish(qreal dtSec, const QRectF &bounds, const QPointF &playerPos, int playerTier)
{
    Q_UNUSED(playerPos);
    Q_UNUSED(playerTier);

    const qreal speedMul = boostActive_ ? kBoostSpeedMul : 1.0;
    const qreal accelMul = boostActive_ ? kBoostAccelMul : 1.0;

    QPointF targetVel(0, 0);
    if (moveDir_.manhattanLength() > 1e-4) {
        QPointF d = moveDir_;
        const qreal len = qSqrt(d.x() * d.x() + d.y() * d.y());
        if (len > 1e-6)
            d /= len;
        const qreal cap = (maxSpeed_ + radius_ * 0.55) * speedMul;
        targetVel = d * cap;
    }

    QPointF dv = targetVel - vel_;
    const qreal maxStep = accel_ * accelMul * dtSec;
    const qreal dvLen = qSqrt(dv.x() * dv.x() + dv.y() * dv.y());
    if (dvLen > maxStep && dvLen > 1e-6)
        dv *= maxStep / dvLen;
    vel_ += dv;

    pos_ += vel_ * dtSec;

    pos_.setX(qBound(bounds.left() + radius_, pos_.x(), bounds.right() - radius_));
    pos_.setY(qBound(bounds.top() + radius_, pos_.y(), bounds.bottom() - radius_));

    const qreal speed = qSqrt(vel_.x() * vel_.x() + vel_.y() * vel_.y());
    if (speed > 12.0) {
        qreal targetHeading = qAtan2(vel_.y(), qAbs(vel_.x()) + 1e-6);
        if (targetHeading > kMaxTiltRad) {
            targetHeading = kMaxTiltRad;
        } else if (targetHeading < -kMaxTiltRad) {
            targetHeading = -kMaxTiltRad;
        }
        headingRad_ = targetHeading;
    }
}
