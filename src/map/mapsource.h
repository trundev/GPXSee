#ifndef MAPSOURCE_H
#define MAPSOURCE_H

#include <QList>
#include "common/range.h"
#include "common/rectc.h"
#include "common/kv.h"
#include "downloader.h"
#include "coordinatesystem.h"

class Map;
class QXmlStreamReader;

class MapSource
{
public:
	static Map *loadMap(const QString &path, QString &errorString);
	static bool isMap(const QString &path);

private:
	enum Type {
		OSM,
		WMTS,
		WMS,
		TMS,
		QuadTiles
	};

	struct Config {
		Type type;
		QString name;
		QString url;
		Range zooms;
		RectC bounds;
		QString layer;
		QString style;
		QString set;
		QString format;
		QString crs;
		CoordinateSystem coordinateSystem;
		bool rest;
		QList<KV<QString, QString> > dimensions;
		Authorization authorization;
		qreal tileRatio;
		int tileSize;
		bool scalable;

		Config();
	};

	static RectC bounds(QXmlStreamReader &reader);
	static Range zooms(QXmlStreamReader &reader);
	static void map(QXmlStreamReader &reader, Config &config);
	static void tile(QXmlStreamReader &reader, Config &config);
};

#endif // MAPSOURCE_H
