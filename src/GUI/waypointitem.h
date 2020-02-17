#ifndef WAYPOINTITEM_H
#define WAYPOINTITEM_H

#include <cmath>
#include <QGraphicsItem>
#include <QFont>
#include "data/waypoint.h"
#include "map/map.h"
#include "units.h"
#include "graphicsscene.h"
#include "format.h"


class WaypointItem : public GraphicsItem
{
public:
	WaypointItem(const Waypoint &waypoint, Map *map, QGraphicsItem *parent = 0);

	const Waypoint &waypoint() const {return _waypoint;}

	void setMap(Map *map) {setPos(map->ll2xy(_waypoint.coordinates()));}
	void setSize(int size);
	void setColor(const QColor &color);
	void showLabel(bool show);
	void setDigitalZoom(int zoom) {setScale(pow(2, -zoom));}
	void setToolTipFormat(Units units, CoordinatesFormat format);

	QPainterPath shape() const {return _shape;}
	QRectF boundingRect() const {return _shape.boundingRect();}
	void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
	  QWidget *widget);

	QString info() const;

protected:
	void hoverEnterEvent(QGraphicsSceneHoverEvent *event);
	void hoverLeaveEvent(QGraphicsSceneHoverEvent *event);
	void mousePressEvent(QGraphicsSceneMouseEvent *event);

private:
	void updateCache();

	Waypoint _waypoint;
	QPainterPath _shape;
	QColor _color;
	int _size;
	bool _showLabel;
	QFont _font;
	QRect _labelBB;

	Units _units;
	CoordinatesFormat _format;
};

#endif // WAYPOINTITEM_H
