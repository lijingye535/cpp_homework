#ifndef GAMEWIDGET_H
#define GAMEWIDGET_H

#include "enemyfish.h"
#include "playerfish.h"

#include <QAudioOutput>
#include <QElapsedTimer>
#include <QKeyEvent>
#include <QMediaPlayer>
#include <QPainter>
#include <QPixmap>
#include <QTimer>
#include <QWidget>

#include <memory>
#include <vector>

// 游戏主界面：定时刷新、生成敌鱼、碰撞与胜负（未使用 Q_OBJECT，便于 VS 下无需单独运行 moc）
class GameWidget : public QWidget {
public:
    explicit GameWidget(QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    struct Bubble {
        QPointF pos;
        qreal radius = 4;
        qreal riseSpeed = 28;
        qreal wobble = 0;
    };

    void onTick();
    void resetGame();
    void spawnEnemy(int playerTier);
    void updateMoveDir();
    void updateBubbles(qreal dtSec);
    void drawBubbles(QPainter &painter) const;
    void drawEnergyBar(QPainter &painter) const;
    void drawBackground(QPainter &painter) const;
    void reloadSkins();
    void applyPlayerSkin();
    void applyEnemySkinToLast();
    void seedBubbles();
    QRectF gameBounds() const;
    int maxConcurrentEnemies() const;

    QTimer timer_;
    QElapsedTimer clock_;
    qint64 lastNs_ = 0;

    std::unique_ptr<PlayerFish> player_;
    std::vector<std::unique_ptr<EnemyFish>> enemies_;

    bool keyLeft_ = false;
    bool keyRight_ = false;
    bool keyUp_ = false;
    bool keyDown_ = false;
    bool keyBoost_ = false;

    qreal gameTimeSec_ = 0;

    qreal boostEnergy_ = 1.0;

    int score_ = 0;
    int highScore_ = 0;
    int tierScore_ = 0;
    bool gameOver_ = false;
    bool gameWon_ = false;
    bool finalPhase_ = false;
    qreal finalSpawnAccumulator_ = 0;
    bool paused_ = false;

    std::vector<Bubble> bubbles_;

    qreal spawnAccumulator_ = 0;
    qreal spawnIntervalSec_ = 1.2;

    QPixmap bgPixmap_;
    QPixmap playerSkins_[6];
    QPixmap enemySkin_;

    QMediaPlayer *bgmPlayer_;
    QAudioOutput *audioOutput_;
    QMediaPlayer *winSoundPlayer_;
    QAudioOutput *winAudioOutput_;
    QMediaPlayer *loseSoundPlayer_;
    QAudioOutput *loseAudioOutput_;
    bool winSoundPlayed_ = false;
    bool loseSoundPlayed_ = false;
};

#endif
