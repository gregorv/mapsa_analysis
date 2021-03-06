
#include "plot.h"
#include <QApplication>

Plot::Plot(QWidget* parent, size_t id, PlotDocument* doc)
 : QCustomPlot(parent), _id(id), _doc(doc), _config(nullptr), _selectedParameters(6),
 _xSelectionLine(new QCPItemLine(this)), _ySelectionLine(new QCPItemLine(this)),
 _colorScale(new QCPColorScale(this))
{
	plotLayout()->addElement(0, 1, _colorScale);
	_colorScale->setType(QCPAxis::atRight);
	_colorScale->setVisible(false);
	setInteraction(QCP::iSelectPlottables, true);
	setInteraction(QCP::iSelectLegend, true);
	legend->setSelectableParts(QCPLegend::spItems);
	_tickerFixed = xAxis->ticker();
	connect(this, &QCustomPlot::selectionChangedByUser, this, &Plot::checkSelections);
}

Plot::~Plot()
{
}

bool Plot::setConfig(const PlotDocument::global_config_t global_config)
{
	try {
		checkCacheInvalidation(global_config);
	} catch(std::out_of_range& e) {
		return false;
	}
	_curves.clear();
	clearPlottables();
	_global = global_config;
	bool found = false;
	for(const auto& config: _global.plots) {
		if(config.id == _id) {
			_config = &config;
			found = true;
			break;
		}
	}
	if(!found) {
		return false;
	}
	setWindowTitle(_config->title);
	xAxis->setLabel(_config->xlabel);
	yAxis->setLabel(_config->ylabel);
	_colorScale->axis()->setLabel(_config->zlabel);
	QSharedPointer<QCPAxisTickerLog> logTicker(new QCPAxisTickerLog);
	if(_config->xlog) {
		xAxis->setScaleType(QCPAxis::stLogarithmic);
		xAxis->setTicker(logTicker);
	} else {
		xAxis->setScaleType(QCPAxis::stLinear);
		xAxis->setTicker(_tickerFixed);
	}
	if(_config->ylog) {
		yAxis->setScaleType(QCPAxis::stLogarithmic);
		yAxis->setTicker(logTicker);
	} else {
		yAxis->setScaleType(QCPAxis::stLinear);
		yAxis->setTicker(_tickerFixed);
	}
	_colorScale->setVisible(false);
	legend->setVisible(_config->legend);
	for(const auto& curve_cfg: _config->curves) {
		curve_t curve { nullptr, nullptr, nullptr, &curve_cfg };
		QPen pen;
		pen.setColor(curve.config->color);
		QCPScatterStyle sstyle;
		sstyle.setPen(pen);
		sstyle.setShape(curve.config->shape);
		
		QCPScatterStyle selectstyle;
		QPen selpen;
		selpen.setColor(QColor::fromRgb(255, 255, 0));
		selpen.setWidth(3);
		selectstyle.setPen(selpen);
		selectstyle.setShape(curve.config->shape);
		if(curve.config->mode == PlotDocument::cmStatisticalBox) {
			curve.statbox = new QCPStatisticalBox(xAxis, yAxis);
			curve.statbox->setOutlierStyle(sstyle);
			curve.statbox->setPen(pen);
			curve.statbox->setName(curve.config->title);
		} else if(curve.config->mode == PlotDocument::cmHistogram2D) {
			curve.colormap = new QCPColorMap(xAxis, yAxis);
			curve.colormap->setColorScale(_colorScale);
		} else {
			curve.graph = new QCPGraph(xAxis, yAxis);
			if(curve.config->mode == PlotDocument::cmHistogram) {
				curve.graph->setAntialiased(false);
				curve.graph->setLineStyle(QCPGraph::lsStepLeft);
				curve.graph->setPen(pen);
				curve.graph->setScatterStyle(QCPScatterStyle::ssNone);
			} else {
				if(curve.config->draw_lines) {
					curve.graph->setLineStyle(QCPGraph::lsLine);
					curve.graph->setPen(pen);
				} else {
					curve.graph->setLineStyle(QCPGraph::lsNone);
				}
				curve.graph->setScatterStyle(sstyle);
			}
			// curve.graph->setSelectedStyle(selectstyle);
			curve.graph->setName(curve.config->title);
		}
		_curves.push_back(curve);
	}
	refresh();
	return true;
}

