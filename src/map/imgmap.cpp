#include <QFile>
#include <QPainter>
#include <QFont>
#include <QPixmapCache>
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
#include <QtCore>
#else // QT_VERSION < 5
#include <QtConcurrent>
#endif // QT_VERSION < 5
#include "common/rectc.h"
#include "common/wgs84.h"
#include "common/range.h"
#include "IMG/textpathitem.h"
#include "IMG/textpointitem.h"
#include "IMG/bitmapline.h"
#include "IMG/style.h"
#include "IMG/img.h"
#include "IMG/gmap.h"
#include "pcs.h"
#include "rectd.h"
#include "imgmap.h"


#define TILE_SIZE   384
#define TEXT_EXTENT 160

#define AREA(rect) \
	(rect.size().width() * rect.size().height())

class RasterTile
{
public:
	RasterTile() : _map(0) {}
	RasterTile(IMGMap *map, const QPoint &xy, const QString &key)
	  : _map(map), _xy(xy), _key(key),
	  _img(TILE_SIZE, TILE_SIZE, QImage::Format_ARGB32_Premultiplied) {}

	const QString &key() const {return _key;}
	const QPoint &xy() const {return _xy;}
	QImage &img() {return _img;}
	QList<MapData::Poly> &polygons() {return _polygons;}
	QList<MapData::Poly> &lines() {return _lines;}
	QList<MapData::Point> &points() {return _points;}

	void render()
	{
		QList<TextItem*> textItems;

		QRect tileRect(_xy, QSize(TILE_SIZE, TILE_SIZE));

		_map->processPoints(_points, textItems);
		_map->processPolygons(_polygons, textItems);
		_map->processLines(_lines, tileRect, textItems);

		_img.fill(Qt::transparent);

		QPainter painter(&_img);
		painter.setRenderHint(QPainter::SmoothPixmapTransform);
		painter.setRenderHint(QPainter::Antialiasing);
		painter.translate(-_xy.x(), -_xy.y());

		_map->drawPolygons(&painter, _polygons);
		_map->drawLines(&painter, _lines);
		_map->drawTextItems(&painter, textItems);
		//painter.setPen(Qt::red);
		//painter.drawRect(QRect(_xy, QSize(TILE_SIZE, TILE_SIZE)));

		qDeleteAll(textItems);
	}

private:
	IMGMap *_map;
	QPoint _xy;
	QString _key;
	QImage _img;
	QList<MapData::Poly> _polygons;
	QList<MapData::Poly> _lines;
	QList<MapData::Point> _points;
};


static const Range zooms(12, 28);

static const QColor shieldColor(Qt::white);
static const QColor shieldBgColor1("#dd3e3e");
static const QColor shieldBgColor2("#379947");
static const QColor shieldBgColor3("#4a7fc1");

static QString convertUnits(const QString &str)
{
	bool ok;
	int number = str.toInt(&ok);
	return ok ? QString::number(qRound(number * 0.3048)) : str;
}

static int minPOIZoom(Style::POIClass cl)
{
	switch (cl) {
		case Style::Food:
		case Style::Shopping:
		case Style::Services:
			return 27;
		case Style::Accommodation:
		case Style::Recreation:
			return 25;
		case Style::ManmadePlaces:
		case Style::NaturePlaces:
		case Style::Transport:
		case Style::Community:
		case Style::Elementary:
			return 23;
		default:
			return 0;
	}
}

static QFont pixelSizeFont(int pixelSize)
{
	QFont f;
	f.setPixelSize(pixelSize);
	return f;
}

static QFont *font(Style::FontSize size, Style::FontSize defaultSize
  = Style::Normal)
{
	/* The fonts must be initialized on first usage (after the QGuiApplication
	   instance is created) */
	static QFont large = pixelSizeFont(16);
	static QFont normal = pixelSizeFont(14);
	static QFont small = pixelSizeFont(12);

	switch (size) {
		case Style::None:
			return 0;
		case Style::Large:
			return &large;
		case Style::Normal:
			return &normal;
		case Style::Small:
			return &small;
		default:
			return font(defaultSize);
	}
}

