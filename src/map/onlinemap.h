#ifndef ONLINEMAP_H
#define ONLINEMAP_H

#include "common/range.h"
#include "common/rectc.h"
#include "map.h"
#include "tileloader.h"

class OnlineMap : public Map
{
	Q_OBJECT

public:
	OnlineMap(const QString &name, const QString &url, const Range &zooms,
	  const RectC &bounds, qreal tileRatio, const Authorization &authorization,
	  int tileSize, bool scalable, bool invertY, bool quadTiles,
	  QObject *parent = 0);

	QString name() const {return _name;}

	QRectF bounds();
	qreal resolution(const QRectF &rect);

	int zoom() const {return _zoom;}
	void setZoom(int zoom) {_zoom = zoom;}
	int zoomFit(const QSize &size, const RectC &rect);
	int zoomIn();
	int zoomOut();

	QPointF ll2xy(const Coordinates &c);
	Coordinates xy2ll(const QPointF &p);

	void draw(QPainter *painter, const QRectF &rect, Flags flags);

	void setDevicePixelRatio(qreal deviceRatio, qreal mapRatio);
	void clearCache() {_tileLoader->clearCache();}

private:
	int limitZoom(int zoom) const;
	qreal tileSize() const;
	qreal coordinatesRatio() const;
	qreal imageRatio() const;

	TileLoader *_tileLoader;
	QString _name;
	Range _zooms;
	RectC _bounds;
	int _zoom;
	qreal _mapRatio, _tileRatio;
	int _tileSize;
	bool _scalable;
	bool _invertY;
};

#endif // ONLINEMAP_H