void Plot::focusOutEvent(QFocusEvent* event)
{
	deselectAll();
	replot();
}

QSize Plot::minimumSizeHint() const
{
	return QSize(300, 200);
}

void Plot::mousePressEvent(QMouseEvent* event)
{
	if(event->button() == Qt::RightButton) {
		double ax = xAxis->pixelToCoord(event->x());
		double ay = yAxis->pixelToCoord(event->y());
		setParameterSelection(ax, _config->selection_x);
		emitParameterSelection(ax, _config->selection_x);
		setParameterSelection(ay, _config->selection_y);
		emitParameterSelection(ay, _config->selection_y);
	} else {
		QCustomPlot::mousePressEvent(event);
	}
}

void Plot::mouseMoveEvent(QMouseEvent* event)
{
	if(event->buttons() & Qt::RightButton) {
		double ax = xAxis->pixelToCoord(event->x());
		double ay = yAxis->pixelToCoord(event->y());
		setParameterSelection(ax, _config->selection_x);
		emitParameterSelection(ax, _config->selection_x);
		setParameterSelection(ay, _config->selection_y);
		emitParameterSelection(ay, _config->selection_y);
	} else {
		QCustomPlot::mouseMoveEvent(event);
	}
}

void Plot::refresh()
{
	// Execute query and get write data to plots
	try {
		for(const auto& curve: _curves) {
			qDebug() << "getData";
			try {
				auto data = getData(curve.config->id);
				applyGraph(data, curve);
				qDebug() << "processed";
			} catch(std::exception& e) {
				emit curveProcessed();
				QApplication::processEvents();
				throw;
			}
			emit curveProcessed();
			QApplication::processEvents();
		}
	} catch(std::runtime_error& e) {
		qDebug() << "Error:" << e.what();
	}
	// apply auto-ranges and (semi-)fixed ranges
	if(!(_config->use_xmin && _config->use_xmax)) {
		xAxis->rescale();
	}
	if(_config->use_xmin) {
		xAxis->setRangeLower(_config->xmin);
	}
	if(_config->use_xmax) {
		xAxis->setRangeUpper(_config->xmax);
	}
	if(!(_config->use_ymin && _config->use_ymax)) {
		yAxis->rescale();
	}
	if(_config->use_ymin) {
		yAxis->setRangeLower(_config->ymin);
	}
	if(_config->use_ymax) {
		yAxis->setRangeUpper(_config->ymax);
	}
	if(_colorScale->visible()) {
		// TODO: apply manual range
	}
	updateSelectionLines();
	replot();
}

void Plot::forcedRefresh()
{
	invalidateAllCaches();
	refresh();
	replot();
}

void Plot::setParameterSelectionX(double par)
{
	_selectedParameters[0] = par;
	updateSelectionLines();
	replot();
}

void Plot::setParameterSelectionY(double par)
{
	_selectedParameters[1] = par;
	updateSelectionLines();
	replot();
}

void Plot::setParameterSelectionZ(double par)
{
	_selectedParameters[2] = par;
	updateSelectionLines();
	replot();
}

void Plot::setParameterSelectionPhi(double par)
{
	_selectedParameters[3] = par;
	updateSelectionLines();
	replot();
}

void Plot::setParameterSelectionTheta(double par)
{
	_selectedParameters[4] = par;
	updateSelectionLines();
	replot();
}

void Plot::setParameterSelectionOmega(double par)
{
	_selectedParameters[5] = par;
	updateSelectionLines();
	replot();
}