static QFont *poiFont(Style::FontSize size = Style::Normal)
{
	static QFont poi = pixelSizeFont(10);

	switch (size) {
		case Style::None:
			return 0;
		default:
			return &poi;
	}
}

static const QColor *shieldBgColor(Label::Shield::Type type)
{
	switch (type) {
		case Label::Shield::USInterstate:
		case Label::Shield::Hbox:
			return &shieldBgColor1;
		case Label::Shield::USShield:
		case Label::Shield::Box:
			return &shieldBgColor2;
		case Label::Shield::USRound:
		case Label::Shield::Oval:
			return &shieldBgColor3;
		default:
			return 0;
	}
}

static int minShieldZoom(Label::Shield::Type type)
{
	switch (type) {
		case Label::Shield::USInterstate:
		case Label::Shield::Hbox:
			return 17;
		case Label::Shield::USShield:
		case Label::Shield::Box:
			return 19;
		case Label::Shield::USRound:
		case Label::Shield::Oval:
			return 20;
		default:
			return 0;
	}
}

static qreal area(const QVector<QPointF> &polygon)
{
	qreal area = 0;

	for (int i = 0; i < polygon.size(); i++) {
		int j = (i + 1) % polygon.size();
		area += polygon.at(i).x() * polygon.at(j).y();
		area -= polygon.at(i).y() * polygon.at(j).x();
	}
	area /= 2.0;

   return area;
}

static QPointF centroid(const QVector<QPointF> &polygon)
{
	qreal cx = 0, cy = 0;
	qreal factor = 1.0 / (6.0 * area(polygon));

	for (int i = 0; i < polygon.size(); i++) {
		int j = (i + 1) % polygon.size();
		qreal f = (polygon.at(i).x() * polygon.at(j).y() - polygon.at(j).x()
		  * polygon.at(i).y());
		cx += (polygon.at(i).x() + polygon.at(j).x()) * f;
		cy += (polygon.at(i).y() + polygon.at(j).y()) * f;
	}

	return QPointF(cx * factor, cy * factor);
}

static bool rectNearPolygon(const QPolygonF &polygon, const QRectF &rect)
{
	return (polygon.boundingRect().contains(rect)
	  && (polygon.containsPoint(rect.topLeft(), Qt::OddEvenFill)
	  || polygon.containsPoint(rect.topRight(), Qt::OddEvenFill)
	  || polygon.containsPoint(rect.bottomLeft(), Qt::OddEvenFill)
	  || polygon.containsPoint(rect.bottomRight(), Qt::OddEvenFill)));
}


IMGMap::IMGMap(const QString &fileName, QObject *parent)
  : Map(parent), _projection(PCS::pcs(3857)), _valid(false)
{
	if (GMAP::isGMAP(fileName))
		_data = new GMAP(fileName);
	else
		_data = new IMG(fileName);

	if (!_data->isValid()) {
		_errorString = _data->errorString();
		return;
	}

	_zoom = zooms.min();
	updateTransform();

	_valid = true;
}

void IMGMap::load()
{
	_data->load();
}

void IMGMap::unload()
{
	_data->clear();
}

QRectF IMGMap::bounds()
{
	RectD prect(_data->bounds(), _projection);
	return QRectF(_transform.proj2img(prect.topLeft()),
	  _transform.proj2img(prect.bottomRight()));
}

int IMGMap::zoomFit(const QSize &size, const RectC &rect)
{
	if (rect.isValid()) {
		RectD pr(rect, _projection, 10);

		_zoom = zooms.min();
		for (int i = zooms.min() + 1; i <= zooms.max(); i++) {
			Transform t(transform(i));
			QRectF r(t.proj2img(pr.topLeft()), t.proj2img(pr.bottomRight()));
			if (size.width() < r.width() || size.height() < r.height())
				break;
			_zoom = i;
		}
	} else
		_zoom = zooms.max();

	updateTransform();

	return _zoom;
}

