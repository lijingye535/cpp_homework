#include "gamewidget.h"

#include <QCoreApplication>
#include <QPaintEvent>
#include <QRandomGenerator>
#include <QResizeEvent>

#include <QtMath>

namespace {
constexpr qreal kPlayerStartRadius = 18.0;
constexpr int kPlayerWinTier = 6;
constexpr int kBubbleCount = 42;

int scoreForEnemyTier(int enemyTier)
{
    static const int tbl[] = {1, 2, 5, 10, 20, 30, 50};
    return tbl[qBound(0, enemyTier, 6)];
}

int scoreForPlayerTier(int tier)
{
    static const int tbl[] = {3, 8, 20, 30, 80, 120};
    return tbl[qBound(1, tier, 6) - 1];
}

qreal playerRadiusForTier(int tier)
{
    static const qreal tbl[] = {29.0, 29.0, 45.0, 45.0, 50.0, 60.0};
    return tbl[qBound(1, tier, 6) - 1];
}

constexpr qreal kBoostDrainFullSec = 1.0;
constexpr qreal kBoostRechargeFullSec = 5.0;

constexpr qreal kPhaseEarlySec = 20.0;
constexpr int kEarlyEnemyCap = 5;
constexpr qreal kPhaseStepSec = 15.0;
}

GameWidget::GameWidget(QWidget *parent)
    : QWidget(parent)
    , bgmPlayer_(nullptr)
    , audioOutput_(nullptr)
    , winSoundPlayer_(nullptr)
    , winAudioOutput_(nullptr)
    , loseSoundPlayer_(nullptr)
    , loseAudioOutput_(nullptr)
{
    setFocusPolicy(Qt::StrongFocus);
    setMinimumSize(640, 480);

    bgmPlayer_ = new QMediaPlayer(this);
    audioOutput_ = new QAudioOutput(this);
    bgmPlayer_->setAudioOutput(audioOutput_);
    bgmPlayer_->setSource(QUrl(QStringLiteral("qrc:/skins/bgm.mp3")));
    audioOutput_->setVolume(0.5);
    bgmPlayer_->play();

    winSoundPlayer_ = new QMediaPlayer(this);
    winAudioOutput_ = new QAudioOutput(this);
    winSoundPlayer_->setAudioOutput(winAudioOutput_);
    winSoundPlayer_->setSource(QUrl(QStringLiteral("qrc:/skins/bgm_he.mp3")));
    winAudioOutput_->setVolume(0.7);

    loseSoundPlayer_ = new QMediaPlayer(this);
    loseAudioOutput_ = new QAudioOutput(this);
    loseSoundPlayer_->setAudioOutput(loseAudioOutput_);
    loseSoundPlayer_->setSource(QUrl(QStringLiteral("qrc:/skins/bgm_be.mp3")));
    loseAudioOutput_->setVolume(0.7);

    connect(&timer_, &QTimer::timeout, this, [this]() { onTick(); });
    timer_.start(16);
    clock_.start();
    lastNs_ = clock_.nsecsElapsed();

    reloadSkins();
    seedBubbles();
    resetGame();
}

namespace {

QPixmap loadSkinFile(const QString &fileName)
{
    QPixmap pm;
    const QString qrcPath = QStringLiteral(":/skins/%1").arg(fileName);
    if (pm.load(qrcPath))
        return pm;
    const QString diskPath = QCoreApplication::applicationDirPath() + QStringLiteral("/skins/%1").arg(fileName);
    if (pm.load(diskPath))
        return pm;
    return {};
}

} // namespace

void GameWidget::reloadSkins()
{
    bgPixmap_ = loadSkinFile(QStringLiteral("background.png"));
    for (int i = 0; i < 6; ++i) {
        playerSkins_[i] = loadSkinFile(QStringLiteral("player%1.png").arg(i + 1));
    }
    enemySkin_ = loadSkinFile(QStringLiteral("enemy.png"));
}

