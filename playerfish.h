#ifndef PLAYERFISH_H
#define PLAYERFISH_H

#include "fish.h"

#include <QPointF>

// 玩家鱼：等级默认 1，胜利目标为 6；平滑移动；boost 提高极速与加速度
class PlayerFish : public Fish {
public:
    PlayerFish(qreal x, qreal y, qreal radius);

    void setMoveDir(const QPointF &dir) { moveDir_ = dir; }
    void setBoostActive(bool on) { boostActive_ = on; }

    void updateFish(qreal dtSec, const QRectF &bounds, const QPointF &playerPos, int playerTier) override;

private:
    QPointF moveDir_;
    QPointF vel_;
    qreal accel_;
    qreal maxSpeed_;
    bool boostActive_ = false;
};

#endif