void Plot::setParameterSelection(double val, PlotDocument::axis_parameter_t par)
{
	if(par == PlotDocument::apX) {
		setParameterSelectionX(val);
	} else if(par == PlotDocument::apY) {
		setParameterSelectionY(val);
	} else if(par == PlotDocument::apZ) {
		setParameterSelectionZ(val);
	} else if(par == PlotDocument::apPhi) {
		setParameterSelectionPhi(val);
	} else if(par == PlotDocument::apTheta) {
		setParameterSelectionTheta(val);
	} else if(par == PlotDocument::apOmega) {
		setParameterSelectionOmega(val);
	}
}

void Plot::emitParameterSelection(double val, PlotDocument::axis_parameter_t par)
{
	if(par == PlotDocument::apX) {
		emit selectedParameterX(val);
	} else if(par == PlotDocument::apY) {
		emit selectedParameterY(val);
	} else if(par == PlotDocument::apZ) {
		emit selectedParameterZ(val);
	} else if(par == PlotDocument::apPhi) {
		emit selectedParameterPhi(val);
	} else if(par == PlotDocument::apTheta) {
		emit selectedParameterTheta(val);
	} else if(par == PlotDocument::apOmega) {
		emit selectedParameterOmega(val);
	}
}

void Plot::checkSelections()
{
	auto num_legends = selectedLegends().size();
	auto num_plottables = selectedPlottables().size();
	_selectedCurve = nullptr;
	emit selectionChanged(false);
	if(num_legends == 0 && num_plottables == 0) {
		return;
	}
	QCPGraph* graph = nullptr;
	QCPStatisticalBox* statbox = nullptr;
	if(num_legends > 0) {
		auto legend = dynamic_cast<QCPPlottableLegendItem*>(selectedLegends()[0]->selectedItems()[0]);
		if(legend) {
			graph = dynamic_cast<QCPGraph*>(legend->plottable());
			statbox = dynamic_cast<QCPStatisticalBox*>(legend->plottable());
		}
	}
	if(num_plottables > 0) {
		graph = dynamic_cast<QCPGraph*>(selectedPlottables()[0]);
		statbox = dynamic_cast<QCPStatisticalBox*>(selectedPlottables()[0]);
	}
	for(const auto& curve: _curves) {
		if(curve.graph == graph && curve.statbox == statbox) {
			_selectedCurve = &curve;
			emit selectedCurve(curve.config->plot_id, curve.config->id);
			emit selectionChanged(true);
			return;
		}
	}
}

Database::data_t Plot::getData(size_t curveId)
{
	if(_cache.contains(curveId)) {
		return _cache[curveId];
	}
	qDebug() << "Query data for curve" << _id << ":" << curveId;
	QString jobQuery = _global.global_query;
	if(_config->job_query.size() > 0) {
		jobQuery = _config->job_query;
	}
	auto curve_cfg = PlotDocument::curveById(_global, _id, curveId);
	QString query(curve_cfg.query);
	query.replace("%jobs", jobQuery);
	_cache[curveId] = _doc->db.exec(query, curve_cfg.mode == PlotDocument::cmStatisticalBox);
	return _cache[curveId];
}

void Plot::checkCacheInvalidation(PlotDocument::global_config_t newConfig)
{
	if(_config == nullptr) {
		invalidateAllCaches();
		return;
	}
	auto plot_config = PlotDocument::plotById(newConfig, _id);
	if(&plot_config == nullptr) {
		invalidateAllCaches();
		return;
	}
	if(plot_config.job_query.size() > 0 || _config->job_query.size() > 0) {
		if(plot_config.job_query != _config->job_query) {
			invalidateAllCaches();
			return;
		}
	} else if(newConfig.global_query != _global.global_query) {
		invalidateAllCaches();
		return;
	}
	for(auto newCurve: plot_config.curves) {
		bool match = false;
		for(auto oldCurve: _config->curves) {
			if(newCurve.id != oldCurve.id) {
				continue;
			}
			if(newCurve.query != oldCurve.query || newCurve.mode != oldCurve.mode) {
				qDebug() << "Query or mode change invalidation";
				invalidateCache(newCurve.id);
			}
			match = true;
			break;
		}
		if(!match) {
			qDebug() << "No match invalidation";
			invalidateCache(newCurve.id);
		}
	}
}

