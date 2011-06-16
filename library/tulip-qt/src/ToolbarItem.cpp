#include "tulip/ToolbarItem.h"

#include <QtGui/QPainter>
#include <QtGui/QAction>
#include <QtGui/QApplication>
#include <QtGui/QPalette>
#include <QtGui/QGraphicsScene>
#include <QtCore/QParallelAnimationGroup>
#include <QtCore/QSequentialAnimationGroup>
#include <QtCore/QTimer>
#include <QtSvg/QGraphicsSvgItem>
#include <assert.h>

ToolbarItem::ToolbarItem(QGraphicsItem *parent,QGraphicsScene *scene)
  : QGraphicsItemGroup(parent,scene),
  _activeAction(0), _activeButton(0), _expanded(false), _currentExpandAnimation(0), _collapseTimeout(0),
  _snapArea(Qt::TopToolBarArea), _allowedSnapAreas(Qt::AllToolBarAreas),
  _iconSize(32,32), _spacing(8), _orientation(Qt::Horizontal), _backgroundRectangleRound(4),
  _animationMsec(100), _animationEasing(QEasingCurve::Linear) {

  setFlags(QGraphicsItem::ItemIsMovable | QGraphicsItem::ItemSendsGeometryChanges);
  setCacheMode(QGraphicsItem::ItemCoordinateCache);
  setHandlesChildEvents(false);
  setAcceptHoverEvents(true);

  _toolbarBorder = QApplication::palette().color(QPalette::Shadow);

  _toolbarBackground = QApplication::palette().color(QPalette::Window);
  _toolbarBackground.setAlpha(200);
}
//==========================
ToolbarItem::~ToolbarItem() {
  if (_currentExpandAnimation)
    _currentExpandAnimation->stop();
}
//==========================
void ToolbarItem::addAction(QAction *action) {
  if (_actions.contains(action))
    return;
  _actions.push_back(action);
  PushButtonItem *btn = buildButton(action);
  _actionButton[action] = btn;
  if (!_expanded)
    btn->setVisible(false);

  if (!_activeAction)
    setActiveAction(action);
}
//==========================
void ToolbarItem::removeAction(QAction *action) {
  if (!_actions.contains(action))
    return;
  _actions.remove(_actions.indexOf(action));

  PushButtonItem *btn = _actionButton[action];
  delete btn;
  _actionButton.remove(action);

  if (action == _activeAction) {
    if (_actions.size() > 0)
      setActiveAction(_actions[0]);
    else
      setActiveAction(0);
  }
}
//==========================
void ToolbarItem::setActiveAction(QAction *action) {
  _activeAction = action;
  if (_activeAction) {
    if (_activeButton)
      _activeButton->setAction(action);
    else
      _activeButton = buildButton(_activeAction);

    _activeButton->setFadeout(false);
  }
  else {
    delete _activeButton;
    _activeButton=0;
  }
}
//==========================
PushButtonItem *ToolbarItem::buildButton(QAction *action) {
  PushButtonItem *result = new PushButtonItem(action,_iconSize,this);
  result->setAnimationBehavior(AnimatedGraphicsObject::ContinuePreviousAnimation);
  result->setPos(_spacing,_spacing);

  result->setBorderColor(Qt::transparent);
  result->setBackgroundColor(Qt::transparent);
  connect(result,SIGNAL(clicked()),this,SLOT(buttonClicked()),Qt::DirectConnection);
  return result;
}
//==========================
void ToolbarItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
  QPointF pos(_spacing/2,_spacing/2);
  QRectF brect(boundingRect());
  QSizeF bsize = buttonSize();
  QPointF tVect = translationVector();

  painter->setBrush(_toolbarBackground);
  painter->setPen(_toolbarBorder);

  if (_snapArea == Qt::TopToolBarArea) {
    painter->drawRoundedRect(0,-_backgroundRectangleRound,brect.width(),brect.height()+_backgroundRectangleRound,_backgroundRectangleRound,_backgroundRectangleRound);
    pos.setX(pos.x() + _backgroundRectangleRound);
  } else if (_snapArea == Qt::BottomToolBarArea) {
    painter->drawRoundedRect(0,0,brect.width(),brect.height()+_backgroundRectangleRound,_backgroundRectangleRound,_backgroundRectangleRound);
    pos += QPointF(_backgroundRectangleRound,_backgroundRectangleRound);
  } else if (_snapArea == Qt::LeftToolBarArea) {
    painter->drawRoundedRect(-_backgroundRectangleRound,0,brect.width()+_backgroundRectangleRound,brect.height(),_backgroundRectangleRound,_backgroundRectangleRound);
    pos.setY(pos.y() + _backgroundRectangleRound);
  } else if (_snapArea == Qt::RightToolBarArea) {
    painter->drawRoundedRect(0,0,brect.width()+_backgroundRectangleRound,brect.height(),_backgroundRectangleRound,_backgroundRectangleRound);
    pos += QPointF(_backgroundRectangleRound,_backgroundRectangleRound);
  }

  if (_activeButton) {
    _activeButton->setPos(pos);
    pos+=QPointF(tVect.x() * (_spacing + bsize.width()),tVect.y() * (_spacing + bsize.height()));
  }

  if (!_expanded)
    return;

  { // separator
    QColor col = _toolbarBorder;
    col.setAlpha(200);
    QPointF p1(pos),p2(pos.x() + tVect.y()*bsize.width(), pos.y() + tVect.x()*bsize.height());
    QLinearGradient grad(p1,p2);
    grad.setColorAt(0,Qt::transparent);
    grad.setColorAt(0.3,col);
    grad.setColorAt(0.7,col);
    grad.setColorAt(1,Qt::transparent);
    QPen pen;
    pen.setBrush(grad);
    painter->setPen(pen);
    painter->drawLine(p1,p2);
  }
  pos+=QPointF(tVect.x()*_spacing,tVect.y()*_spacing);

  for (int i=0; i < _actions.size(); ++i) {
    PushButtonItem *btn = _actionButton[_actions[i]];
    modifyButton(btn,pos);
    pos+=QPointF(tVect.x() * (_spacing + bsize.width()),tVect.y() * (_spacing + bsize.height()));
  }

  for (QMap<QAction *,PushButtonItem *>::iterator it = _actionButton.begin();it != _actionButton.end();++it)
    it.value()->setAnimated(true);
}
//==========================
void ToolbarItem::modifyButton(PushButtonItem *btn, const QPointF &newPos) const {
  btn->moveItem(newPos,_animationMsec,_animationEasing);
}
//==========================
QRectF ToolbarItem::boundingRect() const {
  QSizeF result(_spacing,_spacing);
  if (_snapArea == Qt::TopToolBarArea || _snapArea == Qt::BottomToolBarArea)
    result+=QSizeF(_backgroundRectangleRound*2,_backgroundRectangleRound);
  else if (_snapArea == Qt::LeftToolBarArea || Qt::RightToolBarArea)
    result+=QSizeF(_backgroundRectangleRound,_backgroundRectangleRound*2);

  QSizeF bsize = buttonSize();
  result+=bsize;
  QPointF tVect = translationVector();

  if (_expanded)
    result += QSizeF(tVect.x() * (_actions.size() * (_spacing + bsize.width()) + _spacing),
                     tVect.y() * (_actions.size() * (_spacing + bsize.height()) + _spacing));

  return QRectF(QPointF(0,0),result);
}
//==========================
QPointF ToolbarItem::translationVector() const {
  switch (_orientation) {
  case Qt::Horizontal:
    return QPointF(1,0);
    break;
  case Qt::Vertical:
    return QPointF(0,1);
    break;
  }
}
//==========================
QSizeF ToolbarItem::buttonSize() const {
  if (!_activeButton)
    return QSizeF();
  return _activeButton->boundingRect().size();
}
//==========================
void ToolbarItem::buttonClicked() {
  PushButtonItem *btn = static_cast<PushButtonItem *>(sender());
  if (btn != _activeButton)
    setActiveAction(btn->action());
  if (btn == _activeButton)
    emit activeButtonClicked();
  else
    emit buttonClicked(btn);
}
//==========================
void ToolbarItem::hoverEnterEvent(QGraphicsSceneHoverEvent *event) {
  if (_collapseTimeout) {
    delete _collapseTimeout;
    _collapseTimeout = 0;
  }
  for (int i=0; i < _actions.size(); ++i)
    _actionButton[_actions[i]]->setAnimationBehavior(AnimatedGraphicsObject::ContinuePreviousAnimation);

  setExpanded(true);
}
//==========================
void ToolbarItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *event) {
  if (_collapseTimeout) {
    delete _collapseTimeout;
    _collapseTimeout = 0;
  }
  _collapseTimeout = new QTimer(this);
  _collapseTimeout->setSingleShot(true);
  _collapseTimeout->setInterval(2000);
  connect(_collapseTimeout,SIGNAL(timeout()),this,SLOT(collapse()));
  _collapseTimeout->start();
  for (int i=0; i < _actions.size(); ++i)
    _actionButton[_actions[i]]->setAnimationBehavior(AnimatedGraphicsObject::StopPreviousAnimation);
  update();
}
//==========================
void ToolbarItem::setExpanded(bool f) {
  if (_currentExpandAnimation)
    _currentExpandAnimation->stop();

  if (_expanded == f)
    return;

  _expanded = f;
  emit expanded(f);

  _currentExpandAnimation = new QParallelAnimationGroup();
  for (QMap<QAction *,PushButtonItem *>::iterator it = _actionButton.begin(); it != _actionButton.end(); ++it) {
    PushButtonItem *btn = it.value();
    if (_expanded)
      btn->setVisible(true);

    btn->setAcceptHoverEvents(false);

    if (!f) {
      QPropertyAnimation *posAnim = new QPropertyAnimation(btn,"pos");
      posAnim->setStartValue(btn->pos());
      posAnim->setEndValue(QPointF(_spacing,_spacing));
      posAnim->setEasingCurve(_animationEasing);
      posAnim->setDuration(_animationMsec);
      _currentExpandAnimation->addAnimation(posAnim);
    }

    QPropertyAnimation *fadeAnim = new QPropertyAnimation(btn,"opacity");
    fadeAnim->setStartValue(btn->opacity());
    fadeAnim->setEndValue((f ? 1 : 0));
    fadeAnim->setEasingCurve(_animationEasing);
    fadeAnim->setDuration(_animationMsec);
    _currentExpandAnimation->addAnimation(fadeAnim);
  }

  connect(_currentExpandAnimation, SIGNAL(finished()), this, SLOT(expandAnimationFinished()));
  _currentExpandAnimation->start(QAbstractAnimation::DeleteWhenStopped);
}
//==========================
void ToolbarItem::expandAnimationFinished() {
  _currentExpandAnimation = 0;
  for (QMap<QAction *,PushButtonItem *>::iterator it = _actionButton.begin(); it != _actionButton.end(); ++it) {
    it.value()->setVisible(_expanded);
    it.value()->setAcceptHoverEvents(true);
  }
}
//==========================
void ToolbarItem::collapse() {
  if (_collapseTimeout) {
    delete _collapseTimeout;
    _collapseTimeout = 0;
  }
  setExpanded(false);
}
//==========================
void ToolbarItem::setOrientation(Qt::Orientation o) {
  if (o == _orientation)
    return;
  _orientation = o;
  for (QMap<QAction *,PushButtonItem *>::iterator it = _actionButton.begin();it != _actionButton.end();++it)
    it.value()->setAnimated(false);
  update();
}
//==========================
QVariant ToolbarItem::itemChange(GraphicsItemChange change, const QVariant &value) {
  if (change == ItemSceneChange) {
    disconnect(scene(),SIGNAL(sceneRectChanged(QRectF)),this,SLOT(sceneResized()));
    return value;
  }

  if (change == ItemSceneHasChanged) {
    connect(scene(),SIGNAL(sceneRectChanged(QRectF)),this,SLOT(sceneResized()));
    return value;
  }

  if (change != ItemPositionChange || !scene())
    return value;

  QRectF brect = boundingRect();
  qreal bMax = std::max<qreal>(brect.width(),brect.height());
  QSizeF bsize(bMax,bMax);

  // Update toolbar orientation and position
  QPointF result = value.toPointF();
  if (result.y() <= 0)// top
    result = setArea(Qt::TopToolBarArea,result);
  else if (result.x() <= 0)// left
    result = setArea(Qt::LeftToolBarArea,result);
  else if (result.y()+bsize.height() >= scene()->height()) // bottom
    result = setArea(Qt::BottomToolBarArea,result);
  else if (result.x()+bsize.width() >= scene()->width()) // right
    result = setArea(Qt::RightToolBarArea,result);
  else if (_orientation == Qt::Vertical)
    result.setX(pos().x());
  else
    result.setY(pos().y());

  // Compute new bounding size
  bsize = boundingRect().size();

  // Check that we are in the scene's rect
  result.setX(std::max<qreal>(0,result.x()));
  result.setY(std::max<qreal>(0,result.y()));
  result.setX(std::min<qreal>(scene()->width()-bsize.width(),result.x()));
  result.setY(std::min<qreal>(scene()->height()-bsize.height(),result.y()));
  return result;
}
//==========================
QPointF ToolbarItem::setArea(Qt::ToolBarArea area, const QPointF &p) {
  QPointF result = p;
  if (_allowedSnapAreas.testFlag(area))
    _snapArea = area;

  if (_snapArea == Qt::BottomToolBarArea) {
    setOrientation(Qt::Horizontal);
    result.setY(scene()->height()-boundingRect().height());
  }
  else if (_snapArea == Qt::LeftToolBarArea) {
    setOrientation(Qt::Vertical);
    result.setX(0);
  }
  else if (_snapArea == Qt::RightToolBarArea) {
    setOrientation(Qt::Vertical);
    result.setX(scene()->width()-boundingRect().width());
  }
  else if (_snapArea == Qt::TopToolBarArea){
    setOrientation(Qt::Horizontal);
    result.setY(0);
  }
  return result;
}
//==========================
void ToolbarItem::sceneResized() {
  QPointF newPos(pos());
  if (_snapArea == Qt::TopToolBarArea)
    newPos.setY(0);
  else if (_snapArea == Qt::LeftToolBarArea)
    newPos.setX(0);
  else if (_snapArea == Qt::RightToolBarArea)
    newPos.setX(scene()->width()-boundingRect().width());
  else
    newPos.setY(scene()->height()-boundingRect().height());

  setPos(newPos);
}
