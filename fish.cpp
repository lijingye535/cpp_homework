#include "fish.h"

#include <QtMath>

Fish::Fish(qreal x, qreal y, qreal radius, const QColor &color, int tier)
    : pos_(x, y)
    , radius_(radius)
    , color_(color)
    , tier_(tier)
    , headingRad_(0)
{
}

void Fish::setSkinPixmap(const QPixmap &pixmap)
{
    skin_ = pixmap;
}

void Fish::clearSkinPixmap()
{
    skin_ = QPixmap();
}

QRectF Fish::boundingRect() const
{
    return QRectF(pos_.x() - radius_, pos_.y() - radius_, radius_ * 2, radius_ * 2);
}

bool Fish::collidesWith(const Fish &other) const
{
    const qreal dx = pos_.x() - other.pos_.x();
    const qreal dy = pos_.y() - other.pos_.y();
    const qreal dist = qSqrt(dx * dx + dy * dy);
    return dist < (radius_ + other.radius_);
}

void Fish::draw(QPainter &painter) const
{
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.translate(pos_);
    painter.rotate(qRadiansToDegrees(headingRad_));

    if (!skin_.isNull()) {
        painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
        const int tw = qMax(8, int(radius_ * 3.5));
        const int th = qMax(8, int(radius_ * 2.3));
        const QPixmap scaled = skin_.scaled(tw, th, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        painter.drawPixmap(QPointF(-scaled.width() / 2.0, -scaled.height() / 2.0), scaled);
        painter.restore();
        return;
    }

    const qreal r = radius_;
    const qreal penW = qBound(1.0, r * 0.07, 4.0);

    QRadialGradient bodyGrad(-r * 0.15f, -r * 0.12f, r * 1.35f);
    bodyGrad.setColorAt(0.0, color_.lighter(125));
    bodyGrad.setColorAt(0.55, color_);
    bodyGrad.setColorAt(1.0, color_.darker(115));

    painter.setPen(QPen(color_.darker(175), penW));
    painter.setBrush(QBrush(bodyGrad));
    painter.drawEllipse(QRectF(-r * 0.88, -r * 0.56, r * 1.76, r * 1.12));

    QPolygonF dorsal;
    dorsal << QPointF(-r * 0.15, -r * 0.58) << QPointF(r * 0.35, -r * 0.95) << QPointF(r * 0.55, -r * 0.52);
    painter.setBrush(color_.darker(108));
    painter.drawPolygon(dorsal);

    QPolygonF tail;
    tail << QPointF(-r * 1.18, 0) << QPointF(-r * 0.42, -r * 0.52) << QPointF(-r * 0.42, r * 0.52);
    painter.setBrush(color_.darker(102));
    painter.drawPolygon(tail);

    const qreal eyeR = qMax(2.5, r * 0.13);
    const QPointF eyePos(r * 0.42, -r * 0.18);
    painter.setPen(Qt::NoPen);
    painter.setBrush(Qt::white);
    painter.drawEllipse(eyePos, eyeR, eyeR);
    painter.setBrush(QColor(25, 25, 40));
    painter.drawEllipse(QPointF(eyePos.x() + eyeR * 0.15, eyePos.y()), eyeR * 0.45, eyeR * 0.45);

    painter.restore();
}