int IMGMap::zoomIn()
{
	_zoom = qMin(_zoom + 1, zooms.max());
	updateTransform();
	return _zoom;
}

int IMGMap::zoomOut()
{
	_zoom = qMax(_zoom - 1, zooms.min());
	updateTransform();
	return _zoom;
}

void IMGMap::setZoom(int zoom)
{
	_zoom = zoom;
	updateTransform();
}

Transform IMGMap::transform(int zoom) const
{
	double scale = _projection.isGeographic()
	  ? 360.0 / (1<<zoom) : (2.0 * M_PI * WGS84_RADIUS) / (1<<zoom);
	PointD topLeft(_projection.ll2xy(_data->bounds().topLeft()));
	return Transform(ReferencePoint(PointD(0, 0), topLeft),
	  PointD(scale, scale));
}

void IMGMap::updateTransform()
{
	_transform = transform(_zoom);
}

QPointF IMGMap::ll2xy(const Coordinates &c)
{
	return _transform.proj2img(_projection.ll2xy(c));
}

Coordinates IMGMap::xy2ll(const QPointF &p)
{
	return _projection.xy2ll(_transform.img2proj(p));
}

void IMGMap::drawPolygons(QPainter *painter, const QList<MapData::Poly> &polygons)
{
	for (int n = 0; n < _data->style()->drawOrder().size(); n++) {
		for (int i = 0; i < polygons.size(); i++) {
			const MapData::Poly &poly = polygons.at(i);
			if (poly.type != _data->style()->drawOrder().at(n))
				continue;
			const Style::Polygon &style = _data->style()->polygon(poly.type);

			painter->setPen(style.pen());
			painter->setBrush(style.brush());
			painter->drawPolygon(poly.points);
		}
	}
}

void IMGMap::drawLines(QPainter *painter, const QList<MapData::Poly> &lines)
{
	painter->setBrush(Qt::NoBrush);

	for (int i = 0; i < lines.size(); i++) {
		const MapData::Poly &poly = lines.at(i);
		const Style::Line &style = _data->style()->line(poly.type);

		if (style.background() == Qt::NoPen)
			continue;

		painter->setPen(style.background());
		painter->drawPolyline(poly.points);
	}

	for (int i = 0; i < lines.size(); i++) {
		const MapData::Poly &poly = lines.at(i);
		const Style::Line &style = _data->style()->line(poly.type);

		if (!style.img().isNull())
			BitmapLine::draw(painter, poly.points, style.img());
		else if (style.foreground() != Qt::NoPen) {
			painter->setPen(style.foreground());
			painter->drawPolyline(poly.points);
		}
	}
}

void IMGMap::drawTextItems(QPainter *painter, const QList<TextItem*> &textItems)
{
	for (int i = 0; i < textItems.size(); i++)
		textItems.at(i)->paint(painter);
}

void IMGMap::processPolygons(QList<MapData::Poly> &polygons,
  QList<TextItem*> &textItems)
{
	for (int i = 0; i < polygons.size(); i++) {
		MapData::Poly &poly = polygons[i];
		for (int j = 0; j < poly.points.size(); j++) {
			QPointF &p = poly.points[j];
			p = ll2xy(Coordinates(p.x(), p.y()));
		}

		if (poly.label.text().isEmpty())
			continue;

		if (_zoom <= 23 && (Style::isWaterArea(poly.type)
		  || Style::isMilitaryArea(poly.type)
		  || Style::isNatureReserve(poly.type))) {
			const Style::Polygon &style = _data->style()->polygon(poly.type);
			TextPointItem *item = new TextPointItem(
			  centroid(poly.points).toPoint(), &poly.label.text(),
			  poiFont(), 0, &style.brush().color());
			if (item->isValid() && !item->collides(textItems)
			  && rectNearPolygon(poly.points, item->boundingRect()))
				textItems.append(item);
			else
				delete item;
		}
	}
}

