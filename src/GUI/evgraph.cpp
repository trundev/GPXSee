#include <QApplication>
#include <QLocale>
#include <QSettings>
#include "data/data.h"
#include "settings.h"
#include "common/evdata.h"
#include "evgraphitem.h"
#include "evgraph.h"


// Find action by ID, intended to lookup in the EVGraph::_evShowActions
static QAction* findAction(QList<QAction*> actions, int data) {
	for (int i = 0; i < actions.size(); i++) {
		if (actions[i]->data() == data) {
			return actions[i];
		}
	}
	return NULL;
}

EVGraph::EVGraph(QWidget *parent) : GraphTab(parent)
{
	_showTracks = true;

	setYLabel(tr("Various"));

	setSliderPrecision(1);

#ifndef QT_NO_CONTEXTMENU
	QSettings settings(qApp->applicationName(), qApp->applicationName());
	settings.beginGroup(EVDATA_SETTINGS_GROUP);
	for (int id = 0; id < EVData::t_scalar_num; id++) {
		EVData::scalar_t scalarId = (EVData::scalar_t)id;

		QAction *a = new QAction(tr("Show ") + tr(EVData::getUserName(scalarId)), this);
		a->setMenuRole(QAction::NoRole);
		a->setCheckable(true);
		a->setData(id);

		bool checked = true;
		// Initially remove some of the parameters that are too big (fix incorrect scalling)....
		switch ((EVData::scalar_t)id)
		{
		case EVData::t_distance: case EVData::t_totaldistance:
			checked = false;
			break;
		}

		checked = settings.value(QString(EVDATA_SHOW_PREFIX_SETTINGS) + EVData::getInternalName(scalarId), checked).toBool();
		a->setChecked(checked);

		connect(a, SIGNAL(triggered(bool)), this,
		  SLOT(showEVData(bool)));

		_evShowActions.append(a);
		_contextMenu.addAction(a);
	}
	settings.endGroup();
#endif // QT_NO_CONTEXTMENU
}

EVGraph::~EVGraph()
{
#ifndef QT_NO_CONTEXTMENU
	QSettings settings(qApp->applicationName(), qApp->applicationName());
	settings.beginGroup(EVDATA_SETTINGS_GROUP);
	for (int id = 0; id < EVData::t_scalar_num; id++) {
		EVData::scalar_t scalarId = (EVData::scalar_t)id;

		QAction *a = findAction(_evShowActions, id);
		if (a) {
			bool checked = a->isChecked();
			settings.setValue(QString(EVDATA_SHOW_PREFIX_SETTINGS) + EVData::getInternalName(scalarId), checked);
		}
	}
	settings.endGroup();
	_evShowActions.clear();
#endif // QT_NO_CONTEXTMENU
}

void EVGraph::setInfo()
{
	if (_showTracks) {
		QLocale l(QLocale::system());

		GraphTab::addInfo(tr("Graph number"), l.toString(_graphs.size()));
	} else
		clearInfo();
}

QList<GraphItem*> EVGraph::loadData(const Data &data)
{
	// Remove all graphs from previous file.
	// Do not visualize multiple files, to avoid confusion with the multiple
	// graphs from the same file.
	clear();

#ifndef QT_NO_CONTEXTMENU
	for (int i = 0; i < _evShowActions.size(); i++) {
		_evShowActions[i]->setDisabled(true);
	}
#endif // QT_NO_CONTEXTMENU

	QList<GraphItem*> graphs;
	for (int i = 0; i < data.tracks().count(); i++) {
		const Track &track = data.tracks().at(i);

		for (int id = 0; id < EVData::t_scalar_num; id++) {
			EVData::scalar_t scalarId = (EVData::scalar_t)id;

			const Graph &graph = track.evScalar(scalarId);
			if (graph.isValid()) {
				EVGraphItem *gi = new EVGraphItem(graph, _graphType);
				gi->setScalarId(scalarId);

				GraphTab::addGraph(gi, id);
				graphs.append(gi);

#ifndef QT_NO_CONTEXTMENU
				QAction *a = findAction(_evShowActions, id);
				if (a) {
					a->setDisabled(false);
					showGraph(a->isChecked(), id);
				}
#endif // QT_NO_CONTEXTMENU
			}
		}
	}

	if (graphs.size() == 0) {
		skipColor();
		graphs.append(0);
	}

	setInfo();
	redraw();

	return graphs;
}

void EVGraph::clear()
{
	GraphTab::clear();
}

void EVGraph::setUnits(Units units)
{
	GraphTab::setUnits(units);
}

void EVGraph::showTracks(bool show)
{
	_showTracks = show;

	showGraph(show);
	setInfo();

	redraw();
}

#ifndef QT_NO_CONTEXTMENU
void EVGraph::contextMenuEvent(QContextMenuEvent *event)
{
	qDebug() << __FUNCTION__ << ": " << event;

	_contextMenu.exec(event->globalPos());
}
#endif // QT_NO_CONTEXTMENU

void EVGraph::showEVData(bool show)
{
	QAction* act = qobject_cast<QAction *>(sender());
	if (act == NULL)
		return;

	showGraph(show, act->data().toInt());
	redraw();
}
