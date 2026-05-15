#ifndef ENEMYFISH_H
#define ENEMYFISH_H

#include "fish.h"

#include <QColor>
#include <QPointF>
#include <QRandomGenerator>

// 敌方鱼：等级 0–5；默认场内反弹游走；HorizontalCross 为水平横穿（离场后移除）
class EnemyFish : public Fish {
public:
    enum class MovementMode {
        BounceWander,
        HorizontalCross,
    };

    EnemyFish(qreal x, qreal y, qreal radius, const QPointF &velocity, const QColor &color, int enemyTier,
              MovementMode mode = MovementMode::BounceWander);

    void updateFish(qreal dtSec, const QRectF &bounds, const QPointF &playerPos, int playerTier) override;

    QPointF velocity() const { return vel_; }
    MovementMode movementMode() const { return mode_; }
    qreal speedMultiplier() const;

    bool isOutsideAfterCross(const QRectF &bounds) const;

private:
    void applyTierSteering(qreal dtSec, const QPointF &playerPos, int playerTier);
    void clampVelocity();

    QPointF vel_;
    MovementMode mode_;
};

#endif