void IMGMap::processLines(QList<MapData::Poly> &lines, const QRect &tileRect,
  QList<TextItem*> &textItems)
{
	qStableSort(lines);

	for (int i = 0; i < lines.size(); i++) {
		MapData::Poly &poly = lines[i];
		for (int j = 0; j < poly.points.size(); j++) {
			QPointF &p = poly.points[j];
			p = ll2xy(Coordinates(p.x(), p.y()));
		}
	}

	if (_zoom >= 22)
		processStreetNames(lines, tileRect, textItems);
	processShields(lines, tileRect, textItems);
}

void IMGMap::processStreetNames(QList<MapData::Poly> &lines,
  const QRect &tileRect, QList<TextItem*> &textItems)
{
	for (int i = 0; i < lines.size(); i++) {
		MapData::Poly &poly = lines[i];
		const Style::Line &style = _data->style()->line(poly.type);

		if (style.img().isNull() && style.foreground() == Qt::NoPen)
			continue;
		if (poly.label.text().isEmpty()
		  || style.textFontSize() == Style::None)
			continue;

		if (Style::isContourLine(poly.type))
			poly.label.setText(convertUnits(poly.label.text()));

		const QFont *fnt = font(style.textFontSize(), Style::Small);
		const QColor *color = style.textColor().isValid()
		  ? &style.textColor() : 0;

		TextPathItem *item = new TextPathItem(poly.points,
		  &poly.label.text(), tileRect, fnt, color);
		if (item->isValid() && !item->collides(textItems))
			textItems.append(item);
		else
			delete item;
	}
}

void IMGMap::processShields(QList<MapData::Poly> &lines, const QRect &tileRect,
  QList<TextItem*> &textItems)
{
	for (int type = FIRST_SHIELD; type <= LAST_SHIELD; type++) {
		if (minShieldZoom(static_cast<Label::Shield::Type>(type)) > _zoom)
			continue;

		QHash<Label::Shield, QPolygonF> shields;
		QHash<Label::Shield, const Label::Shield*> sp;

		for (int i = 0; i < lines.size(); i++) {
			const MapData::Poly &poly = lines.at(i);
			const Label::Shield &shield = poly.label.shield();
			if (!shield.isValid() || shield.type() != type
			  || !Style::isMajorRoad(poly.type))
				continue;

			QPolygonF &p = shields[shield];
			for (int j = 0; j < poly.points.size(); j++)
				p.append(poly.points.at(j));

			sp.insert(shield, &shield);
		}

		for (QHash<Label::Shield, QPolygonF>::const_iterator it
		  = shields.constBegin(); it != shields.constEnd(); ++it) {
			const QPolygonF &p = it.value();
			QRectF rect(p.boundingRect() & tileRect);
			if (qSqrt(AREA(rect)) < TILE_SIZE/8)
				continue;

			QMap<qreal, int> map;
			QPointF center = rect.center();
			for (int j = 0; j < p.size(); j++) {
				QLineF l(p.at(j), center);
				map.insert(l.length(), j);
			}

			QMap<qreal, int>::const_iterator jt = map.constBegin();

			TextPointItem *item = new TextPointItem(
			  p.at(jt.value()).toPoint(), &(sp.value(it.key())->text()),
			  poiFont(), 0, &shieldColor, shieldBgColor(it.key().type()));

			bool valid = false;
			while (true) {
				if (!item->collides(textItems)
				  && tileRect.contains(item->boundingRect().toRect())) {
					valid = true;
					break;
				}
				if (++jt == map.constEnd())
					break;
				item->setPos(p.at(jt.value()).toPoint());
			}

			if (valid)
				textItems.append(item);
			else
				delete item;
		}
	}
}

