#ifndef FISH_H
#define FISH_H

#include <QColor>
#include <QPainter>
#include <QPixmap>
#include <QPointF>
#include <QRectF>

// 所有鱼的基类：位置、半径、颜色、等级（tier）、碰撞与绘制
class Fish {
public:
    Fish(qreal x, qreal y, qreal radius, const QColor &color, int tier = 0);
    virtual ~Fish() = default;

    QPointF position() const { return pos_; }
    void setPosition(const QPointF &p) { pos_ = p; }

    qreal radius() const { return radius_; }
    void setRadius(qreal r) { radius_ = r; }

    int tier() const { return tier_; }
    void setTier(int t) { tier_ = t; }

    const QColor &color() const { return color_; }

    QRectF boundingRect() const;
    bool collidesWith(const Fish &other) const;

    qreal headingRad() const { return headingRad_; }

    void setSkinPixmap(const QPixmap &pixmap);
    void clearSkinPixmap();
    bool hasSkinPixmap() const { return !skin_.isNull(); }

    virtual void draw(QPainter &painter) const;
    virtual void updateFish(qreal dtSec, const QRectF &bounds, const QPointF &playerPos, int playerTier) = 0;

protected:
    QPointF pos_;
    qreal radius_;
    QColor color_;
    int tier_ = 0;
    qreal headingRad_ = 0; // 鱼头朝向（弧度），+x 为 0，逆时针为正
    QPixmap skin_;           // 非空时优先绘制贴图（图片应朝 +x 为头）
};

#endif