void GameWidget::drawBackground(QPainter &painter) const
{
    if (bgPixmap_.isNull()) {
        QLinearGradient grad(0, 0, 0, height());
        grad.setColorAt(0, QColor(24, 110, 175));
        grad.setColorAt(0.45, QColor(18, 72, 140));
        grad.setColorAt(1, QColor(8, 28, 72));
        painter.fillRect(rect(), grad);
        return;
    }

    const QSize sz = size();
    const QPixmap scaled = bgPixmap_.scaled(sz, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    const int x = qMax(0, (scaled.width() - sz.width()) / 2);
    const int y = qMax(0, (scaled.height() - sz.height()) / 2);
    painter.drawPixmap(0, 0, sz.width(), sz.height(), scaled, x, y, sz.width(), sz.height());
}

void GameWidget::applyPlayerSkin()
{
    if (!player_)
        return;
    const int tierIndex = qBound(0, player_->tier() - 1, 5);
    if (!playerSkins_[tierIndex].isNull())
        player_->setSkinPixmap(playerSkins_[tierIndex]);
    else
        player_->clearSkinPixmap();
}

void GameWidget::applyEnemySkinToLast()
{
    if (enemies_.empty())
        return;
    EnemyFish *e = enemies_.back().get();
    if (!enemySkin_.isNull())
        e->setSkinPixmap(enemySkin_);
    else
        e->clearSkinPixmap();
}

void GameWidget::seedBubbles()
{
    bubbles_.clear();
    bubbles_.resize(kBubbleCount);
    QRandomGenerator *rng = QRandomGenerator::global();
    QRectF b = gameBounds();
    if (b.width() < 80 || b.height() < 80) {
        const qreal fw = qMax(640.0, static_cast<qreal>(width()));
        const qreal fh = qMax(480.0, static_cast<qreal>(height()));
        b = QRectF(8, 8, fw - 16, fh - 16);
    }
    for (Bubble &bubble : bubbles_) {
        bubble.radius = 3.0 + rng->generateDouble() * 6.0;
        bubble.riseSpeed = 22.0 + rng->generateDouble() * 55.0;
        bubble.wobble = rng->generateDouble() * qDegreesToRadians(360.0);
        bubble.pos = QPointF(b.left() + rng->generateDouble() * b.width(),
                             b.top() + rng->generateDouble() * b.height());
    }
}

QRectF GameWidget::gameBounds() const
{
    constexpr qreal margin = 8.0;
    return QRectF(margin, margin, width() - 2 * margin, height() - 2 * margin);
}

int GameWidget::maxConcurrentEnemies() const
{
    if (gameTimeSec_ < kPhaseEarlySec)
        return kEarlyEnemyCap;
    const int extraBands = static_cast<int>((gameTimeSec_ - kPhaseEarlySec) / kPhaseStepSec);
    return kEarlyEnemyCap + 2 * (1 + extraBands);
}

void GameWidget::resetGame()
{
    enemies_.clear();
    const QRectF b = gameBounds();
    player_ = std::make_unique<PlayerFish>(b.center().x(), b.center().y(), playerRadiusForTier(1));
    player_->setTier(1);
    player_->setRadius(playerRadiusForTier(1));
    score_ = 0;
    tierScore_ = 0;
    gameOver_ = false;
    gameWon_ = false;
    finalPhase_ = false;
    finalSpawnAccumulator_ = 0;
    paused_ = false;
    gameTimeSec_ = 0;
    boostEnergy_ = 1.0;
    winSoundPlayed_ = false;
    loseSoundPlayed_ = false;
    bgmPlayer_->stop();
    bgmPlayer_->setPosition(0);
    bgmPlayer_->play();
    keyBoost_ = false;
    spawnAccumulator_ = 0;
    spawnIntervalSec_ = 1.15;
    seedBubbles();
    applyPlayerSkin();
}

void GameWidget::spawnEnemy(int playerTier)
{
    if (static_cast<int>(enemies_.size()) >= maxConcurrentEnemies())
        return;

    QRandomGenerator *rng = QRandomGenerator::global();
    const QRectF b = gameBounds();
    int enemyTier;
    const int maxEnemyTier = qMin(playerTier + 2, 6);

    if (playerTier == 1) {
        const int rand = rng->bounded(10);
        if (rand < 3) {
            enemyTier = 0;
        } else if (rand < 6) {
            enemyTier = 1;
        } else {
            enemyTier = 2;
        }
    } else if (playerTier == 3) {
        const int rand = rng->bounded(10);
        if (rand < 3) {
            enemyTier = rng->bounded(3);
        } else if (rand < 6) {
            enemyTier = 3;
        } else {
            enemyTier = 2;
        }
    } else {
        const int rand = rng->bounded(10);
        if (rand < 3) {
            enemyTier = rng->bounded(qMin(maxEnemyTier + 1, 4));
        } else {
            enemyTier = qMin(maxEnemyTier, 4) + rng->bounded(maxEnemyTier - qMin(maxEnemyTier, 4) + 1);
        }
    }
    const qreal r = 11.0 + enemyTier * 4.0 + rng->bounded(14) / 7.0;
    const int rangeY = qMax(1, int(b.height() - 2 * r));

    const bool earlyPhase = gameTimeSec_ < kPhaseEarlySec;

    if (earlyPhase) {
        const qreal speed = 52.0 + rng->bounded(48);
        const bool fromLeft = rng->bounded(2) == 0;
        const qreal y = b.top() + r + rng->bounded(rangeY);
        const qreal x = fromLeft ? (b.left() - r) : (b.right() + r);
        const QPointF vel(fromLeft ? speed : -speed, 0);
        const int hue = 18 + enemyTier * 12;
        const QColor c(255, qBound(0, 200 - hue, 255), qBound(0, 90 + enemyTier * 18, 255),
                       qBound(0, 55 + enemyTier * 6, 255));
        enemies_.push_back(
            std::make_unique<EnemyFish>(x, y, r, vel, c, enemyTier, EnemyFish::MovementMode::HorizontalCross));
        applyEnemySkinToLast();
        return;
    }

    const int edge = rng->bounded(2);
    qreal x = 0;
    qreal y = 0;
    switch (edge) {
    case 0:
        x = b.left() - r;
        y = b.top() + r + rng->bounded(qMax(1, int(b.height() - 2 * r)));
        break;
    default:
        x = b.right() + r;
        y = b.top() + r + rng->bounded(qMax(1, int(b.height() - 2 * r)));
        break;
    }

    const qreal speed = 48.0 + rng->bounded(130) + qMin(80.0, score_ * 0.35);
    const bool fromLeft = edge == 0;
    const QPointF vel(fromLeft ? speed : -speed, 0);

    const int hue = 18 + enemyTier * 12;
    const QColor c(255, qBound(0, 200 - hue, 255), qBound(0, 90 + enemyTier * 18, 255),
                   qBound(0, 55 + enemyTier * 6, 255));
    enemies_.push_back(
        std::make_unique<EnemyFish>(x, y, r, vel, c, enemyTier, EnemyFish::MovementMode::BounceWander));
    applyEnemySkinToLast();
}

void GameWidget::updateMoveDir()
{
    if (!player_)
        return;
    qreal dx = 0;
    qreal dy = 0;
    if (keyLeft_)
        dx -= 1;
    if (keyRight_)
        dx += 1;
    if (keyUp_)
        dy -= 1;
    if (keyDown_)
        dy += 1;
    player_->setMoveDir(QPointF(dx, dy));
}

void GameWidget::updateBubbles(qreal dtSec)
{
    QRandomGenerator *rng = QRandomGenerator::global();
    const QRectF b = gameBounds();
    for (Bubble &bubble : bubbles_) {
        bubble.pos.ry() -= bubble.riseSpeed * dtSec;
        bubble.pos.rx() += qSin(bubble.wobble) * 18.0 * dtSec;
        bubble.wobble += dtSec * 2.1;

        if (bubble.pos.y() + bubble.radius < b.top() - 40) {
            bubble.pos.rx() = b.left() + rng->generateDouble() * b.width();
            bubble.pos.ry() = b.bottom() + 10 + rng->generateDouble() * 40;
            bubble.riseSpeed = 22.0 + rng->generateDouble() * 55.0;
        }
        bubble.pos.rx() = qBound(b.left() - 30, bubble.pos.rx(), b.right() + 30);
    }
}

void GameWidget::drawBubbles(QPainter &painter) const
{
    painter.save();
    painter.setPen(Qt::NoPen);
    for (const Bubble &bubble : bubbles_) {
        const int alpha = static_cast<int>(40 + bubble.radius * 10);
        QColor c(255, 255, 255, qBound(18, alpha, 110));
        painter.setBrush(c);
        painter.drawEllipse(bubble.pos, bubble.radius, bubble.radius);
    }
    painter.restore();
}

void GameWidget::drawEnergyBar(QPainter &p) const
{
    constexpr int barW = 200;
    constexpr int barH = 12;
    const int x0 = width() - barW - 18;
    const int y0 = 18;

    p.save();
    QFont f(QStringLiteral("Microsoft YaHei UI"), 10);
    p.setFont(f);
    p.setPen(QColor(235, 248, 255));
    p.drawText(x0, y0 - 4, QStringLiteral("冲刺能量 (按住空格)"));

    QRectF frame(x0, static_cast<qreal>(y0), static_cast<qreal>(barW), static_cast<qreal>(barH));
    p.setPen(QPen(QColor(255, 255, 255, 160), 1));
    p.setBrush(QColor(0, 30, 55, 140));
    p.drawRoundedRect(frame, 4, 4);

    const qreal fillW = qMax(0.0, barW * boostEnergy_ - 2);
    QRectF fill(x0 + 1, y0 + 1.0, fillW, barH - 2.0);
    QLinearGradient g(fill.topLeft(), fill.bottomLeft());
    g.setColorAt(0, QColor(120, 220, 255));
    g.setColorAt(1, QColor(40, 140, 255));
    p.setPen(Qt::NoPen);
    p.setBrush(g);
    p.drawRoundedRect(fill, 3, 3);
    p.restore();
}

void GameWidget::onTick()
{
    if (bgmPlayer_ && bgmPlayer_->mediaStatus() == QMediaPlayer::EndOfMedia) {
        bgmPlayer_->setPosition(0);
        bgmPlayer_->play();
    }
    
    const qint64 now = clock_.nsecsElapsed();
    const qreal dt = (now - lastNs_) / 1e9;
    lastNs_ = now;
    if (dt <= 0 || dt > 0.25)
        return;

    const QRectF bounds = gameBounds();

    updateBubbles(dt);

    if (!gameOver_ && !gameWon_ && !paused_) {
        gameTimeSec_ += dt;

        const bool wantBoost = keyBoost_;
        const bool boosting = wantBoost && boostEnergy_ > 0;
        player_->setBoostActive(boosting);
        if (boosting)
            boostEnergy_ -= dt / kBoostDrainFullSec;
        else if (!wantBoost && boostEnergy_ < 1.0)
            boostEnergy_ += dt / kBoostRechargeFullSec;
        boostEnergy_ = qBound(0.0, boostEnergy_, 1.0);

        const QPointF playerPos = player_->position();
        const int playerTier = player_->tier();

        spawnAccumulator_ += dt;
        while (spawnAccumulator_ >= spawnIntervalSec_) {
            spawnAccumulator_ -= spawnIntervalSec_;
            spawnEnemy(playerTier);
        }
        spawnIntervalSec_ = qMax(0.28, 1.08 - score_ * 0.026);

        if (finalPhase_) {
            finalSpawnAccumulator_ += dt;
            if (finalSpawnAccumulator_ < 2.0) {
                const qreal finalSpawnInterval = 0.3;
                qreal spawnDt = finalSpawnAccumulator_;
                while (spawnDt >= finalSpawnInterval) {
                    spawnDt -= finalSpawnInterval;
                    spawnEnemy(playerTier);
                }
            }
        }

        player_->updateFish(dt, bounds, playerPos, playerTier);

        for (auto &e : enemies_)
            e->updateFish(dt, bounds, playerPos, playerTier);

        for (size_t i = 0; i < enemies_.size();) {
            if (enemies_[i]->isOutsideAfterCross(bounds)) {
                enemies_.erase(enemies_.begin() + static_cast<qsizetype>(i));
                continue;
            }
            ++i;
        }

        for (size_t i = 0; i < enemies_.size();) {
            EnemyFish *e = enemies_[i].get();
            if (!player_->collidesWith(*e)) {
                ++i;
                continue;
            }

            const int pt = player_->tier();
            const int et = e->tier();

            if (pt >= et) {
                score_ += scoreForEnemyTier(et);
                tierScore_ += scoreForEnemyTier(et);
                if (pt < kPlayerWinTier) {
                    const int requiredScore = scoreForPlayerTier(pt);
                    if (tierScore_ >= requiredScore) {
                        const int newTier = pt + 1;
                        tierScore_ = 0;
                        player_->setTier(newTier);
                        player_->setRadius(playerRadiusForTier(newTier));
                        applyPlayerSkin();
                        if (newTier >= kPlayerWinTier) {
                            finalPhase_ = true;
                            finalSpawnAccumulator_ = 0;
                            player_->setBoostActive(false);
                        }
                    }
                }
                enemies_.erase(enemies_.begin() + static_cast<qsizetype>(i));
                if (finalPhase_ && enemies_.empty() && finalSpawnAccumulator_ >= 5.0) {
                    gameWon_ = true;
                    highScore_ = qMax(highScore_, score_);
                    if (!winSoundPlayed_) {
                        bgmPlayer_->stop();
                        winSoundPlayer_->play();
                        winSoundPlayed_ = true;
                    }
                }
                continue;
            }

            gameOver_ = true;
            highScore_ = qMax(highScore_, score_);
            player_->setBoostActive(false);
            if (!loseSoundPlayed_) {
                bgmPlayer_->stop();
                loseSoundPlayer_->play();
                loseSoundPlayed_ = true;
            }
            break;
        }
    } else {
        player_->setBoostActive(false);
    }

    update();
}

void GameWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    drawBackground(p);

    drawBubbles(p);

    const QRectF bounds = gameBounds();
    p.setPen(QPen(QColor(190, 235, 255, 100), 2));
    p.setBrush(Qt::NoBrush);
    p.drawRect(bounds.toRect());

    for (const auto &e : enemies_)
        e->draw(p);
    if (player_)
        player_->draw(p);

    drawEnergyBar(p);

    p.setPen(Qt::white);
    QFont uiFont(QStringLiteral("Microsoft YaHei UI"), 11);
    p.setFont(uiFont);
    p.drawText(16, 26, QStringLiteral("得分: %1").arg(score_));
    p.drawText(16, 46, QStringLiteral("最高: %1").arg(highScore_));
    if (player_)
        p.drawText(16, 66, QStringLiteral("等级: %1 / %2  |  规则: 高等级吃低等级")
                                  .arg(player_->tier())
                                  .arg(kPlayerWinTier));
    p.drawText(16, 86, QStringLiteral("时间: %1 秒  |  敌鱼上限: %2")
                              .arg(static_cast<int>(gameTimeSec_))
                              .arg(maxConcurrentEnemies()));

    p.drawText(16, height() - 36, QStringLiteral("移动: 方向键 / WASD   冲刺: 按住空格"));
    p.drawText(16, height() - 18, QStringLiteral("暂停: P   重新开始: R"));

    if (paused_ && !gameOver_ && !gameWon_) {
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0, 20, 40, 120));
        p.drawRect(rect());
        p.setPen(QColor(240, 248, 255));
        QFont hint(QStringLiteral("Microsoft YaHei UI"), 20, QFont::Bold);
        p.setFont(hint);
        p.drawText(rect(), Qt::AlignCenter, QStringLiteral("已暂停\n按 P 继续"));
    }

    if (gameWon_) {
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0, 45, 30, 160));
        p.drawRect(rect());
        p.setPen(QColor(200, 255, 220));
        QFont big(QStringLiteral("Microsoft YaHei UI"), 22, QFont::Bold);
        p.setFont(big);
        p.drawText(rect(), Qt::AlignCenter,
                   QStringLiteral("嗝~派大星吃完所有憨八嘎！\n\n本局得分: %1\n最高纪录: %2\n\n按 R 重新开始")
                       .arg(score_)
                       .arg(highScore_));
    } else if (gameOver_) {
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0, 0, 0, 145));
        p.drawRect(rect());
        p.setPen(Qt::white);
        QFont big(QStringLiteral("Microsoft YaHei UI"), 22, QFont::Bold);
        p.setFont(big);
        p.drawText(rect(), Qt::AlignCenter,
                   QStringLiteral("游戏结束\n\n本局: %1\n最高: %2\n\n按 R 重新开始")
                       .arg(score_)
                       .arg(highScore_));
    }
}