void IMGMap::processPoints(QList<MapData::Point> &points,
  QList<TextItem*> &textItems)
{
	qSort(points);

	for (int i = 0; i < points.size(); i++) {
		MapData::Point &point = points[i];
		const Style::Point &style = _data->style()->point(point.type);

		if (point.poi && _zoom < minPOIZoom(Style::poiClass(point.type)))
			continue;

		const QString *label = point.label.text().isEmpty()
		  ? 0 : &(point.label.text());
		const QImage *img = style.img().isNull() ? 0 : &style.img();
		const QFont *fnt = point.poi
		  ? poiFont(style.textFontSize()) : font(style.textFontSize());
		const QColor *color = style.textColor().isValid()
		  ? &style.textColor() : 0;

		if ((!label || !fnt) && !img)
			continue;

		if (Style::isSpot(point.type))
			point.label.setText(convertUnits(point.label.text()));
		if (Style::isSummit(point.type) && !point.label.text().isEmpty()) {
			QStringList list = point.label.text().split(" ");
			list.last() = convertUnits(list.last());
			point.label = list.join(" ");
		}

		TextPointItem *item = new TextPointItem(
		  ll2xy(point.coordinates).toPoint(), label, fnt, img, color);
		if (item->isValid() && !item->collides(textItems))
			textItems.append(item);
		else
			delete item;
	}
}

static void render(RasterTile &tile)
{
	tile.render();
}

void IMGMap::draw(QPainter *painter, const QRectF &rect, Flags flags)
{
	Q_UNUSED(flags);

	QPointF tl(floor(rect.left() / TILE_SIZE)
	  * TILE_SIZE, floor(rect.top() / TILE_SIZE) * TILE_SIZE);
	QSizeF s(rect.right() - tl.x(), rect.bottom() - tl.y());
	int width = ceil(s.width() / TILE_SIZE);
	int height = ceil(s.height() / TILE_SIZE);

	QList<RasterTile> tiles;

	for (int i = 0; i < width; i++) {
		for (int j = 0; j < height; j++) {
			QPixmap pm;
			QPoint ttl(tl.x() + i * TILE_SIZE, tl.y() + j * TILE_SIZE);
			QString key = _data->fileName() + "-" + QString::number(_zoom) + "_"
			  + QString::number(ttl.x()) + "_" + QString::number(ttl.y());
			if (QPixmapCache::find(key, pm))
				painter->drawPixmap(ttl, pm);
			else {
				tiles.append(RasterTile(this, ttl, key));
				RasterTile &tile = tiles.last();

				RectD polyRect(_transform.img2proj(ttl), _transform.img2proj(
				  QPointF(ttl.x() + TILE_SIZE, ttl.y() + TILE_SIZE)));
				_data->polys(polyRect.toRectC(_projection, 4), _zoom,
				  &(tile.polygons()), &(tile.lines()));

				RectD pointRect(_transform.img2proj(QPointF(ttl.x() - TEXT_EXTENT,
				  ttl.y() - TEXT_EXTENT)), _transform.img2proj(QPointF(ttl.x()
				  + TILE_SIZE + TEXT_EXTENT, ttl.y() + TILE_SIZE + TEXT_EXTENT)));
				_data->points(pointRect.toRectC(_projection, 4), _zoom,
				  &(tile.points()));
			}
		}
	}

	QFuture<void> future = QtConcurrent::map(tiles, render);
	future.waitForFinished();

	for (int i = 0; i < tiles.size(); i++) {
		RasterTile &mt = tiles[i];
		QPixmap pm(QPixmap::fromImage(mt.img()));
		if (pm.isNull())
			continue;

		QPixmapCache::insert(mt.key(), pm);

		painter->drawPixmap(mt.xy(), pm);
	}
}

void IMGMap::setProjection(const Projection &projection)
{
	_projection = projection;
	updateTransform();
	QPixmapCache::clear();
}