void Plot::invalidateAllCaches()
{
	qDebug() << "Invalidate all caches on plot" << _id;
	_cache.clear();
}

void Plot::invalidateCache(size_t curve_id)
{
	qDebug() << "Invalidate curve" << _id << ":" << curve_id;
	_cache.remove(curve_id);
}

void Plot::updateSelectionLines()
{
	if(!_config) {
		return;
	}
	if(_config->selection_x != PlotDocument::apNone) {
		_xSelectionLine->setVisible(true);
		double x = _selectedParameters[_config->selection_x - 1];
		_xSelectionLine->start->setCoords(x, yAxis->range().lower);
		_xSelectionLine->end->setCoords(x, yAxis->range().upper);
	} else {
		_xSelectionLine->setVisible(false);
	}
	if(_config->selection_y != PlotDocument::apNone) {
		_ySelectionLine->setVisible(true);
		double y = _selectedParameters[_config->selection_y - 1];
		_ySelectionLine->start->setCoords(xAxis->range().lower, y);
		_ySelectionLine->end->setCoords(xAxis->range().upper, y);
	} else {
		_ySelectionLine->setVisible(false);
	}
}

void Plot::applyGraph(Database::data_t data, const curve_t& curve)
{
	if(curve.config->mode == PlotDocument::cmPoints) {
		curve.graph->setData(data.x, data.y);
	} else if(curve.config->mode == PlotDocument::cmHistogram) {
		plotHistogram(data, curve.config, curve.graph);
	} else if(curve.config->mode == PlotDocument::cmHistogram2D) {
		plotHistogram2D(data, curve.config, curve.colormap);
	}
}

void Plot::plotHistogram(Database::data_t data, const PlotDocument::curve_config_t* config, QCPGraph* graph)
{
	QVector<double> bin_x(config->hist_nbins_x+1);
	QVector<double> count(config->hist_nbins_x+1);
	for(size_t i = 0; i < config->hist_nbins_x+1; ++i) {
		bin_x[i] = (config->hist_high_x - config->hist_low_x) / (config->hist_nbins_x+1) *i + config->hist_low_x;
	}
	for(size_t i = 0; i < data.x.size(); ++i) {
		if(data.x[i] < bin_x[0]) {
			continue;
		}
		if(data.x[i] > bin_x[config->hist_nbins_x]) {
			continue;
		}
		for(int idx = config->hist_nbins_x; idx >= 0; --idx) {
			if(data.x[i] > bin_x[idx]) {
				count[idx] += 1;
				break;
			}
		}
	}
	qDebug() << "Histogram size x/y" << bin_x.size() << count.size();
	qDebug() << bin_x;
	qDebug() << count;
	graph->setData(bin_x, count);
}

void Plot::plotHistogram2D(Database::data_t data, const PlotDocument::curve_config_t* config, QCPColorMap* colorMap)
{
	_colorScale->setVisible(true);
	colorMap->data()->clear();
	colorMap->data()->setSize(config->hist_nbins_x+1, config->hist_nbins_y+1);
	colorMap->data()->setRange(QCPRange(config->hist_low_x, config->hist_high_x),
	                           QCPRange(config->hist_low_y, config->hist_high_y));
	assert(data.x.size() == data.y.size());
	for(size_t i = 0; i < data.x.size(); ++i) {
		const auto& x = data.x[i];
		const auto& y = data.y[i];
		if(!colorMap->data()->keyRange().contains(x) ||
		   !colorMap->data()->valueRange().contains(y)) {
			continue;
		}
		int ix, iy;
		colorMap->data()->coordToCell(x, y, &ix, &iy);
		qDebug() << x << y << ix << iy;
		colorMap->data()->setCell(ix, iy, colorMap->data()->cell(ix, iy) + 1);
	}
	colorMap->setGradient(config->gradient);
	colorMap->rescaleDataRange(false);
}