void GameWidget::keyPressEvent(QKeyEvent *event)
{
    if (event->isAutoRepeat())
        return;

    switch (event->key()) {
    case Qt::Key_Left:
    case Qt::Key_A:
        keyLeft_ = true;
        break;
    case Qt::Key_Right:
    case Qt::Key_D:
        keyRight_ = true;
        break;
    case Qt::Key_Up:
    case Qt::Key_W:
        keyUp_ = true;
        break;
    case Qt::Key_Down:
    case Qt::Key_S:
        keyDown_ = true;
        break;
    case Qt::Key_Space:
        if (!gameOver_ && !gameWon_)
            keyBoost_ = true;
        update();
        return;
    case Qt::Key_P:
        if (!gameOver_ && !gameWon_)
            paused_ = !paused_;
        update();
        return;
    case Qt::Key_R:
        resetGame();
        lastNs_ = clock_.nsecsElapsed();
        break;
    default:
        QWidget::keyPressEvent(event);
        return;
    }
    updateMoveDir();
}

void GameWidget::keyReleaseEvent(QKeyEvent *event)
{
    if (event->isAutoRepeat())
        return;

    switch (event->key()) {
    case Qt::Key_Left:
    case Qt::Key_A:
        keyLeft_ = false;
        break;
    case Qt::Key_Right:
    case Qt::Key_D:
        keyRight_ = false;
        break;
    case Qt::Key_Up:
    case Qt::Key_W:
        keyUp_ = false;
        break;
    case Qt::Key_Down:
    case Qt::Key_S:
        keyDown_ = false;
        break;
    case Qt::Key_Space:
        keyBoost_ = false;
        update();
        return;
    default:
        QWidget::keyReleaseEvent(event);
        return;
    }
    updateMoveDir();
}

void GameWidget::resizeEvent(QResizeEvent *event)
{
    Q_UNUSED(event);
    seedBubbles();
    if (player_) {
        const QRectF b = gameBounds();
        QPointF ppos = player_->position();
        ppos.setX(qBound(b.left() + player_->radius(), ppos.x(), b.right() - player_->radius()));
        ppos.setY(qBound(b.top() + player_->radius(), ppos.y(), b.bottom() - player_->radius()));
        player_->setPosition(ppos);
    }
}
